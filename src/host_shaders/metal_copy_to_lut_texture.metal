#include <metal_stdlib>
using namespace metal;

constant ushort lutTextureWidth [[function_constant(0)]];

// The copy is done in a vertex shader instead of a compute kernel, since dispatching compute would require ending the render pass
vertex void vertexCopyToLutTexture(uint vid [[vertex_id]], texture1d_array<float, access::write> out [[texture(0)]], constant float2* data [[buffer(0)]], constant uint& arrayOffset [[buffer(1)]]) {
    out.write(float4(data[vid], 0.0, 0.0), vid % lutTextureWidth, arrayOffset + vid / lutTextureWidth);
}
