Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

cbuffer cbNeverChanges : register(b0)
{
    matrix View;
};

cbuffer cbChangeOnResize : register(b1)
{
    matrix Projection;
};

cbuffer cbChangesEachFrame : register(b2)
{
    matrix World;
    float4 MeshColor;
    float4 LightDirs[2];
    float4 LightColors[2];
};


struct VS_INPUT
{
    float4 Pos      : POSITION;
    float3 Norm     : NORMAL;
    float2 Tex      : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos      : SV_POSITION;
    float3 Norm     : TEXCOORD0;
    float2 Tex      : TEXCOORD1;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;

    output.Pos = mul(input.Pos, World);
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);

    output.Norm = mul(input.Norm, World);

    output.Tex = input.Tex;

    return output;
}

float4 PsIllumSurf(PS_INPUT input) : SV_Target
{
    float4 color = 0;
    for (int i = 0; i<2; i++)
        // Naive Lambert illumination
        color += saturate(dot((float3)LightDirs[i], input.Norm) * LightColors[i]);
    color.a = 1;

    //color *= MeshColor;
    color *= txDiffuse.Sample(samLinear, input.Tex);

    return color;
}

float4 PsEmissiveSurf(PS_INPUT input) : SV_Target
{
    return MeshColor;
}
