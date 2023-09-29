#pragma once

#include <glad/gl.h>

#include "SDL.h"
#include "helpers.hpp"
#include "config.hpp"

struct SDL_Window;

class Window {
  public:
    Window() = default;
    virtual ~Window() = default;

	virtual void* getHandle() const = 0;
	virtual int getWidth() const = 0;
	virtual int getHeight() const = 0;
	virtual void swapBuffers() = 0;
};

class WindowSDL : public Window {
  public:
    explicit WindowSDL(const EmulatorConfig& config, u32 width, u32 height) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
            Helpers::panic("Failed to initialize SDL2");
        }

		// Make SDL use consistent positional button mapping
		SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
		if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
			Helpers::warn("Failed to initialize SDL2 GameController: %s", SDL_GetError());
		}

		// We need OpenGL for software rendering or for OpenGL if it's enabled
		bool needOpenGL = config.rendererType == RendererType::Software;
#ifdef PANDA3DS_ENABLE_OPENGL
		needOpenGL = needOpenGL || (config.rendererType == RendererType::OpenGL);
#endif

		if (needOpenGL) {
			// Demand 3.3 core for software renderer, or 4.1 core for OpenGL renderer (max available on MacOS)
			// MacOS gets mad if we don't explicitly demand a core profile
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, config.rendererType == RendererType::Software ? 3 : 4);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, config.rendererType == RendererType::Software ? 3 : 1);
			window = SDL_CreateWindow("Alber", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 400, 480, SDL_WINDOW_OPENGL);

			if (window == nullptr) {
				Helpers::panic("Window creation failed: %s", SDL_GetError());
			}

			glContext = SDL_GL_CreateContext(window);
			if (glContext == nullptr) {
				Helpers::panic("OpenGL context creation failed: %s", SDL_GetError());
			}

			if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
				Helpers::panic("OpenGL init failed");
			}
		}

#ifdef PANDA3DS_ENABLE_VULKAN
		if (config.rendererType == RendererType::Vulkan) {
			window = SDL_CreateWindow("Alber", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
									  width, height, SDL_WINDOW_VULKAN);

			if (window == nullptr) {
				Helpers::warn("Window creation failed: %s", SDL_GetError());
			}
		}
#endif
	}

	~WindowSDL() override {
		SDL_DestroyWindow(window);
		if (glContext) {
			SDL_GL_DeleteContext(glContext);
		}
	}

	void* getHandle() const override {
		return window;
	}

	int getWidth() const override {
		int width;
		SDL_GL_GetDrawableSize(window, &width, nullptr);
		return width;
	}

	int getHeight() const override {
		int height;
		SDL_GL_GetDrawableSize(window, nullptr, &height);
		return height;
	}

	void swapBuffers() override {
		if (glContext) {
			SDL_GL_SwapWindow(window);
		}
	}

  private:
    SDL_Window* window;
#ifdef PANDA3DS_ENABLE_OPENGL
    SDL_GLContext glContext;
#endif
};
