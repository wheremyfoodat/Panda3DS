#include <metal_stdlib>
using namespace metal;

constant ushort lutTextureWidth [[function_constant(0)]];

vertex void vertexCopyToLUTTexture(uint vid [[vertex_id]], constant ushort* data [[buffer(0)]], texture1d_array<ushort, access::write> out [[texture(0)]]) {
    out.write(data[vid], vid % lutTextureWidth, vid / lutTextureWidth);
}
