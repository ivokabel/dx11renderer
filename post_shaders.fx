
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

Texture2D Texture0 : register(t0);
Texture2D Texture1 : register(t1);

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
    float4 input = 0.0f;
    float2 samplePos;
    for (int i = 0; i < 15; i++)
    {
        samplePos = Input.Tex + BlurOffsets[i];
        input = Texture0.Sample(LinearSampler, samplePos);
        convolution += BlurWeights[i] * input;
    }
    return convolution;
}

float4 FinalPassPS(QUAD_VS_OUTPUT Input) : SV_TARGET
{
    float4 render = Texture0.Sample(PointSampler, Input.Tex);
    float4 bloom = Texture1.Sample(LinearSampler, Input.Tex);

    const float bloomStrength = 0.010f;

    return render * (1 - bloomStrength) + bloom * bloomStrength;
}


float4 DebugPS(QUAD_VS_OUTPUT Input) : SV_TARGET
{
    float4 color = Texture0.Sample(PointSampler, Input.Tex);

    //const float green = 0.2f;
    //return color * (1 - green) + float4(0., 1., 0., 1.) * green;

    //float4 isColorNan = isnan(color);
    //return float4(
    //    isColorNan.r ? 1 : 0,
    //    isColorNan.g ? 1 : 0,
    //    isColorNan.b ? 1 : 0,
    //    1);

    //return float4(
    //    IsNAN(color.r) ? 1 : 0,
    //    IsNAN(color.g) ? 1 : 0,
    //    IsNAN(color.b) ? 1 : 0,
    //    1);

    //return float4(
    //    isfinite(color.r) ? 0 : 1,
    //    isfinite(color.g) ? 0 : 1,
    //    isfinite(color.b) ? 0 : 1,
    //    1);

    //return float4(0.7f * color.rgb, 1);

    return float4(0.9f * pow(color.rgb, 0.85f.xxx), 1); // simple tonemapping
}
