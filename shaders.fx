
cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
}

struct VS_INPUT
{
    float4 Pos      : POSITION;
    float4 Color    : COLOR;
};

struct PS_INPUT
{
    float4 Pos      : SV_POSITION;
    float4 Color    : COLOR;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul(input.Pos, World);
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);
    output.Color = input.Color;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
    //return float4(0.80f, 0.50f, 0.11f, 1.0f);
    return input.Color;
}
