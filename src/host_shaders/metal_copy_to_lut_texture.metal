#include <metal_stdlib>
using namespace metal;

constant ushort lutTextureWidth [[function_constant(0)]];

// The copy is done in a vertex shader instead of a compute kernel, since dispatching compute would require ending the render pass
vertex void vertexCopyToLutTexture(uint vid [[vertex_id]], texture1d_array<ushort, access::write> out [[texture(0)]], constant ushort* data [[buffer(0)]]) {
    out.write(data[vid], vid % lutTextureWidth, vid / lutTextureWidth);
}
