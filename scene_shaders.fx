#include "constants.hpp"

static const float PI = 3.14159265f;

// Metalness workflow
Texture2D BaseColorTexture      : register(t0);
Texture2D MetalRoughnessTexture : register(t1);

// Specularity workflow
Texture2D DiffuseTexture        : register(t2);
Texture2D SpecularTexture       : register(t3);

SamplerState LinearSampler : register(s0);

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
    // Light sources
    float4 AmbientLightLuminance;
    float4 DirectLightDirs[DIRECT_LIGHTS_COUNT];
    float4 DirectLightLuminances[DIRECT_LIGHTS_COUNT];
    float4 PointLightPositions[POINT_LIGHTS_COUNT];
    float4 PointLightIntensities[POINT_LIGHTS_COUNT];
};

cbuffer cbChangesPerSceneNode : register(b3)
{
    matrix WorldMtrx;
    float4 MeshColor;
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


float4 PsNormalVisualizer(PS_INPUT input) : SV_Target
{
    const float3 normal = normalize(input.Normal); // normal is interpolated - renormalize 
    const float3 positiveNormal = (normal + float3(1.f, 1.f, 1.f)) / 2.;
    return float4(positiveNormal.x, positiveNormal.y, positiveNormal.z, 1.);
}


float ThetaCos(float3 normal, float3 lightDir)
{
    return max(dot(normal, lightDir), 0.);
}


struct PbrSpec_LightContrib
{
    float4 Diffuse;
    float4 Specular;
};


float4 Diffuse()
{
    return 1 / PI;
};


float4 BlinPhongSpecular(float3 lightDir, float3 normal, float3 viewDir, float specPower)
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


PbrSpec_LightContrib PbrSpec_AmbLightContrib(float4 luminance)
{
    PbrSpec_LightContrib contrib;
    contrib.Diffuse  = luminance;
    contrib.Specular = luminance; // estimate based on assumption that full specular lobe integrates to 1
    return contrib;
}


PbrSpec_LightContrib PbrSpec_DirLightContrib(float3 lightDir,
                                             float3 normal,
                                             float3 viewDir,
                                             float4 luminance,
                                             float specPower)
{
    const float thetaCos = ThetaCos(normal, lightDir);

    PbrSpec_LightContrib contrib;
    contrib.Diffuse  = Diffuse() * thetaCos * luminance;
    contrib.Specular = BlinPhongSpecular(lightDir, normal, viewDir, specPower) * luminance;
    return contrib;
}


PbrSpec_LightContrib PbrSpec_PointLightContrib(float3 surfPos,
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

    PbrSpec_LightContrib contrib;
    contrib.Diffuse  = Diffuse() * thetaCos * intensity / distSqr;
    contrib.Specular = BlinPhongSpecular(lightDir, normal, viewDir, specPower) * intensity / distSqr;
    return contrib;
}


float4 PsPbrSpecularity(PS_INPUT input) : SV_Target
{
    const float3 normal  = normalize(input.Normal); // normal is interpolated - renormalize 
    const float3 viewDir = normalize((float3)CameraPos - (float3)input.PosWorld);

    PbrSpec_LightContrib lightContribs = { {0, 0, 0, 0}, {0, 0, 0, 0} };

    const float specPower = 100.f;

    lightContribs = PbrSpec_AmbLightContrib(AmbientLightLuminance);

    int i;
    for (i = 0; i < DIRECT_LIGHTS_COUNT; i++)
    {
        PbrSpec_LightContrib contrib = PbrSpec_DirLightContrib((float3)DirectLightDirs[i],
                                                               normal,
                                                               viewDir,
                                                               DirectLightLuminances[i],
                                                               specPower);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    for (i = 0; i < POINT_LIGHTS_COUNT; i++)
    {
        PbrSpec_LightContrib contrib = PbrSpec_PointLightContrib((float3)input.PosWorld,
                                                                 (float3)PointLightPositions[i],
                                                                 normal,
                                                                 viewDir,
                                                                 PointLightIntensities[i],
                                                                 specPower);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    float4 diffuseColor     = DiffuseTexture.Sample(LinearSampler, input.Tex);
    float4 specularColor    = SpecularTexture.Sample(LinearSampler, input.Tex);

    float4 output =
          lightContribs.Diffuse  * diffuseColor
        + lightContribs.Specular * specularColor;

    output.a = 1;

    return output;
}


struct PbrMetal_Inputs
{
    float4 diffuse;
    float4 f0;
    float alpha;
};


PbrMetal_Inputs PbrMetal_ComputeInputs(PS_INPUT input)
{
    const float4 baseColor      = BaseColorTexture.Sample(LinearSampler, input.Tex);
    const float4 metalRoughness = MetalRoughnessTexture.Sample(LinearSampler, input.Tex);
    const float4 metalness      = float4(metalRoughness.bbb, 1);
    const float  roughness      = metalRoughness.g;

    const float4 specularDiel   = float4(0.04, 0.04, 0.04, 1);
    const float4 diffuseDiel    = (float4(1, 1, 1, 1) - specularDiel) * baseColor;

    const float4 specularMetal  = baseColor;
    const float4 diffuseMetal   = float4(0, 0, 0, 1);

    PbrMetal_Inputs metalInputs;
    metalInputs.diffuse  = lerp(diffuseDiel,  diffuseMetal,  metalness);
    metalInputs.f0       = lerp(specularDiel, specularMetal, metalness);
    metalInputs.alpha    = roughness * roughness;
    return metalInputs;
}


float4 PsPbrMetalness(PS_INPUT input) : SV_Target
{
    const float3 normal  = normalize(input.Normal); // normal is interpolated - renormalize 
    const float3 viewDir = normalize((float3)CameraPos - (float3)input.PosWorld);

    const PbrMetal_Inputs metalInputs = PbrMetal_ComputeInputs(input);

    PbrSpec_LightContrib lightContribs = { {0, 0, 0, 0}, {0, 0, 0, 0} };

    const float specPower = 10 / (metalInputs.alpha + 0.001f); // blinn-phong approximation

    lightContribs = PbrSpec_AmbLightContrib(AmbientLightLuminance);

    int i;
    for (i = 0; i < DIRECT_LIGHTS_COUNT; i++)
    {
        PbrSpec_LightContrib contrib = PbrSpec_DirLightContrib((float3)DirectLightDirs[i],
                                                               normal,
                                                               viewDir,
                                                               DirectLightLuminances[i],
                                                               specPower);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    for (i = 0; i < POINT_LIGHTS_COUNT; i++)
    {
        PbrSpec_LightContrib contrib = PbrSpec_PointLightContrib((float3)input.PosWorld,
                                                                 (float3)PointLightPositions[i],
                                                                 normal,
                                                                 viewDir,
                                                                 PointLightIntensities[i],
                                                                 specPower);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    float4 output =
          lightContribs.Diffuse  * metalInputs.diffuse
        + lightContribs.Specular * metalInputs.f0;
    output.a = 1;

    return output;
}


float4 PsConstEmissive(PS_INPUT input) : SV_Target
{
    return MeshColor;
}
