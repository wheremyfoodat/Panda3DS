#include "renderer_gl/textures.hpp"
#include "colour.hpp"
#include <array>

using namespace Helpers;

void Texture::allocate() {
    glGenTextures(1, &texture.m_handle);
    texture.create(size.u(), size.v(), GL_RGBA8);
    texture.bind();

#ifdef GPU_DEBUG_INFO
	const auto name = Helpers::format("Surface %dx%d %s from 0x%08X", size.x(), size.y(), PICA::textureFormatToString(format), location);
	OpenGL::setObjectLabel(GL_TEXTURE, texture.handle(), name.c_str());
#endif

    setNewConfig(config);
}

// Set the texture's configuration, which includes min/mag filters, wrapping S/T modes, and so on
void Texture::setNewConfig(u32 cfg) {
    config = cfg;
    // The wrapping mode field is 3 bits instead of 2 bits.
    // The bottom 4 undocumented wrapping modes are taken from Citra.
    static constexpr std::array<OpenGL::WrappingMode, 8> wrappingModes = {
        OpenGL::ClampToEdge, OpenGL::ClampToBorder, OpenGL::Repeat, OpenGL::RepeatMirrored,
        OpenGL::ClampToEdge, OpenGL::ClampToBorder, OpenGL::Repeat, OpenGL::Repeat
    };

    const auto magFilter = (cfg & 0x2) != 0 ? OpenGL::Linear : OpenGL::Nearest;
    const auto minFilter = (cfg & 0x4) != 0 ? OpenGL::Linear : OpenGL::Nearest;
    const auto wrapT = wrappingModes[getBits<8, 3>(cfg)];
    const auto wrapS = wrappingModes[getBits<12, 3>(cfg)];

    texture.setMinFilter(minFilter);
    texture.setMagFilter(magFilter);
    texture.setWrapS(wrapS);
    texture.setWrapT(wrapT);
}

void Texture::free() {
	valid = false;

	if (texture.exists()) {
		texture.free();
	}
}

void Texture::uploadTexture(std::span<const u8> data) {
    std::vector<u32> decoded = decodeTexture(data);
    texture.bind();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.u(), size.v(), GL_RGBA, GL_UNSIGNED_BYTE, decoded.data());
}
