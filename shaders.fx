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

#define DIRECT_LIGHTS_COUNT 1
#define POINT_LIGHTS_COUNT  1

cbuffer cbChangesEachFrame : register(b2)
{
    // Transformations
    matrix World;
    float4 MeshColor;

    // Light sources
    float4 AmbientLight;
    float4 DirectLightDirs[DIRECT_LIGHTS_COUNT];
    float4 DirectLightColors[DIRECT_LIGHTS_COUNT];
    float4 PointLightDirs[POINT_LIGHTS_COUNT];
    float4 PointLightColors[POINT_LIGHTS_COUNT];
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

float4 DiffuseBrdf(float3 normal, float3 lightDir, float4 lightColor)
{
    return saturate(dot(normal, lightDir) * lightColor);
}

float4 PsIllumSurf(PS_INPUT input) : SV_Target
{
    float4 color = 0;

    color += AmbientLight;
    for (int i = 0; i<DIRECT_LIGHTS_COUNT; i++)
        color += DiffuseBrdf(input.Norm, (float3)DirectLightDirs[i], DirectLightColors[i]);
    for (int i = 0; i < POINT_LIGHTS_COUNT; i++)
        // TODO: Proper point-light evaluation
        color += DiffuseBrdf(input.Norm, (float3)PointLightDirs[i], PointLightColors[i]);
    color.a = 1;

    //color *= MeshColor;
    color *= txDiffuse.Sample(samLinear, input.Tex);

    return color;
}

float4 PsEmissiveSurf(PS_INPUT input) : SV_Target
{
    return MeshColor;
}
