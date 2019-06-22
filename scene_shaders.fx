#include "constants.hpp"

static const float PI = 3.14159265f;


Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

cbuffer cbNeverChanges : register(b0)
{
    matrix ViewMtrx;
    float4 CameraPos;
};

cbuffer cbChangeOnResize : register(b1)
{
    matrix ProjectionMtrx;
};

cbuffer cbChangesEachFrame : register(b2)
{
    // Transformations
    matrix WorldMtrx;
    float4 MeshColor;

    // Light sources
    float4 AmbientLightLuminance;
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

    output.PosWorld = mul(input.Pos, WorldMtrx);

    output.PosProj = mul(output.PosWorld, ViewMtrx);
    output.PosProj = mul(output.PosProj, ProjectionMtrx);

    output.Normal = (float3)mul(input.Normal, WorldMtrx);

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


float4 Diffuse()
{
    return 1 / PI;
};


float4 Specular(float3 lightDir, float3 normal, float3 viewDir, float specPower)
{
    // Blinn-Phong
    if (dot(lightDir, normal) < 0)
        return float4(0, 0, 0, 1); // Light is below surface - no contribution
    else
    {
        const float3 halfwayRaw = lightDir + viewDir;
        const float  halfwayLen = length(halfwayRaw);
        const float3 halfway = (halfwayLen > 0.001f) ? (halfwayRaw / halfwayLen) : normal;
        const float  halfwayCos = max(dot(halfway, normal), 0);
        const float  energyConserv = (2 + specPower) / (8 * PI);
        return pow(halfwayCos, specPower) * energyConserv;
    }
}


LightContrib AmbLightContrib(float4 luminance)
{
    LightContrib contrib;
    contrib.Diffuse  = luminance;
    contrib.Specular = luminance; // estimate based on assumption that full specular lobe integrates to 1
    return contrib;
}


LightContrib DirLightContrib(float3 lightDir,
                             float3 normal,
                             float3 viewDir,
                             float4 luminance,
                             float specPower)
{
    const float thetaCos = ThetaCos(normal, lightDir);

    LightContrib contrib;
    contrib.Diffuse  = Diffuse() * thetaCos * luminance;
    contrib.Specular = Specular(lightDir, normal, viewDir, specPower) * luminance;
    return contrib;
}


LightContrib PointLightContrib(float3 surfPos,
                               float3 lightPos,
                               float3 normal,
                               float3 viewDir,
                               float4 intensity,
                               float specPower)
{
    const float3 dirRaw = lightPos - surfPos;
    const float  len = length(dirRaw);
    const float3 lightDir = dirRaw / len;
    const float  distSqr = len * len;

    const float thetaCos = ThetaCos(normal, lightDir);

    LightContrib contrib;
    contrib.Diffuse  = Diffuse() * thetaCos * intensity / distSqr;
    contrib.Specular = Specular(lightDir, normal, viewDir, specPower) * intensity / distSqr;
    return contrib;
}


float4 PsIllumSurf(PS_INPUT input) : SV_Target
{
    const float3 normal  = normalize(input.Normal); // normal is interpolated - renormalize 
    const float3 viewDir = normalize((float3)CameraPos - (float3)input.PosWorld);

    LightContrib lightContribs = { {0, 0, 0, 0}, {0, 0, 0, 0} };

    const float specPower = 400.f;

    lightContribs = AmbLightContrib(AmbientLightLuminance);

    for (int i = 0; i < DIRECT_LIGHTS_COUNT; i++)
    {
        LightContrib contrib = DirLightContrib((float3)DirectLightDirs[i],
                                               normal,
                                               viewDir,
                                               DirectLightLuminances[i],
                                               specPower);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    for (int i = 0; i < POINT_LIGHTS_COUNT; i++)
    {
        LightContrib contrib = PointLightContrib((float3)input.PosWorld,
                                                 (float3)PointLightPositions[i],
                                                 normal,
                                                 viewDir,
                                                 PointLightIntensities[i],
                                                 specPower);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    float4 diffuseTexture  = { 1.0, 1.0, 1.0, 1 };
    float4 specularTexture = { 1.0, 1.0, 1.0, 1 };
    //float4 diffuseTexture  = { 0.0, 0.0, 0.0, 1 }; //{ 1.0, 1.0, 1.0, 1 };
    //float4 specularTexture = { 1.0, 1.0, 1.0, 1 }; //{ 0.0, 0.0, 0.0, 1 };
    diffuseTexture = txDiffuse.Sample(samLinear, input.Tex);

    float4 output =
          lightContribs.Diffuse  * diffuseTexture  * 0.9
        + lightContribs.Specular * specularTexture * 0.1
        ;

    output.a = 1;

    return output;
}


float4 PsEmissiveSurf(PS_INPUT input) : SV_Target
{
    return MeshColor;
}
