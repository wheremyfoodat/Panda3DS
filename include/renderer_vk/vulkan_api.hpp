#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_EXCEPTIONS
// Disable asserts on result-codes
#define VULKAN_HPP_ASSERT_ON_RESULT
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_hash.hpp>