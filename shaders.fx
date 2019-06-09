#include "constants.hpp"

static const float PI = 3.14159265f;


Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

cbuffer cbNeverChanges : register(b0)
{
    matrix View;
    float4 CameraPos;
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

    output.Normal = (float3)mul(input.Normal, World);

    output.Tex = input.Tex;

    return output;
}


float4 ThetaCos(float3 normal, float3 lightDir)
{
    return max(dot(normal, lightDir), 0.);
}


struct LightContrib
{
    float4 Diffuse;
    float4 Specular;
};


float4 Specular(float3 lightDir, float3 normal, float3 viewDir)
{
    // Phong
    //const float3 reflDir = normalize(reflect(-lightDir, normal));
    //const float viewDotRefl = max(dot(viewDir, reflDir), 0);
    //return pow(viewDotRefl, 500.0);

    // Blinn-Phong
    if (dot(lightDir, normal) < 0)
        // Light is below surface - no contribution
        return float4(0, 0, 0, 1);
    else
    {
        const float3 halfwayRaw = lightDir + viewDir;
        const float  halfwayLen = length(halfwayRaw);
        const float3 halfway = (halfwayLen > 0.001f) ? (halfwayRaw / halfwayLen) : normal;
        const float  halfwayCos = max(dot(halfway, normal), 0);
        return pow(halfwayCos, 800.0);
    }
}


LightContrib DirLightContrib(float3 lightDir, float3 normal, float3 viewDir, float4 luminance)
{
    const float thetaCos = ThetaCos(normal, lightDir);

    LightContrib contrib;
    contrib.Diffuse  = thetaCos * luminance;
    contrib.Specular = Specular(lightDir, normal, viewDir) * luminance;
    return contrib;
}


LightContrib PointLightContrib(float3 surfPos, float3 lightPos, float3 normal, float3 viewDir, float4 intensity)
{
    const float3 dirRaw = lightPos - surfPos;
    const float  len = length(dirRaw);
    const float3 lightDir = dirRaw / len;
    const float  lenSqr = len * len;

    const float thetaCos = ThetaCos(normal, lightDir);

    LightContrib contrib;
    contrib.Diffuse  = thetaCos * intensity / lenSqr;
    contrib.Specular = Specular(lightDir, normal, viewDir) * intensity / lenSqr;
    return contrib;
}


float4 PsIllumSurf(PS_INPUT input) : SV_Target
{
    const float3 normal  = normalize(input.Normal); // normal is interpolated - renormalize 
    const float3 viewDir = normalize((float3)CameraPos - (float3)input.PosWorld);

    LightContrib lightContribs = { {0, 0, 0, 0}, {0, 0, 0, 0} };

    for (int i = 0; i < DIRECT_LIGHTS_COUNT; i++)
    {
        LightContrib contrib = DirLightContrib((float3)DirectLightDirs[i],
                                               normal,
                                               viewDir,
                                               DirectLightLuminances[i]);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    for (int i = 0; i < POINT_LIGHTS_COUNT; i++)
    {
        LightContrib contrib = PointLightContrib((float3)input.PosWorld,
                                                 (float3)PointLightPositions[i],
                                                 normal,
                                                 viewDir,
                                                 PointLightIntensities[i]);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    float4 diffuseTexture = { 1, 1, 1, 1 };
    diffuseTexture = txDiffuse.Sample(samLinear, input.Tex);

    float4 output =
          (AmbientLight + lightContribs.Diffuse) * diffuseTexture
        + lightContribs.Specular
        ;

    output.a = 1;

    return output;
}


float4 PsEmissiveSurf(PS_INPUT input) : SV_Target
{
    return MeshColor;
}
