#include "screen_layout.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_map>

using namespace ScreenLayout;

// Calculate screen coordinates on the screen for a given layout & a given size for the output window
// Used in both the renderers and in the frontends (To eg calculate touch screen boundaries)
void ScreenLayout::calculateCoordinates(
	WindowCoordinates& coordinates, u32 outputWindowWidth, u32 outputWindowHeight, float topScreenPercentage, Layout layout
) {
	if (layout == Layout::Default || layout == Layout::DefaultFlipped) {
		// Calculate available height for each screen based on split
		int availableTopHeight = int(outputWindowHeight * topScreenPercentage + 0.5f);
		int availableBottomHeight = outputWindowHeight - availableTopHeight;

		// Calculate scales for top and bottom screens, and then the actual sizes
		float scaleTop = std::min(float(outputWindowWidth) / float(TOP_SCREEN_WIDTH), float(availableTopHeight) / float(TOP_SCREEN_HEIGHT));
		float scaleBottom =
			std::min(float(outputWindowWidth) / float(BOTTOM_SCREEN_WIDTH), float(availableBottomHeight) / float(BOTTOM_SCREEN_HEIGHT));

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

		// Check if we can blit the screens in 1 blit. If not, we'll break it into two.
		// TODO: Maybe add some more size-related checks too.
		coordinates.singleBlitInfo.canDoSingleBlit = layout == Layout::Default && topScreenPercentage == 0.5 &&
													 coordinates.topScreenY + coordinates.topScreenHeight == coordinates.bottomScreenY;
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
		int centerY = (outputWindowHeight - maxHeight) / 2;

		int topScreenY = centerY + (maxHeight - topScreenHeight) / 2;
		int bottomScreenY = centerY + (maxHeight - bottomScreenHeight) / 2;

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
		coordinates.singleBlitInfo.canDoSingleBlit = false;
		coordinates.singleBlitInfo.destX = 0;
		coordinates.singleBlitInfo.destY = 0;
		coordinates.singleBlitInfo.destWidth = 0;
		coordinates.singleBlitInfo.destHeight = 0;
	} else {
		Helpers::panic("Unimplemented screen layout");
	}
}

Layout ScreenLayout::layoutFromString(std::string inString) {
	// Transform to lower-case to make the setting case-insensitive
	std::transform(inString.begin(), inString.end(), inString.begin(), [](unsigned char c) { return std::tolower(c); });

	static const std::unordered_map<std::string, Layout> map = {
		{"default", Layout::Default},
		{"defaultflipped", Layout::DefaultFlipped},
		{"sidebyside", Layout::SideBySide},
		{"sidebysideflipped", Layout::SideBySideFlipped},
	};

	if (auto search = map.find(inString); search != map.end()) {
		return search->second;
	}

	printf("Invalid screen layout. Defaulting to Default\n");
	return Layout::Default;
}

const char* ScreenLayout::layoutToString(Layout layout) {
	switch (layout) {
		case Layout::Default: return "default";
		case Layout::DefaultFlipped: return "defaultFlipped";
		case Layout::SideBySide: return "sideBySide";
		case Layout::SideBySideFlipped: return "sideBySideFlipped";
		default: return "invalid";
	}
}
    