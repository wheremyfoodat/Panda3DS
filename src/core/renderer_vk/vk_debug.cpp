#include "renderer_vk/vk_debug.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>

#include "helpers.hpp"

static std::uint8_t severityColor(vk::DebugUtilsMessageSeverityFlagBitsEXT Severity) {
	switch (Severity) {
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose: {
			// Dark Gray
			return 90u;
		}
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo: {
			// Light Gray
			return 90u;
		}
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning: {
			// Light Magenta
			return 95u;
		}
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError: {
			// Light red
			return 91u;
		}
	}
	// Default Foreground Color
	return 39u;
}

static std::uint8_t messageTypeColor(vk::DebugUtilsMessageTypeFlagsEXT MessageType) {
	if (MessageType & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral) {
		// Dim
		return 2u;
	}
	if (MessageType & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance) {
		// Bold/Bright
		return 1u;
	}
	if (MessageType & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation) {
		// Light Gray
		return 90u;
	}
	// Default Foreground Color
	return 39u;
}

namespace Vulkan {

	static void debugMessageCallback(
		vk::DebugUtilsMessageSeverityFlagBitsEXT MessageSeverity, vk::DebugUtilsMessageTypeFlagsEXT MessageType,
		const vk::DebugUtilsMessengerCallbackDataEXT& CallbackData
	) {
		Helpers::debug_printf(
			"\033[%um[vk][%s]: \033[%um%s\033[0m\n", severityColor(MessageSeverity), CallbackData.pMessageIdName, messageTypeColor(MessageType),
			CallbackData.pMessage
		);
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData
	) {
		debugMessageCallback(
			vk::DebugUtilsMessageSeverityFlagBitsEXT(messageSeverity), vk::DebugUtilsMessageTypeFlagsEXT(messageType), *callbackData
		);
		return VK_FALSE;
	}

	#ifdef GPU_DEBUG_INFO
	void setObjectName(vk::Device device, vk::ObjectType objectType, const void* objectHandle, const char* format, ...) {
		va_list args;
		va_start(args, format);
		const auto nameLength = std::vsnprintf(nullptr, 0, format, args);
		va_end(args);
		if (nameLength < 0) {
			// Invalid vsnprintf
			return;
		}

		std::unique_ptr<char[]> objectName = std::make_unique<char[]>(std::size_t(nameLength) + 1u);

		// Write formatted object name
		va_start(args, format);
		std::vsnprintf(objectName.get(), std::size_t(nameLength) + 1u, format, args);
		va_end(args);

		vk::DebugUtilsObjectNameInfoEXT nameInfo = {};
		nameInfo.objectType = objectType;
		nameInfo.objectHandle = reinterpret_cast<std::uintptr_t>(objectHandle);
		nameInfo.pObjectName = objectName.get();

		if (device.setDebugUtilsObjectNameEXT(nameInfo) != vk::Result::eSuccess) {
			// Failed to set object name
		}
	}

	void beginDebugLabel(vk::CommandBuffer commandBuffer, std::span<const float, 4> color, const char* format, ...) {
		va_list args;
		va_start(args, format);
		const auto nameLength = std::vsnprintf(nullptr, 0, format, args);
		va_end(args);
		if (nameLength < 0) {
			// Invalid vsnprintf
			return;
		}

		std::unique_ptr<char[]> objectName = std::make_unique<char[]>(std::size_t(nameLength) + 1u);

		// Write formatted object name
		va_start(args, format);
		std::vsnprintf(objectName.get(), std::size_t(nameLength) + 1u, format, args);
		va_end(args);

		vk::DebugUtilsLabelEXT labelInfo = {};
		labelInfo.pLabelName = objectName.get();
		labelInfo.color[0] = color[0];
		labelInfo.color[1] = color[1];
		labelInfo.color[2] = color[2];
		labelInfo.color[3] = color[3];

		commandBuffer.beginDebugUtilsLabelEXT(labelInfo);
	}

	void insertDebugLabel(vk::CommandBuffer commandBuffer, std::span<const float, 4> color, const char* format, ...) {
		va_list args;
		va_start(args, format);
		const auto nameLength = std::vsnprintf(nullptr, 0, format, args);
		va_end(args);
		if (nameLength < 0) {
			// Invalid vsnprintf
			return;
		}

		std::unique_ptr<char[]> objectName = std::make_unique<char[]>(std::size_t(nameLength) + 1u);

		// Write formatted object name
		va_start(args, format);
		std::vsnprintf(objectName.get(), std::size_t(nameLength) + 1u, format, args);
		va_end(args);

		vk::DebugUtilsLabelEXT labelInfo = {};
		labelInfo.pLabelName = objectName.get();
		labelInfo.color[0] = color[0];
		labelInfo.color[1] = color[1];
		labelInfo.color[2] = color[2];
		labelInfo.color[3] = color[3];

		commandBuffer.insertDebugUtilsLabelEXT(labelInfo);
	}

	void endDebugLabel(vk::CommandBuffer commandBuffer) { commandBuffer.endDebugUtilsLabelEXT(); }
	#else
	void setObjectName(vk::Device device, vk::ObjectType objectType, const void* objectHandle, const char* format, ...) {}
	void beginDebugLabel(vk::CommandBuffer commandBuffer, std::span<const float, 4> color, const char* format, ...) {}
	void insertDebugLabel(vk::CommandBuffer commandBuffer, std::span<const float, 4> color, const char* format, ...) {}
	void endDebugLabel(vk::CommandBuffer commandBuffer) {}
	#endif // GPU_DEBUG_INFO

}  // namespace Vulkan