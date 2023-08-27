#pragma once

#include <optional>
#include <unordered_map>

#include "helpers.hpp"
#include "vk_api.hpp"

namespace Vulkan {
	// Implements a simple pool of reusable sampler objects
	class SamplerCache {
	  private:
		const vk::Device device;

		std::unordered_map<std::size_t, vk::UniqueSampler> samplerMap;

		explicit SamplerCache(vk::Device device);

	  public:
		~SamplerCache() = default;

		SamplerCache(SamplerCache&&) = default;

		const vk::Sampler& getSampler(const vk::SamplerCreateInfo& samplerInfo);

		static std::optional<SamplerCache> create(vk::Device device);
	};
}  // namespace Vulkan