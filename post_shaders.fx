
static const float PI = 3.14159265f;

SamplerState PointSampler  : register (s0);
SamplerState LinearSampler : register (s1);

struct QUAD_VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct QUAD_VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

Texture2D s0 : register(t0);
Texture2D s1 : register(t1);

QUAD_VS_OUTPUT QuadVS(QUAD_VS_INPUT Input)
{
    QUAD_VS_OUTPUT Output;
    Output.Pos = Input.Pos;
    Output.Tex = Input.Tex;
    return Output;
}

cbuffer cbBloom
{
    float2 BlurOffsets[15];
    float4 BlurWeights[15];
}

float4 BloomPS(QUAD_VS_OUTPUT Input) : SV_TARGET
{
    // Convolute by blurring filter in one dimension
    float4 convolution = 0.0f;
    float4 color = 0.0f;
    float2 samplePos;
    for (int i = 0; i < 15; i++)
    {
        samplePos = Input.Tex + BlurOffsets[i];
        color = s0.Sample(LinearSampler, samplePos);
        convolution += BlurWeights[i] * color;
    }
    return convolution;
}

float4 FinalPassPS(QUAD_VS_OUTPUT Input) : SV_TARGET
{
    float4 color = s0.Sample(PointSampler, Input.Tex);
    float4 bloom = s1.Sample(LinearSampler, Input.Tex);

    //return color;
    //return bloom;

    return color * 0.99f + bloom * 0.01f;
}
