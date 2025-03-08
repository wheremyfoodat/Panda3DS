#import <Foundation/Foundation.h>

extern "C" {
#include "ios_driver.h"
}

#undef ABS
#undef NO

#include <memory>
#include "emulator.hpp"

#define IOS_EXPORT extern "C" __attribute__((visibility("default")))

std::unique_ptr<Emulator> emulator = nullptr;
HIDService* hidService = nullptr;

IOS_EXPORT void iosCreateEmulator() {
	printf("Creating emulator\n");

	emulator = std::make_unique<Emulator>();
	hidService = &emulator->getServiceManager().getHID();
	emulator->initGraphicsContext(nullptr);

	// auto path = emulator->getAppDataRoot() / "Kirb Demo.3ds";
	// auto path = emulator->getAppDataRoot() / "Kirb Demo.3ds";

	auto path = emulator->getAppDataRoot() / "SimplerTri.elf";
	emulator->loadROM(path);
	printf("Created emulator\n");
}

IOS_EXPORT void iosRunFrame(void* layer) {
	printf("Running a frame\n");
	emulator->getRenderer()->setMTKLayer(layer);
	emulator->runFrame();
	printf("Ran a frame\n");
}