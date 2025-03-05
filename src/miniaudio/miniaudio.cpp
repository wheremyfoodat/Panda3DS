// We do not need the ability to be able to encode or decode audio files for the time being
// So we disable said functionality to make the executable smaller.
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION

#ifndef PANDA3DS_IOS
#include "miniaudio.h"
#endif