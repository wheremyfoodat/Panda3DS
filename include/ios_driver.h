#pragma once
#include <Foundation/Foundation.h>
#include <QuartzCore/QuartzCore.h>

void iosCreateEmulator();
void iosLoadROM(NSString* pathNS);
void iosRunFrame(CAMetalLayer* layer);