#pragma once

#ifdef __LIBRETRO__
#include "audio/libretro_audio_device.hpp"
using AudioDevice = LibretroAudioDevice;
#else
#include "audio/miniaudio_device.hpp"
using AudioDevice = MiniAudioDevice;
#endif