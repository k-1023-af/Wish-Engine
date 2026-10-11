//Particle.hlsli

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET;
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};