cbuffer Transformation : register(b0)
{
    float4x4 WVP;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    o.position = mul(float4(input.position, 1.0f), WVP);
    o.color = input.color;
    return o;
}