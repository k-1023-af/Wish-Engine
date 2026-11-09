//Particle.VS.hlsl

#include "Particle.hlsli"


StructuredBuffer<ParticleForGPU> gParticles : register(t0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    
    output.position = mul(input.position, gParticles[instanceId].WVP);
    output.texcoord = input.texcoord;
    
    output.normal = normalize(mul(input.normal, (float32_t3x3)gParticles[instanceId].World));
    return output;
}