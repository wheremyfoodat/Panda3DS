#pragma once

#include "helpers.hpp"
// TODO: remove dependency on OpenGL
#include "opengl.hpp"

void decodeTexelABGR8ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelBGR8ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelA1BGR5ToBGR5A1(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelA1BGR5ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelB5G6R5ToB5G6R5(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelB5G6R5ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelABGR4ToABGR4(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelABGR4ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelAI8ToRG8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelGR8ToRG8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelI8ToR8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelA8ToA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelAI4ToABGR4(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelAI4ToRG8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelI4ToR8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelA4ToA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelETC1ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
void decodeTexelETC1A4ToRGBA8(OpenGL::uvec2 size, u32 u, u32 v, std::span<const u8> inData, u8* outData);
