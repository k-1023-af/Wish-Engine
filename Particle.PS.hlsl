//Particle.PS.hlsl

#include "Particle.hlsli"

struct Material
{
    float4 color;
    int32_t enableLighting;
    int32_t lightingMode;
    float halfLambertPower;
    float padding; // Padding to align the next member to a 16-byte boundary
    float4x4 uvTransform;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

ConstantBuffer<Material> gMaterial : register(b0);
//ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);


PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    //float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    output.color = gMaterial.color * textureColor * input.color;
    
    if (output.color.a == 0.0f) {
        discard;
    }
    return output;
}