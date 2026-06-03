#include "Object3d.hlsli"

struct Material{
    float4 color;
};
ConstantBuffer<Material> gMaterial : register(b1);

Texture2D<float> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = gMaterial.color * textureColor;
    
    return output;
}

