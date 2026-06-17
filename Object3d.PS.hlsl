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
    float4 color = gMaterial.color * textureColor;

    //if (gMaterial.enableLighting != 0) {
    //    float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
    //}
    //else {
    //    output.color = gMaterial.color * textureColor;
    //}
    
    if (gMaterial.enableLighting != 0)
    {
        //float3 normal = normalize(input.normal);
        //float NdotL = saturate(dot(normal, -gDirectionalLight.direction));
        //color.rgb *= gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;
        
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f, 2.0f);
        
        color.rgb *= gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
    }
    
    output.color = color;
    
    return output;
}

