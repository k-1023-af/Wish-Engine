//Object3d.PS.hlsl

#include "Object3d.hlsli"

struct Material
{
    float4 color;
    int32_t enableLighting;
    //float3 padding;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

//ConstantBuffer<Material> gMaterial0 : register(b1);
//ConstantBuffer<Material> gMaterial1 : register(b2);
//ConstantBuffer<Material> gMaterial2 : register(b3);
//Texture2D<float4> gTexture0 : register(t0);
//Texture2D<float4> gTexture1 : register(t1);


PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    //output.color = textureColor * gMaterial.color;
    
    if (gMaterial.enableLighting != 0) {
        float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
    }
    else {
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}

