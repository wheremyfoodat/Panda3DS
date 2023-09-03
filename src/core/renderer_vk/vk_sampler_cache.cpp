#include "renderer_vk/vk_sampler_cache.hpp"

#include <vulkan/vulkan_hash.hpp>

#include "helpers.hpp"

namespace Vulkan {

	SamplerCache::SamplerCache(vk::Device device) : device(device) {}

	const vk::Sampler& SamplerCache::getSampler(const vk::SamplerCreateInfo& samplerInfo) {
		const std::size_t samplerHash = std::hash<vk::SamplerCreateInfo>()(samplerInfo);

		// Cache hit
		if (samplerMap.contains(samplerHash)) {
			return samplerMap.at(samplerHash).get();
		}

		if (auto createResult = device.createSamplerUnique(samplerInfo); createResult.result == vk::Result::eSuccess) {
			return (samplerMap[samplerHash] = std::move(createResult.value)).get();
		} else {
			Helpers::panic("Error creating sampler: %s\n", vk::to_string(createResult.result).c_str());
		}
	}

	std::optional<SamplerCache> SamplerCache::create(vk::Device device) {
		SamplerCache newSamplerCache(device);

		return {std::move(newSamplerCache)};
	}
}  // namespace Vulkan