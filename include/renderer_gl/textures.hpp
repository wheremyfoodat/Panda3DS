#pragma once

#include <string>
#include "PICA/texture.hpp"
#include "opengl.hpp"

struct Texture : public PICA::Texture {
    // OpenGL resources allocated to buffer
    OpenGL::Texture texture;

    Texture() = default;
    Texture(u32 loc, PICA::TextureFmt format, u32 x, u32 y, u32 config, bool valid = true)
        : PICA::Texture(loc, format, x, y, config, valid) {}

    void allocate();
    void setNewConfig(u32 newConfig);
    void uploadTexture(std::span<const u8> data);
    void free();
};
