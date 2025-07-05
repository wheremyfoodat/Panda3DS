#pragma once
#include <string>

#include "helpers.hpp"

namespace ScreenLayout {
	static constexpr u32 TOP_SCREEN_WIDTH = 400;
	static constexpr u32 BOTTOM_SCREEN_WIDTH = 320;
	static constexpr u32 TOP_SCREEN_HEIGHT = 240;
	static constexpr u32 BOTTOM_SCREEN_HEIGHT = 240;

	// The bottom screen is less wide by 80 pixels, so we center it by offsetting it 40 pixels right
	static constexpr u32 BOTTOM_SCREEN_X_OFFSET = 40;
	static constexpr u32 CONSOLE_HEIGHT = TOP_SCREEN_HEIGHT + BOTTOM_SCREEN_HEIGHT;

	enum class Layout {
		Default = 0,        // Top screen up, bottom screen down
		DefaultFlipped,     // Top screen down, bottom screen up
		SideBySide,         // Top screen left, bottom screen right,
		SideBySideFlipped,  // Top screen right, bottom screen left,
	};

	// For properly handling touchscreen, we have to remember what window coordinates our screens map to
	// We also remember some more information that is useful to our renderers, particularly for the final screen blit.
	struct WindowCoordinates {
		u32 topScreenX = 0;
		u32 topScreenY = 0;
		u32 topScreenWidth = TOP_SCREEN_WIDTH;
		u32 topScreenHeight = TOP_SCREEN_HEIGHT;

		u32 bottomScreenX = BOTTOM_SCREEN_X_OFFSET;
		u32 bottomScreenY = TOP_SCREEN_HEIGHT;
		u32 bottomScreenWidth = BOTTOM_SCREEN_WIDTH;
		u32 bottomScreenHeight = BOTTOM_SCREEN_HEIGHT;

		u32 windowWidth = topScreenWidth;
		u32 windowHeight = topScreenHeight + bottomScreenHeight;

		// Information used when optimizing the final screen blit into a single blit
		struct {
			// Can we actually render both of the screens in a single blit?
			bool canDoSingleBlit = false;
			// Blit information used if we can
			int destX = 0, destY = 0;
			int destWidth = TOP_SCREEN_WIDTH;
			int destHeight = CONSOLE_HEIGHT;
		} singleBlitInfo;
	};

	// Calculate screen coordinates on the screen for a given layout & a given size for the output window
	// Used in both the renderers and in the frontends (To eg calculate touch screen boundaries)
	void calculateCoordinates(
		WindowCoordinates& coordinates, u32 outputWindowWidth, u32 outputWindowHeight, float topScreenPercentage, Layout layout
	);

	Layout layoutFromString(std::string inString);
	const char* layoutToString(Layout layout);
}  // namespace ScreenLayout
