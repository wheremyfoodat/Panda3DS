#pragma once
#include <algorithm>

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
		Default,            // Top screen up, bottom screen down
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
			int destX = 0, destY = 0;
			int destWidth = TOP_SCREEN_WIDTH;
			int destHeight = CONSOLE_HEIGHT;
		} singleBlitInfo;

		float scale = 1.0f;
	};

	// Calculate screen coordinates on the screen for a given layout & a given size for the output window
	// Used in both the renderers and in the frontends (To eg calculate touch screen boundaries)
	static void calculateCoordinates(WindowCoordinates& coordinates, u32 outputWindowWidth, u32 outputWindowHeight, Layout layout) {
		layout = Layout::SideBySideFlipped;
		const float destAspect = float(outputWindowWidth) / float(outputWindowHeight);

		if (layout == Layout::Default || layout == Layout::DefaultFlipped) {
			const float srcAspect = 400.0 / 480.0;

			int destX = 0, destY = 0, destWidth = outputWindowWidth, destHeight = outputWindowHeight;

			if (destAspect > srcAspect) {
				// Window is wider than source
				destWidth = int(outputWindowHeight * srcAspect + 0.5f);
				destX = (outputWindowWidth - destWidth) / 2;
			} else {
				// Window is taller than source
				destHeight = int(outputWindowWidth / srcAspect + 0.5f);
				destY = (outputWindowHeight - destHeight) / 2;
			}

			// How much we'll scale the output by
			const float scale = float(destWidth) / float(TOP_SCREEN_WIDTH);

			// Calculate coordinates and return them
			// TODO: This will break when we allow screens to be scaled separately
			coordinates.topScreenX = u32(destX);
			coordinates.topScreenWidth = u32(float(TOP_SCREEN_WIDTH) * scale);
			coordinates.bottomScreenX = u32(destX) + BOTTOM_SCREEN_X_OFFSET * scale;
			coordinates.bottomScreenWidth = u32(float(BOTTOM_SCREEN_WIDTH) * scale);

			if (layout == Layout::Default) {
				coordinates.topScreenY = u32(destY + (destHeight - int(CONSOLE_HEIGHT * scale)) / 2);
				coordinates.topScreenHeight = u32(float(TOP_SCREEN_HEIGHT) * scale);

				coordinates.bottomScreenY = coordinates.topScreenY + TOP_SCREEN_HEIGHT * scale;
				coordinates.bottomScreenHeight = coordinates.topScreenHeight;
			} else {
				// Flip the screens vertically
				coordinates.bottomScreenY = u32(destY + (destHeight - int(CONSOLE_HEIGHT * scale)) / 2);
				coordinates.bottomScreenHeight = u32(float(BOTTOM_SCREEN_HEIGHT) * scale);

				coordinates.topScreenY = coordinates.bottomScreenY + TOP_SCREEN_HEIGHT * scale;
				coordinates.topScreenHeight = coordinates.bottomScreenHeight;
			}

			coordinates.windowWidth = outputWindowWidth;
			coordinates.windowHeight = outputWindowHeight;
			coordinates.scale = scale;

			coordinates.singleBlitInfo.destX = destX;
			coordinates.singleBlitInfo.destY = destY;
			coordinates.singleBlitInfo.destWidth = destWidth;
			coordinates.singleBlitInfo.destHeight = destHeight;
		} else if (layout == Layout::SideBySide || layout == Layout::SideBySideFlipped) {
			// For side-by-side layouts, the 3DS aspect ratio is calculated as (top width + bottom width) / height
			const float srcAspect = float(TOP_SCREEN_WIDTH + BOTTOM_SCREEN_WIDTH) / float(CONSOLE_HEIGHT);

			int destX = 0, destY = 0, destWidth = outputWindowWidth, destHeight = outputWindowHeight;

			if (destAspect > srcAspect) {
				// Window is wider than the side-by-side layout — center horizontally
				destWidth = int(outputWindowHeight * srcAspect + 0.5f);
				destX = (outputWindowWidth - destWidth) / 2;
			} else {
				// Window is taller — center vertically
				destHeight = int(outputWindowWidth / srcAspect + 0.5f);
				destY = (outputWindowHeight - destHeight) / 2;
			}

			// How much we'll scale the output by. Again, we want to take both screens into account
			const float scale = float(destWidth) / float(TOP_SCREEN_WIDTH + BOTTOM_SCREEN_WIDTH);

			// Annoyingly, the top screen is wider than the bottom screen, so to display them side-by-side and centered
			// vertically, we have to account for that

			// The top screen is currently always larger, but it's still best to check which screen is taller anyways
			// Since eventually we'll want to let users scale each screen separately.
			const int topHeightScaled = int(TOP_SCREEN_HEIGHT * scale + 0.5f);
			const int bottomHeightScaled = int(BOTTOM_SCREEN_HEIGHT * scale + 0.5f);
			const int maxHeight = std::max(topHeightScaled, bottomHeightScaled);
			const int centerY = destY + (destHeight - maxHeight) / 2;

			coordinates.topScreenY = u32(centerY + (maxHeight - topHeightScaled) / 2);
			coordinates.topScreenWidth = u32(TOP_SCREEN_WIDTH * scale);
			coordinates.topScreenHeight = u32(TOP_SCREEN_HEIGHT * scale);

			coordinates.bottomScreenY = u32(centerY + (maxHeight - bottomHeightScaled) / 2);
			coordinates.bottomScreenWidth = u32(BOTTOM_SCREEN_WIDTH * scale);
			coordinates.bottomScreenHeight = u32(BOTTOM_SCREEN_HEIGHT * scale);

			if (layout == Layout::SideBySide) {
				coordinates.topScreenX = destX;
				coordinates.bottomScreenX = destX + coordinates.topScreenWidth;
			} else {
				// Flip the screens horizontally
				coordinates.bottomScreenX = destX;
				coordinates.topScreenX = destX + coordinates.bottomScreenWidth;
			}

			coordinates.windowWidth = outputWindowWidth;
			coordinates.windowHeight = outputWindowHeight;
			coordinates.scale = scale;

			// Set singleBlitInfo values to dummy values. Side-by-side screens can't be rendere in only 1 blit.
			coordinates.singleBlitInfo.destX = 0;
			coordinates.singleBlitInfo.destY = 0;
			coordinates.singleBlitInfo.destWidth = 0;
			coordinates.singleBlitInfo.destHeight = 0;
		} else {
			Helpers::panic("Unimplemented screen layout");
		}
	}
}  // namespace ScreenLayout
