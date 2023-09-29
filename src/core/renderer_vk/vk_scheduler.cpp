#include "renderer_vk/vk_scheduler.hpp"
#include "renderer_vk/vk_instance.hpp"

namespace Vulkan {

static constexpr size_t NUM_GROW_STEP = 4;

Scheduler::Scheduler(const Instance& instance_) : instance(instance_) {
    const vk::Device device = instance.getDevice();
    const vk::StructureChain semaphoreChain = {
        vk::SemaphoreCreateInfo{},
        vk::SemaphoreTypeCreateInfoKHR{
            .semaphoreType = vk::SemaphoreType::eTimeline,
            .initialValue = 0,
        },
    };
    if (auto createResult = device.createSemaphoreUnique(semaphoreChain.get()); createResult.result == vk::Result::eSuccess) {
        timeline = std::move(createResult.value);
    } else {
        Helpers::panic("Unable to create timeline semaphore!\n");
    }

    const vk::CommandPoolCreateInfo poolCreateInfo = {
        .flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = instance.getQueueFamilyIndex(),
    };
    if (auto createResult = device.createCommandPoolUnique(poolCreateInfo); createResult.result == vk::Result::eSuccess) {
        cmdpool = std::move(createResult.value);
    } else {
        Helpers::panic("Unable to create command pool!\n");
    }

    switchCmdbuffer();
}

Scheduler::~Scheduler() = default;

void Scheduler::refresh() {
    gpuCounter = instance.getDevice().getSemaphoreCounterValueKHR(*timeline).value;
}

void Scheduler::submitWork(vk::Semaphore signal, vk::Semaphore wait) {
    submitCallback();

    const vk::CommandBuffer cmdbuf = getCmdBuf();
    if (cmdbuf.end() != vk::Result::eSuccess) {
        Helpers::panic("Unable to end current command buffer!\n");
        return;
    }

    const u64 signalValue = cpuCounter++;
    const u32 numSignalSemaphores = signal ? 2u : 1u;
    const std::array signalValues{signalValue, u64(0)};
    const std::array signalSemaphores{*timeline, signal};
    const u32 numWaitSemaphores = wait ? 1u : 0u;

    static constexpr std::array<vk::PipelineStageFlags, 2> waitStageMasks = {
        vk::PipelineStageFlagBits::eAllCommands,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
    };

    const vk::TimelineSemaphoreSubmitInfoKHR timelineSubmitInfo = {
        .signalSemaphoreValueCount = numSignalSemaphores,
        .pSignalSemaphoreValues = signalValues.data(),
    };

    const vk::SubmitInfo submitInfo = {
        .pNext = &timelineSubmitInfo,
        .waitSemaphoreCount = numWaitSemaphores,
        .pWaitSemaphores = &wait,
        .pWaitDstStageMask = waitStageMasks.data(),
        .commandBufferCount = 1u,
        .pCommandBuffers = &cmdbuf,
        .signalSemaphoreCount = numSignalSemaphores,
        .pSignalSemaphores = signalSemaphores.data(),
    };

    if (auto result = instance.getQueue().submit(submitInfo); result != vk::Result::eSuccess) [[unlikely]] {
        Helpers::panic("Error during %d\n", result);
    }

    switchCmdbuffer();
}

void Scheduler::setSubmitCallback(std::function<void()>&& callback) {
    submitCallback = std::move(callback);
}

void Scheduler::finish() {
    const u64 presubmitCounter = cpuCounter;
    submitWork();
    waitFor(presubmitCounter);
}

void Scheduler::waitFor(u64 counter) {
	if (counter == getCpuCounter()) {
		submitWork();
	}
    if (isFree(counter)) {
        return;
    }
    refresh();
    if (isFree(counter)) {
        return;
    }

    const vk::Semaphore semaphore = *timeline;
    const vk::SemaphoreWaitInfo waitInfo = {
        .semaphoreCount = 1u,
        .pSemaphores = &semaphore,
        .pValues = &counter,
    };

    const vk::Result result = instance.getDevice().waitSemaphores(waitInfo, std::numeric_limits<u64>::max());
    if (result != vk::Result::eSuccess) {
        Helpers::panic("Unable to wait for timeline semaphore: counter %lld result: %s\n", counter, vk::to_string(result).c_str());
    }
    refresh();
}

void Scheduler::switchCmdbuffer() {
    // Search for a free command buffer to use
    bool cmdbufferFound = false;
    for (size_t i = 0; i < cmdbuffers.size(); i++) {
        auto& cmdbuffer = cmdbuffers[i];
        if (isFree(cmdbuffer.lastUsedCounter)) {
            currentCmdbuf = i;
            cmdbufferFound = true;
            break;
        }
    }

    // If we cannot find any, grow the pool and pick the first of newlly allocated cmdbuffers
    if (!cmdbufferFound) {
        currentCmdbuf = static_cast<u32>(cmdbuffers.size());
        growNumBuffers();
    }

    // Mark the current cpu counter the buffer was used
    cmdbuffers[currentCmdbuf].lastUsedCounter = cpuCounter;

    // Ensure new command buffer is in the recording state
    const vk::CommandBufferBeginInfo beginInfo = {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };

    const vk::Result result = getCmdBuf().begin(beginInfo);
    if (result != vk::Result::eSuccess) [[unlikely]] {
        Helpers::panic("Unable to put command buffer in recording state!\n");
    }
}

void Scheduler::growNumBuffers() {
    const vk::CommandBufferAllocateInfo bufferAllocInfo = {
        .commandPool = *cmdpool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = NUM_GROW_STEP,
    };

    const auto [result, newBuffers] = instance.getDevice().allocateCommandBuffers(bufferAllocInfo);
    if (result != vk::Result::eSuccess) [[unlikely]] {
        Helpers::panic("Unable to allocate new command buffers!\n");
        return;
    }

    const size_t oldSize = cmdbuffers.size();
    cmdbuffers.resize(oldSize + NUM_GROW_STEP);

    std::transform(newBuffers.begin(), newBuffers.end(), cmdbuffers.begin() + oldSize,
                   [](vk::CommandBuffer cmdbuf) {
        return CommandBuffer{.cmdbuf = cmdbuf, .lastUsedCounter = 0};
    });
}

} // namespace Vulkan
