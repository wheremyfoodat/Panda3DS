#pragma once

#include <optional>
#include <unordered_map>
#include "vk_api.hpp"

namespace Vulkan {
	// Implements a simple pool of reusable sampler objects
	class SamplerCache {
	  private:
		const vk::Device device;
		std::unordered_map<std::size_t, vk::UniqueSampler> samplerMap;

	  public:
        explicit SamplerCache(vk::Device device);
		~SamplerCache() = default;

		SamplerCache(SamplerCache&&) = default;

		const vk::Sampler& getSampler(const vk::SamplerCreateInfo& samplerInfo);
	};
}  // namespace Vulkan
