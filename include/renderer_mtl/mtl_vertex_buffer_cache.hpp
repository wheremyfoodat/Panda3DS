#pragma once

#include <map>
#include "pica_to_mtl.hpp"

using namespace PICA;

namespace Metal {

struct BufferHandle {
    MTL::Buffer* buffer;
    size_t offset;
};

// 64MB buffer for caching vertex data
#define CACHE_BUFFER_SIZE 64 * 1024 * 1024

class VertexBufferCache {
public:
    VertexBufferCache() = default;

    ~VertexBufferCache() {
        clear();
    }

    void set(MTL::Device* dev) {
        device = dev;
        buffer = device->newBuffer(CACHE_BUFFER_SIZE, MTL::ResourceStorageModeShared);
        buffer->setLabel(toNSString("Shared vertex buffer"));
    }

    void endFrame() {
        ptr = 0;
        for (auto buffer : additionalAllocations) {
            buffer->release();
        }
        additionalAllocations.clear();
    }

    BufferHandle get(const std::span<const PICA::Vertex>& vertices) {
        // If the vertex buffer is too large, just create a new one
        if (ptr + vertices.size_bytes() > CACHE_BUFFER_SIZE) {
            MTL::Buffer* newBuffer = device->newBuffer(vertices.data(), vertices.size_bytes(), MTL::ResourceStorageModeShared);
            newBuffer->setLabel(toNSString("Additional vertex buffer"));
            additionalAllocations.push_back(newBuffer);
            Helpers::warn("Vertex buffer doesn't have enough space, creating a new buffer");

            return BufferHandle{newBuffer, 0};
        }

        // Copy the data into the buffer
        memcpy((char*)buffer->contents() + ptr, vertices.data(), vertices.size_bytes());

        size_t oldPtr = ptr;
        ptr += vertices.size_bytes();

        return BufferHandle{buffer, oldPtr};
    }

    void clear() {
        endFrame();
        buffer->release();
    }

private:
    MTL::Buffer* buffer;
    size_t ptr = 0;
    std::vector<MTL::Buffer*> additionalAllocations;

    MTL::Device* device;
};

} // namespace Metal
