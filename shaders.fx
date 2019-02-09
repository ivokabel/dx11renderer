
cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
}

struct VS_OUTPUT
{
    float4 Pos      : SV_POSITION;
    float4 Color    : COLOR0;
};

VS_OUTPUT VS(float4 Pos : POSITION, float4 Color : COLOR)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    output.Pos = mul(Pos, World);
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);
    output.Color = Color;
    return output;
}

float4 PS(VS_OUTPUT input) : SV_Target
{
    //return float4(0.80f, 0.50f, 0.11f, 1.0f);
    return input.Color;
}

