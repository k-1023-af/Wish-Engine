#include "Object3d.hlsli"

//struct PixelShaderInput
//{
//    float32_t4 position : SV_POSITION;
//    float32_t2 texcoord : TEXCOORD0;
//};
//
//Texture2D<float32_t4> gTexture : register(t0);
//SamplerState gSampler : register(s0);
//
//ConstantBuffer<Material> gMaterial : register(b1);
//
//PixelShaderOutput main(VertexShaderOutput input)
//{
//    PixelShaderOutput output;
//    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
//    output.color = gMaterial.color * textureColor;
//    return output;
//}


struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
struct Material
{
    float4 color;
};

//Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
//ConstantBuffer<Material> gMaterial : register(b1);

ConstantBuffer<Material> gMaterial0 : register(b0);
ConstantBuffer<Material> gMaterial1 : register(b1);
ConstantBuffer<Material> gMaterial2 : register(b2);
Texture2D<float4> gTexture0 : register(t3);
Texture2D<float4> gTexture1 : register(t4);


PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = textureColor * gMaterial.color;
    
    return output;
}

