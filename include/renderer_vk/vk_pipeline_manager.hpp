#pragma once

#include "vk_pipeline.hpp"
#include "vk_descriptor_heap.hpp"
#include "vk_descriptor_update_batch.hpp"
#include <unordered_map>
#include <memory>

namespace Vulkan {

class Instance;
class Scheduler;
class DescriptorHeap;

enum BufferKind : u32 {
	VsSupport,
	FsSupport,
	Count,
};

class PipelineManager {
public:
    PipelineManager(const Instance& instance, Scheduler& scheduler, vk::Buffer streamBuffer);
    ~PipelineManager();

	void beginUpdate() {
		currentBinding = 0;
		currentTextureSet = textureHeap->allocateDescriptorSet().value();
	}

	void pushImage(vk::ImageView imageView, vk::Sampler sampler) {
		updateBatch.addImageSampler(currentTextureSet, currentBinding++, 0, imageView, sampler);
	}

	void flushUpdates() {
		isDirty = true;
		updateBatch.flush();
	}

	void updateRange(BufferKind kind, u32 offset) {
		dynamicOffsets[kind] = offset;
	}

	void bindPipeline(const PipelineInfo& info);

private:
  const Instance& instance;
  Scheduler& scheduler;
  vk::UniquePipelineLayout picaPipelineLayout;
  vk::UniqueShaderModule picaVertexShaderModule;
  vk::UniqueShaderModule picaFragmentShaderModule;
  std::unordered_map<size_t, std::unique_ptr<Vulkan::Pipeline>> pipelineMap;
  std::unique_ptr<Vulkan::DescriptorHeap> bufferHeap, textureHeap;
  Vulkan::DescriptorUpdateBatch updateBatch;
  vk::DescriptorSet bufferSet{};
  vk::DescriptorSet currentTextureSet{};
  u32 currentBinding{};
  std::array<u32, BufferKind::Count> dynamicOffsets{};
  PipelineInfo currentInfo{};
  bool isDirty{true};
};

}
