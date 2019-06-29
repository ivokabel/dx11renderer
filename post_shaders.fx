
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

float4 Pass1PS(QUAD_VS_OUTPUT Input) : SV_TARGET
{
    float2 coords = Input.Tex;

    //coords.x = coords.x + 0.3f * sin(coords.y * 2.f * PI);

    float4 vColor = s0.Sample(LinearSampler, coords);

    //...

    return vColor;
}

float4 Pass2PS(QUAD_VS_OUTPUT Input) : SV_TARGET
{
    float4 vColor = s0.Sample(PointSampler, Input.Tex);

    //...

    return vColor;
}
