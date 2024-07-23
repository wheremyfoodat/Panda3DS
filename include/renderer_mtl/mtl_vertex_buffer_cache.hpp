#pragma once

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
        reset();
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

    BufferHandle get(const void* data, size_t size) {
        // If the vertex buffer is too large, just create a new one
        if (ptr + size > CACHE_BUFFER_SIZE) {
            MTL::Buffer* newBuffer = device->newBuffer(data, size, MTL::ResourceStorageModeShared);
            newBuffer->setLabel(toNSString("Additional vertex buffer"));
            additionalAllocations.push_back(newBuffer);
            Helpers::warn("Vertex buffer doesn't have enough space, creating a new buffer");

            return BufferHandle{newBuffer, 0};
        }

        // Copy the data into the buffer
        memcpy((char*)buffer->contents() + ptr, data, size);

        size_t oldPtr = ptr;
        ptr += size;

        return BufferHandle{buffer, oldPtr};
    }

    void reset() {
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
