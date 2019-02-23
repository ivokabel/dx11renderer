
cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;

    float4 LightDirs[2];
    float4 LightColors[2];
    float4 OutputColor;
}

struct VS_INPUT
{
    float4 Pos      : POSITION;
    float3 Norm     : NORMAL;
};

struct PS_INPUT
{
    float4 Pos      : SV_POSITION;
    float3 Norm     : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;

    output.Pos = mul(input.Pos, World);
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);

    output.Norm = mul(input.Norm, World);

    return output;
}

float4 PSIllum(PS_INPUT input) : SV_Target
{
    float4 color = 0;
    for (int i = 0; i<2; i++)
        // Naive Lambert illumination
        color += saturate(dot((float3)LightDirs[i], input.Norm) * LightColors[i]);
    color.a = 1;
    return color;
}

float4 PSSolid(PS_INPUT input) : SV_Target
{
    return OutputColor;
}
