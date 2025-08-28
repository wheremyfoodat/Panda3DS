#pragma once

#include <Metal/Metal.hpp>

#include "helpers.hpp"

namespace Metal {
	struct RenderState {
		MTL::RenderPipelineState* renderPipelineState = nullptr;
		MTL::DepthStencilState* depthStencilState = nullptr;
		MTL::Texture* textures[3] = {nullptr};
		MTL::SamplerState* samplerStates[3] = {nullptr};
	};

	class CommandEncoder {
	  public:
		void newRenderCommandEncoder(MTL::RenderCommandEncoder* rce) {
			renderCommandEncoder = rce;

			// Reset the render state
			renderState = RenderState{};
		}

		// Resource binding
		void setRenderPipelineState(MTL::RenderPipelineState* renderPipelineState) {
			if (renderPipelineState != renderState.renderPipelineState) {
				renderCommandEncoder->setRenderPipelineState(renderPipelineState);
				renderState.renderPipelineState = renderPipelineState;
			}
		}

		void setDepthStencilState(MTL::DepthStencilState* depthStencilState) {
			if (depthStencilState != renderState.depthStencilState) {
				renderCommandEncoder->setDepthStencilState(depthStencilState);
				renderState.depthStencilState = depthStencilState;
			}
		}

		void setFragmentTexture(MTL::Texture* texture, u32 index) {
			if (texture != renderState.textures[index]) {
				renderCommandEncoder->setFragmentTexture(texture, index);
				renderState.textures[index] = texture;
			}
		}

		void setFragmentSamplerState(MTL::SamplerState* samplerState, u32 index) {
			if (samplerState != renderState.samplerStates[index]) {
				renderCommandEncoder->setFragmentSamplerState(samplerState, index);
				renderState.samplerStates[index] = samplerState;
			}
		}

	  private:
		MTL::RenderCommandEncoder* renderCommandEncoder = nullptr;

		RenderState renderState;
	};
}  // namespace Metal
