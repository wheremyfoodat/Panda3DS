#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.hpp>
#import <QWindow>
#import <QuartzCore/QuartzCore.hpp>

#import "panda_qt/screen/screen_mtl.hpp"

id<MTLDevice> metalDevice = nil;

bool ScreenWidgetMTL::createMetalContext() {
	NSView* nativeView = (NSView*)this->winId();
	// Retain the layer so that we can manually memory manage it.
	CAMetalLayer* metalLayer = [[CAMetalLayer layer] retain];

	if (!metalLayer) {
		return false;
	}

	metalDevice = MTLCreateSystemDefaultDevice();

	if (!metalDevice) {
		NSLog(@"Failed to create metal device");
		return false;
	}

	metalLayer.device = metalDevice;
	metalLayer.framebufferOnly = NO;
	metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;

	CGFloat scale = [nativeView window].backingScaleFactor;
	CGSize pointSize = nativeView.bounds.size;

	metalLayer.contentsScale = scale;
	metalLayer.drawableSize = CGSizeMake(pointSize.width * scale, pointSize.height * scale);

	[nativeView setLayer:metalLayer];
	[nativeView setWantsLayer:YES];

	CA::MetalLayer* cppLayer = (CA::MetalLayer*)metalLayer;
	mtkLayer = static_cast<void*>(cppLayer);

	return true;
}

void ScreenWidgetMTL::resizeMetalView() {
	NSView* view = (NSView*)this->windowHandle()->winId();
	CAMetalLayer* metalLayer = (CAMetalLayer*)[view layer];

	if (metalLayer) {
		metalLayer.drawableSize = CGSizeMake(surfaceWidth, surfaceHeight);
	}
}

ScreenWidgetMTL::~ScreenWidgetMTL() {
	if (mtkLayer) {
		CAMetalLayer* metalLayer = (__bridge CAMetalLayer*)static_cast<CA::MetalLayer*>(mtkLayer);

		NSView* view = (NSView*)this->winId();
		[view setLayer:nil];
		[view setWantsLayer:NO];

		// Release Metal device and layer
		metalLayer.device = nil;
		[metalLayer release];
		[metalDevice release];

		mtkLayer = nullptr;
	}
}