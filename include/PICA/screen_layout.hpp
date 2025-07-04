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
	};

	// Calculate screen coordinates on the screen for a given layout & a given size for the output window
	// Used in both the renderers and in the frontends (To eg calculate touch screen boundaries)
	static void calculateCoordinates(WindowCoordinates& coordinates, u32 outputWindowWidth, u32 outputWindowHeight, Layout layout) {
		layout = Layout::SideBySideFlipped;
		const float topScreenPercentage = 0.5f;

		const float destAspect = float(outputWindowWidth) / float(outputWindowHeight);

		if (layout == Layout::Default || layout == Layout::DefaultFlipped) {
			// Calculate available height for each screen based on split
			int availableTopHeight = int(outputWindowHeight * topScreenPercentage + 0.5f);
			int availableBottomHeight = outputWindowHeight - availableTopHeight;

			// Calculate scales for top and bottom screens, and then the actual sizes
			float scaleTopX = float(outputWindowWidth) / float(TOP_SCREEN_WIDTH);
			float scaleTopY = float(availableTopHeight) / float(TOP_SCREEN_HEIGHT);
			float scaleTop = std::min(scaleTopX, scaleTopY);

			float scaleBottomX = float(outputWindowWidth) / float(BOTTOM_SCREEN_WIDTH);
			float scaleBottomY = float(availableBottomHeight) / float(BOTTOM_SCREEN_HEIGHT);
			float scaleBottom = std::min(scaleBottomX, scaleBottomY);

			int topScreenWidth = int(TOP_SCREEN_WIDTH * scaleTop + 0.5f);
			int topScreenHeight = int(TOP_SCREEN_HEIGHT * scaleTop + 0.5f);

			int bottomScreenWidth = int(BOTTOM_SCREEN_WIDTH * scaleBottom + 0.5f);
			int bottomScreenHeight = int(BOTTOM_SCREEN_HEIGHT * scaleBottom + 0.5f);

			// Center screens horizontally
			int topScreenX = (outputWindowWidth - topScreenWidth) / 2;
			int bottomScreenX = (outputWindowWidth - bottomScreenWidth) / 2;

			coordinates.topScreenWidth = topScreenWidth;
			coordinates.topScreenHeight = topScreenHeight;
			coordinates.bottomScreenWidth = bottomScreenWidth;
			coordinates.bottomScreenHeight = bottomScreenHeight;

			coordinates.topScreenX = topScreenX;
			coordinates.bottomScreenX = bottomScreenX;

			if (layout == Layout::Default) {
				coordinates.topScreenY = 0;
				coordinates.bottomScreenY = topScreenHeight;
			} else {
				coordinates.bottomScreenY = 0;
				coordinates.topScreenY = bottomScreenHeight;
			}

			coordinates.windowWidth = outputWindowWidth;
			coordinates.windowHeight = outputWindowHeight;

			// Default layout can be rendered using a single blit, flipped layout can't
			if (layout == Layout::Default) {
				coordinates.singleBlitInfo.destX = coordinates.topScreenX;
				coordinates.singleBlitInfo.destY = coordinates.topScreenY;
				coordinates.singleBlitInfo.destWidth = coordinates.topScreenWidth;
				coordinates.singleBlitInfo.destHeight = coordinates.topScreenHeight + coordinates.bottomScreenHeight;
			} else {
				// Dummy data for the single blit info, as we can't render the screen in 1 blit when the screens are side-by-sid
				coordinates.singleBlitInfo.destX = 0;
				coordinates.singleBlitInfo.destY = 0;
				coordinates.singleBlitInfo.destWidth = 0;
				coordinates.singleBlitInfo.destHeight = 0;
			}
		} else if (layout == Layout::SideBySide || layout == Layout::SideBySideFlipped) {
			// Calculate available width for each screen based on split
			int availableTopWidth = int(outputWindowWidth * topScreenPercentage + 0.5f);
			int availableBottomWidth = outputWindowWidth - availableTopWidth;

			// Calculate scales for top and bottom screens, and then the actual sizes
			float scaleTop = std::min(float(availableTopWidth) / float(TOP_SCREEN_WIDTH), float(outputWindowHeight) / float(TOP_SCREEN_HEIGHT));
			float scaleBottom =
				std::min(float(availableBottomWidth) / float(BOTTOM_SCREEN_WIDTH), float(outputWindowHeight) / float(BOTTOM_SCREEN_HEIGHT));

			int topScreenWidth = int(TOP_SCREEN_WIDTH * scaleTop + 0.5f);
			int topScreenHeight = int(TOP_SCREEN_HEIGHT * scaleTop + 0.5f);
			int bottomScreenWidth = int(BOTTOM_SCREEN_WIDTH * scaleBottom + 0.5f);
			int bottomScreenHeight = int(BOTTOM_SCREEN_HEIGHT * scaleBottom + 0.5f);

			// Vertically center the tallest screen
			int maxHeight = std::max(topScreenHeight, bottomScreenHeight);
			int baseY = (outputWindowHeight - maxHeight) / 2;

			int topScreenY = baseY + (maxHeight - topScreenHeight) / 2;
			int bottomScreenY = baseY + (maxHeight - bottomScreenHeight) / 2;

			if (layout == Layout::SideBySide) {
				coordinates.topScreenX = (outputWindowWidth - (topScreenWidth + bottomScreenWidth)) / 2;
				coordinates.bottomScreenX = coordinates.topScreenX + topScreenWidth;
			} else {
				coordinates.bottomScreenX = (outputWindowWidth - (topScreenWidth + bottomScreenWidth)) / 2;
				coordinates.topScreenX = coordinates.bottomScreenX + bottomScreenWidth;
			}

			coordinates.topScreenY = topScreenY;
			coordinates.topScreenWidth = topScreenWidth;
			coordinates.topScreenHeight = topScreenHeight;

			coordinates.bottomScreenY = bottomScreenY;
			coordinates.bottomScreenWidth = bottomScreenWidth;
			coordinates.bottomScreenHeight = bottomScreenHeight;

			coordinates.windowWidth = outputWindowWidth;
			coordinates.windowHeight = outputWindowHeight;

			// Dummy data for the single blit info, as we can't render the screen in 1 blit when the screens are side-by-side
			coordinates.singleBlitInfo.destX = 0;
			coordinates.singleBlitInfo.destY = 0;
			coordinates.singleBlitInfo.destWidth = 0;
			coordinates.singleBlitInfo.destHeight = 0;
		} else {
			Helpers::panic("Unimplemented screen layout");
		}
	}
}  // namespace ScreenLayout
