static const float PI = 3.14159265f;


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
#define POINT_LIGHTS_COUNT  2

cbuffer cbChangesEachFrame : register(b2)
{
    // Transformations
    matrix World;
    float4 MeshColor;

    // Light sources
    float4 AmbientLight;
    float4 DirectLightDirs[DIRECT_LIGHTS_COUNT];
    float4 DirectLightLuminances[DIRECT_LIGHTS_COUNT];
    float4 PointLightDirs[POINT_LIGHTS_COUNT];
    float4 PointLightIntensities[POINT_LIGHTS_COUNT];
};


struct VS_INPUT
{
    float4 Pos      : POSITION;
    float3 Normal   : NORMAL;
    float2 Tex      : TEXCOORD0;
};

struct PS_INPUT
{
    float4 PosProj  : SV_POSITION;
    float4 PosWorld : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 Tex      : TEXCOORD2;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;

    output.PosWorld = mul(input.Pos, World);

    output.PosProj = mul(output.PosWorld, View);
    output.PosProj = mul(output.PosProj, Projection);

    output.Normal = mul(input.Normal, World);

    output.Tex = input.Tex;

    return output;
}

float4 DiffuseBrdf(float3 normal, float3 lightDir)
{
    return max(dot(normal, lightDir), 0.);
}

float4 EvalPointLight(float3 surfPos, float3 normal, float3 lightPos, float4 intensity)
{
    float3 dir = lightPos - surfPos;
    float len = sqrt(dot(dir, dir));
    float3 dirNorm = dir / len;

    return DiffuseBrdf(normal, dirNorm) * intensity / (len * len);
}

float4 PsIllumSurf(PS_INPUT input) : SV_Target
{
    float4 color = 0;

    color += AmbientLight;

    for (int i = 0; i<DIRECT_LIGHTS_COUNT; i++)
        color += DiffuseBrdf(input.Normal, (float3)DirectLightDirs[i]) * DirectLightLuminances[i];

    for (int i = 0; i < POINT_LIGHTS_COUNT; i++)
        color += EvalPointLight((float3)input.PosWorld,
                                input.Normal,
                                (float3)PointLightDirs[i],
                                PointLightIntensities[i]);

    color *= txDiffuse.Sample(samLinear, input.Tex);

    color.a = 1;

    return color;
}

float4 PsEmissiveSurf(PS_INPUT input) : SV_Target
{
    return MeshColor;
}
