#import <Foundation/Foundation.h>

extern "C" {
#include "ios_driver.h"
}

// Apple's Foundation headers define some macros globablly that create issues with our own code, so remove the definitions
#undef ABS
#undef NO

#include <memory>
#include "emulator.hpp"

// The Objective-C++ bridge functions must be exported without name mangling in order for the SwiftUI frontend to be able to call them
#define IOS_EXPORT extern "C" __attribute__((visibility("default")))

std::unique_ptr<Emulator> emulator = nullptr;

IOS_EXPORT void iosCreateEmulator() {
	printf("Creating emulator\n");

	emulator = std::make_unique<Emulator>();
	emulator->initGraphicsContext(nullptr);
}

IOS_EXPORT void iosRunFrame(CAMetalLayer* layer) {
	void* layerBridged = (__bridge void*)layer;

	emulator->getRenderer()->setMTKLayer(layerBridged);
	emulator->runFrame();
}

IOS_EXPORT void iosLoadROM(NSString* pathNS) {
	auto path = std::filesystem::path([pathNS UTF8String]);
	emulator->loadROM(path);
}

IOS_EXPORT void iosSetOutputSize(uint32_t width, uint32_t height) {
	emulator->setOutputSize(width, height);
}