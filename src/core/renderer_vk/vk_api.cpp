#include "renderer_vk/vk_api.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

// Implement vma functions
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
