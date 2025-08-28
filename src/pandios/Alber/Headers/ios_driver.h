#pragma once
#include <Foundation/Foundation.h>
#include <QuartzCore/QuartzCore.h>
#include <stdint.h>

void iosCreateEmulator();
void iosLoadROM(NSString* pathNS);
void iosRunFrame(CAMetalLayer* layer);
void iosSetOutputSize(uint32_t width, uint32_t height);
