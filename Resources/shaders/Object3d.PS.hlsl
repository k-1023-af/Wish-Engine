//Object3d.PS.hlsl

#include "Object3d.hlsli"

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
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);


PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord,0.0f, 1.0f), gMaterial.uvTransform);
    //float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    float4 color = gMaterial.color * textureColor;
    
    if (gMaterial.enableLighting != 0)
    {
        float3 N = normalize(input.normal);
        float3 L = normalize(-gDirectionalLight.direction);
        
        //float halfLambert = NdotL * 0.5f + 0.5f;
        float NdotL = dot(N, L);
        //float cos = pow(halfLambert, power);
        
        float cosValue;
        if (gMaterial.lightingMode == 0)
        {
            cosValue = saturate(NdotL);
            //output.color = gMaterial.color * textureColor * gDirectionalLight.color * cosValue * gDirectionalLight.intensity;
            
        }
        else
        {
            float halfLambert = NdotL * 0.5f + 0.5f;
            //cos = pow(saturate(halfLambert), gMaterial.halfLambertPower);
            cosValue = pow(halfLambert, 2.0f);
        }
        //output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cosValue * gDirectionalLight.intensity;
        color.rgb = color.rgb * gDirectionalLight.color.rgb * cosValue * gDirectionalLight.intensity;
    }
    output.color = color;
    
    return output;
}