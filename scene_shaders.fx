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


struct PbrS_LightContrib
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


PbrS_LightContrib PbrS_AmbLightContrib(float4 luminance)
{
    PbrS_LightContrib contrib;
    contrib.Diffuse  = luminance;
    contrib.Specular = luminance; // estimate based on assumption that full specular lobe integrates to 1
    return contrib;
}


PbrS_LightContrib PbrS_DirLightContrib(float3 lightDir,
                                       float3 normal,
                                       float3 viewDir,
                                       float4 luminance,
                                       float specPower)
{
    const float thetaCos = ThetaCos(normal, lightDir);

    PbrS_LightContrib contrib;
    contrib.Diffuse = Diffuse() * thetaCos * luminance;
    contrib.Specular = BlinPhongSpecular(lightDir, normal, viewDir, specPower) * luminance;
    return contrib;
}


PbrS_LightContrib PbrS_PointLightContrib(float3 surfPos,
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

    PbrS_LightContrib contrib;
    contrib.Diffuse = Diffuse() * thetaCos * intensity / distSqr;
    contrib.Specular = BlinPhongSpecular(lightDir, normal, viewDir, specPower) * intensity / distSqr;
    return contrib;
}


float4 PsPbrSpecularity(PS_INPUT input) : SV_Target
{
    const float3 normal  = normalize(input.Normal); // normal is interpolated - renormalize 
    const float3 viewDir = normalize((float3)CameraPos - (float3)input.PosWorld);

    PbrS_LightContrib lightContribs = { {0, 0, 0, 0}, {0, 0, 0, 0} };

    const float specPower = 100.f;

    lightContribs = PbrS_AmbLightContrib(AmbientLightLuminance);

    int i;
    for (i = 0; i < DIRECT_LIGHTS_COUNT; i++)
    {
        PbrS_LightContrib contrib = PbrS_DirLightContrib((float3)DirectLightDirs[i],
                                                         normal,
                                                         viewDir,
                                                         DirectLightLuminances[i],
                                                         specPower);
        lightContribs.Diffuse  += contrib.Diffuse;
        lightContribs.Specular += contrib.Specular;
    }

    for (i = 0; i < POINT_LIGHTS_COUNT; i++)
    {
        PbrS_LightContrib contrib = PbrS_PointLightContrib((float3)input.PosWorld,
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


struct PbrM_MatInfo
{
    float4 diffuse;
    float4 f0;
    float alpha;

    float specPower; // temporary blinn-phong approximation
};


float4 PbrM_BRDF(float3 lightDir, float3 normal, float3 viewDir, PbrM_MatInfo matInfo)
{
    if (dot(lightDir, normal) < 0)
        return float4(0, 0, 0, 1); // Light is below surface - no contribution

    ////// Blinn-Phong
    ////const float3 halfwayRaw = lightDir + viewDir;
    ////const float  halfwayLen = length(halfwayRaw);
    ////const float3 halfway = (halfwayLen > 0.001f) ? (halfwayRaw / halfwayLen) : normal;
    ////const float  halfwayCos = max(dot(halfway, normal), 0);
    ////const float  energyConserv = (2 + specPower) / (8 * PI);
    ////return pow(halfwayCos, specPower) * energyConserv;


    //// Calculate the shading terms for the microfacet specular shading model
    //vec3 F = specularReflection(materialInfo, angularInfo);
    //float Vis = visibilityOcclusion(materialInfo, angularInfo);
    //float D = microfacetDistribution(materialInfo, angularInfo);
    //
    //// Calculation of analytical lighting contribution
    //vec3 diffuseContrib = (1.0 - F) * diffuse(materialInfo);
    //vec3 specContrib = F * Vis * D;
    //
    //// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    //return angularInfo.NdotL * (diffuseContrib + specContrib);
    const float4 specular = float4(0, 0, 0, 1);

    const float4 diffuse = Diffuse() * matInfo.diffuse;

    return specular + diffuse;
}


float4 PbrM_AmbLightContrib(float4 luminance, PbrM_MatInfo matInfo)
{
    //contrib.Diffuse = luminance;
    const float4 diffuse = matInfo.diffuse * luminance;
    //contrib.Specular = luminance; // estimate based on assumption that full specular lobe integrates to 1
    const float4 specular = float4(0, 0, 0, 1);

    return diffuse + specular;
}


float4 PbrM_DirLightContrib(float3 lightDir,
                            float3 normal,
                            float3 viewDir,
                            float4 luminance,
                            PbrM_MatInfo matInfo)
{
    const float thetaCos = ThetaCos(normal, lightDir);

    const float4 brdf = PbrM_BRDF(lightDir, normal, viewDir, matInfo);

    return brdf * thetaCos * luminance;
}


float4 PbrM_PointLightContrib(float3 surfPos,
                              float3 lightPos,
                              float3 normal,
                              float3 viewDir,
                              float4 intensity,
                              PbrM_MatInfo matInfo)
{
    const float3 dirRaw = lightPos - surfPos;
    const float  len = length(dirRaw);
    const float3 lightDir = dirRaw / len;
    const float  distSqr = len * len;

    const float thetaCos = ThetaCos(normal, lightDir);

    const float4 brdf = PbrM_BRDF(lightDir, normal, viewDir, matInfo);

    return brdf * thetaCos * intensity / distSqr;
}


PbrM_MatInfo PbrM_ComputeMatInfo(PS_INPUT input)
{
    const float4 baseColor      = BaseColorTexture.Sample(LinearSampler, input.Tex);
    const float4 metalRoughness = MetalRoughnessTexture.Sample(LinearSampler, input.Tex);
    const float4 metalness      = float4(metalRoughness.bbb, 1);
    const float  roughness      = metalRoughness.g;

    const float4 specularDiel   = float4(0.04, 0.04, 0.04, 1);
    const float4 diffuseDiel    = (float4(1, 1, 1, 1) - specularDiel) * baseColor;

    const float4 specularMetal  = baseColor;
    const float4 diffuseMetal   = float4(0, 0, 0, 1);

    PbrM_MatInfo matInfo;
    matInfo.diffuse     = lerp(diffuseDiel,  diffuseMetal,  metalness);
    matInfo.f0          = lerp(specularDiel, specularMetal, metalness);
    matInfo.alpha       = roughness * roughness;
    matInfo.specPower   = 10 / (matInfo.alpha + 0.001f); // temporary blinn-phong approximation
    return matInfo;
}


float4 PsPbrMetalness(PS_INPUT input) : SV_Target
{
    const float3 normal  = normalize(input.Normal); // normal is interpolated - renormalize 
    const float3 viewDir = normalize((float3)CameraPos - (float3)input.PosWorld);

    const PbrM_MatInfo matInfo = PbrM_ComputeMatInfo(input);

    float4 output = float4(0, 0, 0, 0);

    output += PbrM_AmbLightContrib(AmbientLightLuminance, matInfo);

    int i;
    for (i = 0; i < DIRECT_LIGHTS_COUNT; i++)
    {
        output += PbrM_DirLightContrib((float3)DirectLightDirs[i],
                                       normal,
                                       viewDir,
                                       DirectLightLuminances[i],
                                       matInfo);
    }

    for (i = 0; i < POINT_LIGHTS_COUNT; i++)
    {
        output += PbrM_PointLightContrib((float3)input.PosWorld,
                                         (float3)PointLightPositions[i],
                                         normal,
                                         viewDir,
                                         PointLightIntensities[i],
                                         matInfo);
    }

    output.a = 1;
    return output;
}


float4 PsConstEmissive(PS_INPUT input) : SV_Target
{
    return MeshColor;
}
