#pragma once

#include <span>
#include <type_traits>
#include <utility>

#include "vulkan_api.hpp"

namespace Vulkan {

	VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData
	);

	void setObjectName(vk::Device device, vk::ObjectType objectType, const void* objectHandle, const char* format, ...);

	template <typename T, typename = std::enable_if_t<vk::isVulkanHandleType<T>::value == true>, typename... ArgsT>
	inline void setObjectName(vk::Device device, const T objectHandle, const char* format, ArgsT&&... args) {
		setObjectName(device, T::objectType, objectHandle, format, std::forward<ArgsT>(args)...);
	}

	void beginDebugLabel(vk::CommandBuffer commandBuffer, std::span<const float, 4> color, const char* format, ...);

	void insertDebugLabel(vk::CommandBuffer commandBuffer, std::span<const float, 4> color, const char* format, ...);

	void endDebugLabel(vk::CommandBuffer commandBuffer);

	class DebugLabelScope {
	  private:
		const vk::CommandBuffer commandBuffer;

	  public:
		template <typename... ArgsT>
		DebugLabelScope(vk::CommandBuffer targetCommandBuffer, std::span<const float, 4> color, const char* format, ArgsT&&... args)
			: commandBuffer(targetCommandBuffer) {
			beginDebugLabel(commandBuffer, color, format, std::forward<ArgsT>(args)...);
		}

		template <typename... ArgsT>
		void operator()(std::span<const float, 4> color, const char* format, ArgsT&&... args) const {
			insertDebugLabel(commandBuffer, color, format, std::forward<ArgsT>(args)...);
		}

		~DebugLabelScope() { endDebugLabel(commandBuffer); }
	};

}  // namespace Vulkan