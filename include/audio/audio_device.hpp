#ifndef __LIBRETRO__
#include "audio/miniaudio_device.hpp"
using AudioDevice = MiniAudioDevice;
#else
#include "audio/libretro_audio_device.hpp"
using AudioDevice = LibretroAudioDevice;
#endif