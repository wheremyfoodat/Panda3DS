#pragma once

#include <map>

#include "pica_to_mtl.hpp"

using namespace PICA;

namespace Metal {
	struct DepthStencilHash {
		u32 stencilConfig;
		u16 stencilOpConfig;
		bool depthStencilWrite;
		u8 depthFunc;
	};

	class DepthStencilCache {
	  public:
		DepthStencilCache() = default;

		~DepthStencilCache() { reset(); }

		void set(MTL::Device* dev) { device = dev; }

		MTL::DepthStencilState* get(DepthStencilHash hash) {
			u64 intHash =
				((u64)hash.depthStencilWrite << 56) | ((u64)hash.depthFunc << 48) | ((u64)hash.stencilConfig << 16) | (u64)hash.stencilOpConfig;
			auto& depthStencilState = depthStencilCache[intHash];
			if (!depthStencilState) {
				MTL::DepthStencilDescriptor* desc = MTL::DepthStencilDescriptor::alloc()->init();
				desc->setDepthWriteEnabled(hash.depthStencilWrite);
				desc->setDepthCompareFunction(toMTLCompareFunc(hash.depthFunc));

				const bool stencilEnable = Helpers::getBit<0>(hash.stencilConfig);
				MTL::StencilDescriptor* stencilDesc = nullptr;
				if (stencilEnable) {
					const u8 stencilFunc = Helpers::getBits<4, 3>(hash.stencilConfig);
					const u8 stencilRefMask = Helpers::getBits<24, 8>(hash.stencilConfig);

					const u32 stencilBufferMask = hash.depthStencilWrite ? Helpers::getBits<8, 8>(hash.stencilConfig) : 0;

					const u8 stencilFailOp = Helpers::getBits<0, 3>(hash.stencilOpConfig);
					const u8 depthFailOp = Helpers::getBits<4, 3>(hash.stencilOpConfig);
					const u8 passOp = Helpers::getBits<8, 3>(hash.stencilOpConfig);

					stencilDesc = MTL::StencilDescriptor::alloc()->init();
					stencilDesc->setStencilFailureOperation(toMTLStencilOperation(stencilFailOp));
					stencilDesc->setDepthFailureOperation(toMTLStencilOperation(depthFailOp));
					stencilDesc->setDepthStencilPassOperation(toMTLStencilOperation(passOp));
					stencilDesc->setStencilCompareFunction(toMTLCompareFunc(stencilFunc));
					stencilDesc->setReadMask(stencilRefMask);
					stencilDesc->setWriteMask(stencilBufferMask);

					desc->setFrontFaceStencil(stencilDesc);
					desc->setBackFaceStencil(stencilDesc);
				}

				depthStencilState = device->newDepthStencilState(desc);

				desc->release();
				if (stencilDesc) {
					stencilDesc->release();
				}
			}

			return depthStencilState;
		}

		void reset() {
			for (auto& pair : depthStencilCache) {
				pair.second->release();
			}
			depthStencilCache.clear();
		}

	  private:
		std::map<u64, MTL::DepthStencilState*> depthStencilCache;
		MTL::Device* device;
	};
}  // namespace Metal
