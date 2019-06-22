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
    float4 vColor = s0.Sample(PointSampler, Input.Tex);

    // debug
    vColor.g = 1.0f;
    vColor.a = 1.0f;

    return vColor;
}
