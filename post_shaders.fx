
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

QUAD_VS_OUTPUT QuadVS(QUAD_VS_INPUT Input)
{
    QUAD_VS_OUTPUT Output;
    Output.Pos = Input.Pos;
    Output.Tex = Input.Tex;
    return Output;
}

cbuffer cbBloom
{
    float2 g_avSampleOffsets[15];
    float4 g_avSampleWeights[15];
}

float4 Pass1PS(QUAD_VS_OUTPUT Input) : SV_TARGET
{
    float2 texCoords = Input.Tex;
    //texCoords.x = texCoords.x + 0.3f * sin(texCoords.y * 2.f * PI);

    // Convolute by blurring filter
    float4 convolution = 0.0f;
    float4 color = 0.0f;
    float2 samplePos;
    for (int i = 0; i < 15; i++)
    {
        samplePos = texCoords + g_avSampleOffsets[i];
        color = s0.Sample(LinearSampler, samplePos);

        convolution += g_avSampleWeights[i] * color;
    }
    return convolution * 0.01f;
}

float4 Pass2PS(QUAD_VS_OUTPUT Input) : SV_TARGET
{
    float4 color = s0.Sample(LinearSampler, Input.Tex);

    //...

    return color;
}
