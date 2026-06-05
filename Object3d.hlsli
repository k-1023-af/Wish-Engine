//Object3d.hlsli

struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PixelShaderOutput {
    float4 color : SV_TARGET;
    float2 texcoord : TEXCOORD0;
};

//struct VertexShaderOutput
//{
//    float32_t4 position : SV_POSITION;
//    float32_t2 texcoord : TEXCOORD0;
//};

//struct PixelShaderOutput
//{
//    float32_t4 color : SV_TARGET;
//    float32_t2 texcoord : TEXCOORD0;
//};