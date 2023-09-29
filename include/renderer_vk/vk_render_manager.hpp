#pragma once

#include <unordered_map>

#include "renderer_vk/vk_surfaces.hpp"
#include "vk_api.hpp"
#include "math_util.hpp"
#include "PICA/regs.hpp"

namespace Vulkan {

class Instance;
class Scheduler;
class Swapchain;

class RenderManager {
    const Instance& instance;
    Scheduler& scheduler;
    vk::UniqueRenderPass renderPasses[6][5];
    std::unordered_map<u64, vk::UniqueFramebuffer> framebuffers;
    vk::ImageView currentColorView{};
    vk::ImageView currentDepthView{};
    vk::RenderPass currentRenderPass{};
    vk::Framebuffer currentFb{};
    bool doClear{};
    bool isRendering{};

  public:
    RenderManager(const Instance& instance, Scheduler& scheduler);
    ~RenderManager();

    static vk::UniqueRenderPass createRenderPass(vk::Device device, vk::Format color,
                                                 vk::Format depth, bool isPresent);

	void beginRendering(const std::optional<Vulkan::ColourBuffer>& color,
						const std::optional<Vulkan::DepthBuffer>& depth,
						Math::Rectangle<u32> renderArea);
    void beginRendering(Swapchain& swapchain);

    void endRendering();

	vk::RenderPass getCurrentRenderPass() const {
		return currentRenderPass;
	}

    vk::RenderPass getRenderPass(PICA::ColorFmt colorFmt = PICA::ColorFmt::Invalid,
                                 PICA::DepthFmt depthFmt = PICA::DepthFmt::Invalid);

    void reset() {
        framebuffers.clear();
    }

  private:
    vk::UniqueFramebuffer createFramebuffer(vk::ImageView colorView, vk::ImageView depthView,
                                            vk::RenderPass renderPass, Math::uvec2 size) const;

};

} // namespace Vulkan
