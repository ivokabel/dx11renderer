#include "constants.hpp"

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

cbuffer cbChangesEachFrame : register(b2)
{
    // Transformations
    matrix World;
    float4 MeshColor;

    // Light sources
    float4 AmbientLight;
    float4 DirectLightDirs[DIRECT_LIGHTS_COUNT];
    float4 DirectLightLuminances[DIRECT_LIGHTS_COUNT];
    float4 PointLightPositions[POINT_LIGHTS_COUNT];
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

struct LightContribution
{
    float4 Diffuse;
    float4 Specular;
};

LightContribution EvalDirLight(float3 normal, float3 lightDir, float4 luminance)
{
    LightContribution result;

    result.Diffuse  = DiffuseBrdf(normal, lightDir) * luminance;
    result.Specular = float4(0, 0, 0, 0);

    return result;
}

LightContribution EvalPointLight(float3 surfPos, float3 normal, float3 lightPos, float4 intensity)
{
    float3 dir = lightPos - surfPos;
    float len = sqrt(dot(dir, dir));
    float3 dirNorm = dir / len;

    LightContribution result;

    result.Diffuse  = DiffuseBrdf(normal, dirNorm) * intensity / (len * len);
    result.Specular = float4(0, 0, 0, 0);

    return result;
}

float4 PsIllumSurf(PS_INPUT input) : SV_Target
{
    LightContribution lightContribs = { {0, 0, 0, 0}, {0, 0, 0, 0} };

    for (int i = 0; i < DIRECT_LIGHTS_COUNT; i++)
    {
        LightContribution contrib = EvalDirLight(input.Normal,
                                                 (float3)DirectLightDirs[i],
                                                 DirectLightLuminances[i]);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    for (int i = 0; i < POINT_LIGHTS_COUNT; i++)
    {
        LightContribution contrib = EvalPointLight((float3)input.PosWorld,
                                                   input.Normal,
                                                   (float3)PointLightPositions[i],
                                                   PointLightIntensities[i]);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    float4 diffuseTexture = { 1, 1, 1, 1 };
    diffuseTexture = txDiffuse.Sample(samLinear, input.Tex);

    float4 output =
          (AmbientLight + lightContribs.Diffuse) * diffuseTexture
        + lightContribs.Specular;

    output.a = 1;

    return output;
}

float4 PsEmissiveSurf(PS_INPUT input) : SV_Target
{
    return MeshColor;
}
