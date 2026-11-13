/*
 Copyright (c) 2026, OpenEmu Team

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of the OpenEmu Team nor the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY OpenEmu Team ''AS IS'' AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL OpenEmu Team BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <iterator>
#include <memory>

// The emulator headers come before anything Foundation-related: Foundation defines an ABS() macro,
// which collides with the ABS() mnemonics in the arm64 assembler Oaknut uses.
#include "emulator.hpp"
#include "renderer.hpp"
#include "renderer_gl/renderer_gl.hpp"
#include "screen_layout.hpp"
#include "audio/audio_device_interface.hpp"
#include "audio/dsp_core.hpp"
#include "services/hid.hpp"
#include "services/service_manager.hpp"

#import "OEPanda3DSCore.h"

#import <OpenEmuBase/OERingBuffer.h>

#include <OpenGL/OpenGL.h>
#include <glad/gl.h>

// The core outputs both screens stacked in the console's native resolution, 400 x 480.
// ScreenLayout knows the individual screens' sizes, which is what the touch screen mapping needs.
static constexpr u32 kOutputWidth  = 400;
static constexpr u32 kOutputHeight = 480;

static constexpr double kSampleRate     = 32768.0;  // 3DS native sample rate
static constexpr NSUInteger kChannelCount = 2;      // stereo

// The circle pad's full deflection, the same value the other Panda3DS frontends send
static constexpr s16 kCirclePadRange = 0x9C;

namespace {

// OpenEmu drives audio output itself, so the emulator must not open one of its own. This device only
// tells the emulator that audio is available, which keeps the DSP filling its sample buffer; the game
// core drains that buffer after every frame.
class OpenEmuAudioDevice final : public AudioDeviceInterface {
  public:
    OpenEmuAudioDevice() : AudioDeviceInterface(nullptr, settings) {}

    void init(Samples& samples, bool safe = false) override {
        this->samples = &samples;
        running = false;
    }
    void close() override { running = false; }
    void start() override { running = true; }
    void stop() override { running = false; }
    bool isInitialized() const { return true; }

  private:
    // Volume and mute are applied by OpenEmu, so the defaults are what we want here
    static inline AudioDeviceConfig settings{};
};

// Panda3DS reaches the OpenGL entry points through glad, which wants a loader for them. macOS has no
// per-context function lookup, so the symbols are taken from the OpenGL framework directly.
void* OpenGLFunctionLoader(const char* name) {
    static void* framework = nullptr;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        framework = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_LOCAL);
    });

    return framework ? dlsym(framework, name) : nullptr;
}

// Panda3DS picks its renderer when the emulator is constructed, and it only learns about it from its
// configuration file: there is no way to hand the choice over once the emulator exists. The file is
// therefore patched before the emulator is created. Only the Renderer line is replaced, so every
// other setting in it keeps whatever value the user gave.
NSString* ConfigWithOpenGLRenderer(NSString* config) {
    static NSString *const rendererLine = @"Renderer = \"opengl\"";

    if (config == nil || config.length == 0)
        return [NSString stringWithFormat:@"[GPU]\n%@\n", rendererLine];

    NSMutableArray<NSString *> *lines = [[config componentsSeparatedByString:@"\n"] mutableCopy];
    NSInteger sectionStart = -1;  // Index of the [GPU] header
    NSInteger rendererIndex = -1;

    for (NSInteger i = 0; i < (NSInteger)lines.count; i++)
    {
        NSString *trimmed = [lines[i] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];

        if (sectionStart < 0)
        {
            if ([trimmed isEqualToString:@"[GPU]"])
                sectionStart = i;
            continue;
        }

        // The next section header ends the [GPU] block
        if ([trimmed hasPrefix:@"["])
            break;

        if ([trimmed hasPrefix:@"Renderer"] && [trimmed containsString:@"="])
            rendererIndex = i;
    }

    if (sectionStart < 0)
    {
        [lines addObject:@"[GPU]"];
        [lines addObject:rendererLine];
    }
    else if (rendererIndex >= 0)
    {
        lines[rendererIndex] = rendererLine;
    }
    else
    {
        [lines insertObject:rendererLine atIndex:sectionStart + 1];
    }

    return [lines componentsJoinedByString:@"\n"];
}

}  // namespace

@interface OEPanda3DSCore () <OEPanda3DSSystemResponderClient>
@property (nonatomic, strong) OERingBuffer *ringBuffer;
@end

@implementation OEPanda3DSCore
{
    // The audio device is passed to the emulator by reference, so it has to outlive it
    OpenEmuAudioDevice _audioDevice;
    std::unique_ptr<Emulator> _emulator;

    // The emulator renders into its own offscreen OpenGL context; the finished picture is handed to
    // OpenEmu through the bitmap path, which copies it out of _videoBuffer every frame.
    CGLContextObj _glContext;
    NSMutableData *_videoBuffer;
    BOOL _graphicsInitialized;
}

- (instancetype)init
{
    if ((self = [super init]))
    {
        // A 3DS video frame is 1/60 s = 546 stereo samples. The ring buffer holds roughly 1.5 s,
        // which absorbs the jitter between the emulation thread and OpenEmu's audio thread.
        self.ringBuffer = [[OERingBuffer alloc] initWithLength:32768 * 2 * sizeof(s16)];

        // OpenEmu asks for the video buffer as soon as the game is set up, before the first frame
        _videoBuffer = [NSMutableData dataWithLength:kOutputWidth * kOutputHeight * 4];
    }
    return self;
}

- (void)dealloc
{
    if (_emulator)
    {
        if (_graphicsInitialized)
            _emulator->deinitGraphicsContext();

        _emulator.reset();
    }

    if (_glContext)
    {
        CGLSetCurrentContext(nullptr);
        CGLDestroyContext(_glContext);
        _glContext = nullptr;
    }
}

#pragma mark - Execution

- (BOOL)loadFileAtPath:(NSString *)path error:(NSError **)error
{
    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSString *supportDirectory = self.supportDirectoryPath;
    NSString *configPath = [supportDirectory stringByAppendingPathComponent:@"config.toml"];

    if (![fileManager createDirectoryAtPath:supportDirectory withIntermediateDirectories:YES attributes:nil error:nil])
    {
        if (error)
            *error = [NSError errorWithDomain:OEGameCoreErrorDomain
                                         code:OEGameCoreCouldNotLoadROMError
                                     userInfo:@{NSLocalizedDescriptionKey: @"The core's support directory could not be created."}];
        return NO;
    }

    // The Metal backend of this Panda3DS version does not render correctly yet, so the core runs the
    // OpenGL renderer, which macOS still provides natively (Core Profile 4.1).
    NSString *config = [NSString stringWithContentsOfFile:configPath encoding:NSUTF8StringEncoding error:nil];
    NSString *openGLConfig = ConfigWithOpenGLRenderer(config);

    if (![openGLConfig isEqualToString:config])
    {
        if (![openGLConfig writeToFile:configPath atomically:YES encoding:NSUTF8StringEncoding error:error])
            return NO;
    }

    _emulator = std::make_unique<Emulator>(&_audioDevice, std::filesystem::path(configPath.fileSystemRepresentation));

    if (_emulator->getRendererType() != RendererType::OpenGL)
    {
        if (error)
            *error = [NSError errorWithDomain:OEGameCoreErrorDomain
                                         code:OEGameCoreCouldNotLoadROMError
                                     userInfo:@{NSLocalizedDescriptionKey: @"Panda3DS did not start its OpenGL renderer."}];
        _emulator.reset();
        return NO;
    }

    if (!_emulator->loadROM(path.fileSystemRepresentation))
    {
        if (error)
            *error = [NSError errorWithDomain:OEGameCoreErrorDomain
                                         code:OEGameCoreCouldNotLoadROMError
                                     userInfo:@{NSLocalizedDescriptionKey: @"Panda3DS could not load the ROM."}];
        _emulator.reset();
        return NO;
    }

    return YES;
}

- (void)stopEmulation
{
    // Pausing first keeps the emulation loop from touching the emulator again; the emulator itself
    // lives until the core is deallocated.
    if (_emulator)
        _emulator->pause();

    [super stopEmulation];
}

- (void)resetEmulation
{
    if (_emulator)
        _emulator->reset(Emulator::ReloadOption::Reload);
}

- (void)executeFrame
{
    if (!_emulator)
        return;

    [self initializeGraphicsIfNeeded];

    // The context is per thread, and OpenEmu runs the core on its own thread
    if (_glContext)
        CGLSetCurrentContext(_glContext);

    _emulator->runFrame();

    // Panda3DS hands the input state over to the emulated HID module here: the frontend announces
    // the current key, circle pad and touch state once per frame, which is what the game polls.
    HIDService& hid = _emulator->getServiceManager().getHID();
    hid.updateInputs(_emulator->getTicks());

    [self drainAudioSamples];

    // Hand the composed picture to OpenEmu; it copies the buffer out before the next frame
    if (_graphicsInitialized)
        static_cast<RendererGL*>(_emulator->getRenderer())->copyScreenToBGRA(_videoBuffer.mutableBytes);
}

- (BOOL)createOpenGLContext
{
    CGLPixelFormatAttribute attributes[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,
        kCGLPFAAccelerated,
        kCGLPFAColorSize,     (CGLPixelFormatAttribute)24,
        kCGLPFAAlphaSize,     (CGLPixelFormatAttribute)8,
        kCGLPFADepthSize,     (CGLPixelFormatAttribute)24,
        (CGLPixelFormatAttribute)0,
    };

    CGLPixelFormatObj pixelFormat = nullptr;
    GLint formatCount = 0;
    if (CGLChoosePixelFormat(attributes, &pixelFormat, &formatCount) != kCGLNoError || pixelFormat == nullptr)
        return NO;

    // An offscreen context draws into framebuffers of its own, so it needs no window or drawable
    CGLError error = CGLCreateContext(pixelFormat, nullptr, &_glContext);
    CGLDestroyPixelFormat(pixelFormat);

    if (error != kCGLNoError)
    {
        _glContext = nullptr;
        return NO;
    }

    CGLSetCurrentContext(_glContext);
    return gladLoadGLLoader(OpenGLFunctionLoader) != 0;
}

#pragma mark - Video

- (OEGameCoreRendering)gameCoreRendering
{
    return OEGameCoreRenderingBitmap;
}

- (const void *)getVideoBufferWithHint:(void *)hint
{
    return _videoBuffer.mutableBytes;
}

- (OEIntSize)bufferSize
{
    return OEIntSizeMake(kOutputWidth, kOutputHeight);
}

- (OEIntSize)aspectSize
{
    return OEIntSizeMake(kOutputWidth, kOutputHeight);
}

- (NSTimeInterval)frameInterval
{
    // Despite the name this is a frame rate: the base class divides by it to get the frame duration
    return 60.0;
}

- (uint32_t)pixelFormat
{
    return OEPixelFormat_BGRA;
}

- (uint32_t)pixelType
{
    return OEPixelType_UNSIGNED_INT_8_8_8_8_REV;
}

- (void)initializeGraphicsIfNeeded
{
    if (_graphicsInitialized || _emulator == nullptr)
        return;

    if (![self createOpenGLContext])
    {
        NSLog(@"Panda3DS: the OpenGL context could not be created");
        [self stopEmulation];
        return;
    }

    _graphicsInitialized = YES;

    Renderer *renderer = _emulator->getRenderer();
    renderer->initGraphicsContext(nullptr);
    renderer->setOutputSize(kOutputWidth, kOutputHeight);
}

#pragma mark - Audio

- (NSUInteger)channelCount
{
    return kChannelCount;
}

- (double)audioSampleRate
{
    return kSampleRate;
}

- (id<OEAudioBuffer>)audioBufferAtIndex:(NSUInteger)index
{
    return self.ringBuffer;
}

- (void)drainAudioSamples
{
    if (_emulator == nullptr)
        return;

    // Move everything the DSP produced during this frame into OpenEmu's ring buffer
    auto& samples = _emulator->getDSP()->getSamples();
    s16 buffer[1024];
    size_t count;

    while ((count = samples.pop(buffer, std::size(buffer))) > 0)
        [self.ringBuffer write:buffer maxLength:count * sizeof(s16)];
}

#pragma mark - Input

- (void)handleButton:(OEPanda3DSButton)button pressed:(BOOL)pressed
{
    if (_emulator == nullptr)
        return;

    HIDService& hid = _emulator->getServiceManager().getHID();

    switch (button)
    {
        case OEPanda3DSButtonUp:    hid.setKey(HID::Keys::Up, pressed);    break;
        case OEPanda3DSButtonDown:  hid.setKey(HID::Keys::Down, pressed);  break;
        case OEPanda3DSButtonLeft:  hid.setKey(HID::Keys::Left, pressed);  break;
        case OEPanda3DSButtonRight: hid.setKey(HID::Keys::Right, pressed); break;
        case OEPanda3DSButtonA:     hid.setKey(HID::Keys::A, pressed);     break;
        case OEPanda3DSButtonB:     hid.setKey(HID::Keys::B, pressed);     break;
        case OEPanda3DSButtonX:     hid.setKey(HID::Keys::X, pressed);     break;
        case OEPanda3DSButtonY:     hid.setKey(HID::Keys::Y, pressed);     break;
        case OEPanda3DSButtonL:     hid.setKey(HID::Keys::L, pressed);     break;
        case OEPanda3DSButtonR:     hid.setKey(HID::Keys::R, pressed);     break;
        case OEPanda3DSButtonZL:    hid.setKey(HID::Keys::ZL, pressed);    break;
        case OEPanda3DSButtonZR:    hid.setKey(HID::Keys::ZR, pressed);    break;
        case OEPanda3DSButtonStart: hid.setKey(HID::Keys::Start, pressed); break;
        case OEPanda3DSButtonSelect: hid.setKey(HID::Keys::Select, pressed); break;
        default: break;
    }
}

- (void)didPushPanda3DSButton:(OEPanda3DSButton)button forPlayer:(NSUInteger)player
{
    [self handleButton:button pressed:YES];
}

- (void)didReleasePanda3DSButton:(OEPanda3DSButton)button forPlayer:(NSUInteger)player
{
    [self handleButton:button pressed:NO];
}

- (void)didMovePanda3DSJoystickDirection:(OEPanda3DSButton)button withValue:(CGFloat)value forPlayer:(NSUInteger)player
{
    if (_emulator == nullptr)
        return;

    HIDService& hid = _emulator->getServiceManager().getHID();
    s16 offset = (s16)std::lround(std::clamp((double)value, -1.0, 1.0) * kCirclePadRange);

    switch (button)
    {
        // The circle pad's Y axis points up, OpenEmu's values point down
        case OEPanda3DSButtonCirclePadUp:    hid.setCirclepadY(offset); break;
        case OEPanda3DSButtonCirclePadDown:  hid.setCirclepadY(-offset); break;
        case OEPanda3DSButtonCirclePadLeft:  hid.setCirclepadX(-offset); break;
        case OEPanda3DSButtonCirclePadRight: hid.setCirclepadX(offset); break;
        default: break;
    }
}

#pragma mark - Touch screen

- (void)didTouchPanda3DSScreenPoint:(OEIntPoint)point
{
    if (_emulator == nullptr)
        return;

    ScreenLayout::WindowCoordinates coordinates;
    ScreenLayout::calculateCoordinates(coordinates, kOutputWidth, kOutputHeight,
                                       _emulator->getConfig().topScreenSize,
                                       _emulator->getConfig().screenLayout);

    HIDService& hid = _emulator->getServiceManager().getHID();

    // Only the bottom screen is a touch screen; touches elsewhere are ignored
    if (point.x < (NSInteger)coordinates.bottomScreenX ||
        point.y < (NSInteger)coordinates.bottomScreenY ||
        point.x >= (NSInteger)(coordinates.bottomScreenX + coordinates.bottomScreenWidth) ||
        point.y >= (NSInteger)(coordinates.bottomScreenY + coordinates.bottomScreenHeight))
    {
        hid.releaseTouchScreen();
        return;
    }

    // OpenEmu sends output pixels, the HID service wants the touch screen's own resolution
    double x = (point.x - coordinates.bottomScreenX) * (double)ScreenLayout::BOTTOM_SCREEN_WIDTH / coordinates.bottomScreenWidth;
    double y = (point.y - coordinates.bottomScreenY) * (double)ScreenLayout::BOTTOM_SCREEN_HEIGHT / coordinates.bottomScreenHeight;

    hid.setTouchScreenPress((u16)x, (u16)y);
}

- (void)didReleasePanda3DSTouch
{
    if (_emulator == nullptr)
        return;

    _emulator->getServiceManager().getHID().releaseTouchScreen();
}

@end
