#pragma once

#include "pica_to_mtl.hpp"

using namespace PICA;

namespace Metal {

struct DepthStencilHash {
    bool depthWrite;
    u8 depthFunc;
};

class DepthStencilCache {
public:
    DepthStencilCache() = default;

    ~DepthStencilCache() {
        clear();
    }

    void set(MTL::Device* dev) {
        device = dev;
    }

    MTL::DepthStencilState* get(DepthStencilHash hash) {
        u8 intHash = hash.depthWrite | (hash.depthFunc << 1);
        auto& depthStencilState = depthStencilCache[intHash];
        if (!depthStencilState) {
            MTL::DepthStencilDescriptor* desc = MTL::DepthStencilDescriptor::alloc()->init();
            desc->setDepthWriteEnabled(hash.depthWrite);
            desc->setDepthCompareFunction(toMTLCompareFunc(hash.depthFunc));

            depthStencilState = device->newDepthStencilState(desc);

            desc->release();
        }

        return depthStencilState;
    }

    void clear() {
        for (auto& pair : depthStencilCache) {
            pair.second->release();
        }
        depthStencilCache.clear();
    }

private:
    std::unordered_map<u8, MTL::DepthStencilState*> depthStencilCache;

    MTL::Device* device;
};

} // namespace Metal
