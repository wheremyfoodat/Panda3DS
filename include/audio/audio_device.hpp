#pragma once

#if defined(__LIBRETRO__) && defined(USE_LIBRETRO_AUDIO_DEVICE)
#include "audio/libretro_audio_device.hpp"
using AudioDevice = LibretroAudioDevice;
#else
#include "audio/miniaudio_device.hpp"
using AudioDevice = MiniAudioDevice;
#endif