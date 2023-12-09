#pragma once
#include "PICA/regs.hpp"
#include "boost/icl/interval.hpp"
#include "helpers.hpp"
#include "math_util.hpp"
#include "opengl.hpp"

template <typename T>
using Interval = boost::icl::right_open_interval<T>;

struct ColourBuffer {
	u32 location;
	PICA::ColorFmt format;
	OpenGL::uvec2 size;
	bool valid;

	// Range of VRAM taken up by buffer
	Interval<u32> range;
	// OpenGL resources allocated to buffer
	OpenGL::Texture texture;
	OpenGL::Framebuffer fbo;

	ColourBuffer() : valid(false) {}

	ColourBuffer(u32 loc, PICA::ColorFmt format, u32 x, u32 y, bool valid = true) : location(loc), format(format), size({x, y}), valid(valid) {
		u64 endLoc = (u64)loc + sizeInBytes();
		// Check if start and end are valid here
		range = Interval<u32>(loc, (u32)endLoc);
	}

	void allocate() {
		// Create texture for the FBO, setting up filters and the like
		// Reading back the current texture is slow, but allocate calls should be few and far between.
		// If this becomes a bottleneck, we can fix it semi-easily
		auto prevTexture = OpenGL::getTex2D();
		texture.create(size.x(), size.y(), GL_RGBA8);
		texture.bind();
		texture.setMinFilter(OpenGL::Linear);
		texture.setMagFilter(OpenGL::Linear);
		glBindTexture(GL_TEXTURE_2D, prevTexture);

#ifdef GPU_DEBUG_INFO
		const auto name = Helpers::format("Surface %dx%d %s from 0x%08X", size.x(), size.y(), PICA::textureFormatToString(format), location);
		OpenGL::setObjectLabel(GL_TEXTURE, texture.handle(), name.c_str());
#endif

		fbo.createWithDrawTexture(texture);
		fbo.bind(OpenGL::DrawAndReadFramebuffer);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			Helpers::warn("ColourBuffer: Incomplete framebuffer");
		}

		// TODO: This should not clear the framebuffer contents. It should load them from VRAM.
		GLint oldViewport[4];
		GLfloat oldClearColour[4];

		glGetIntegerv(GL_VIEWPORT, oldViewport);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, oldClearColour);

		OpenGL::setViewport(size.x(), size.y());
		OpenGL::setClearColor(0.0, 0.0, 0.0, 1.0);
		OpenGL::clearColor();
		OpenGL::setViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
		OpenGL::setClearColor(oldClearColour[0], oldClearColour[1], oldClearColour[2], oldClearColour[3]);
	}

	void free() {
		valid = false;

		if (texture.exists() || fbo.exists()) {
			texture.free();
			fbo.free();
		}
	}

	Math::Rect<u32> getSubRect(u32 inputAddress, u32 width, u32 height) {
		// PICA textures have top-left origin while OpenGL has bottom-left origin.
		// Flip the rectangle on the x axis to account for this.
		const u32 startOffset = (inputAddress - location) / sizePerPixel(format);
		const u32 x0 = (startOffset % (size.x() * 8)) / 8;
		const u32 y0 = (startOffset / (size.x() * 8)) * 8;
		return Math::Rect<u32>{x0, size.y() - y0, x0 + width, size.y() - height - y0};
	}

	bool matches(ColourBuffer& other) {
		return location == other.location && format == other.format && size.x() == other.size.x() && size.y() == other.size.y();
	}

	size_t sizeInBytes() {
		return (size_t)size.x() * (size_t)size.y() * PICA::sizePerPixel(format);
	}
};

struct DepthBuffer {
	u32 location;
	PICA::DepthFmt format;
	OpenGL::uvec2 size;  // Implicitly set to the size of the framebuffer
	bool valid;

	// Range of VRAM taken up by buffer
	Interval<u32> range;
	// OpenGL texture used for storing depth/stencil
	OpenGL::Texture texture;
	OpenGL::Framebuffer fbo;

	DepthBuffer() : valid(false) {}

	DepthBuffer(u32 loc, PICA::DepthFmt format, u32 x, u32 y, bool valid = true) : location(loc), format(format), size({x, y}), valid(valid) {
		u64 endLoc = (u64)loc + sizeInBytes();
		// Check if start and end are valid here
		range = Interval<u32>(loc, (u32)endLoc);
	}

	void allocate() {
		// Create texture for the FBO, setting up filters and the like
		// Reading back the current texture is slow, but allocate calls should be few and far between.
		// If this becomes a bottleneck, we can fix it semi-easily
		auto prevTexture = OpenGL::getTex2D();

		// Internal formats for the texture based on format
		static constexpr std::array<GLenum, 4> internalFormats = {
			GL_DEPTH_COMPONENT16,
			GL_DEPTH_COMPONENT24,
			GL_DEPTH_COMPONENT24,
			GL_DEPTH24_STENCIL8,
		};

		// Format of the texture
		static constexpr std::array<GLenum, 4> formats = {
			GL_DEPTH_COMPONENT,
			GL_DEPTH_COMPONENT,
			GL_DEPTH_COMPONENT,
			GL_DEPTH_STENCIL,
		};
		
		static constexpr std::array<GLenum, 4> types = {
			GL_UNSIGNED_SHORT,
			GL_UNSIGNED_INT,
			GL_UNSIGNED_INT,
			GL_UNSIGNED_INT_24_8,
		};

		auto internalFormat = internalFormats[(int)format];
		auto fmt = formats[(int)format];
		auto type = types[(int)format];

		texture.createDSTexture(size.x(), size.y(), internalFormat, fmt, nullptr, type, GL_TEXTURE_2D);
		texture.bind();
		texture.setMinFilter(OpenGL::Nearest);
		texture.setMagFilter(OpenGL::Nearest);

		glBindTexture(GL_TEXTURE_2D, prevTexture);
		fbo.createWithDrawTexture(texture, fmt == GL_DEPTH_STENCIL ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			Helpers::panic("Incomplete framebuffer");
		}
	}

	void free() {
		valid = false;
		if (texture.exists()) {
			texture.free();
		}
	}

	bool matches(DepthBuffer& other) {
		return location == other.location && format == other.format && size.x() == other.size.x() && size.y() == other.size.y();
	}

	size_t sizeInBytes() {
		return (size_t)size.x() * (size_t)size.y() * PICA::sizePerPixel(format);
	}
};
