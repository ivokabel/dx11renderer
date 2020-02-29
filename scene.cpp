#include "scene.hpp"

#include "scene_utils.hpp"
#include "gltf_utils.hpp"
#include "utils.hpp"
#include "log.hpp"

#include "Libs/tinygltf-2.2.0/tiny_gltf.h" // just the interfaces (no implementation)

#include <cassert>
#include <array>
#include <vector>

// debug
//#include "Libs/tinygltf-2.2.0/loader_example.h"

// debug: redirecting cout to string
// TODO: Move to Utils
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iostream>
struct cout_redirect {
    cout_redirect(std::streambuf * new_buffer) :
        old(std::cout.rdbuf(new_buffer))
    {}

    ~cout_redirect() {
        std::cout.rdbuf(old);
    }

private:
    std::streambuf * old;
};

typedef D3D11_INPUT_ELEMENT_DESC InputElmDesc;
const std::array<InputElmDesc, 3> sVertexLayout =
{
    InputElmDesc{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    InputElmDesc{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    InputElmDesc{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};


struct {
    XMVECTOR eye;
    XMVECTOR at;
    XMVECTOR up;
} sViewData = {
    XMVectorSet(0.0f,  4.0f, 10.0f, 1.0f),
    XMVectorSet(0.0f, -0.2f,  0.0f, 1.0f),
    XMVectorSet(0.0f,  1.0f,  0.0f, 1.0f),
};


struct CbNeverChanged
{
    XMMATRIX ViewMtrx;
    XMFLOAT4 CameraPos;
};

struct CbChangedOnResize
{
    XMMATRIX ProjectionMtrx;
};

struct CbChangedEachFrame
{
    // Light sources
    XMFLOAT4 AmbientLightLuminance;
    XMFLOAT4 DirectLightDirs[DIRECT_LIGHTS_COUNT];
    XMFLOAT4 DirectLightLuminances[DIRECT_LIGHTS_COUNT];
    XMFLOAT4 PointLightPositions[POINT_LIGHTS_COUNT];
    XMFLOAT4 PointLightIntensities[POINT_LIGHTS_COUNT];
};

struct CbChangedPerSceneNode
{
    XMMATRIX WorldMtrx;
    XMFLOAT4 MeshColor; // May be eventually replaced by the emmisive component of the standard surface shader
};

Scene::Scene(const SceneId sceneId) :
    mSceneId(sceneId)
{}

Scene::~Scene()
{
    Destroy();
}


bool Scene::Init(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return false;

    uint32_t wndWidth, wndHeight;
    if (!ctx.GetWindowSize(wndWidth, wndHeight))
        return false;

    auto device = ctx.GetDevice();
    auto immCtx = ctx.GetImmediateContext();

    HRESULT hr = S_OK;

    // Vertex shader
    ID3DBlob* pVsBlob = nullptr;
    if (!ctx.CreateVertexShader(L"../scene_shaders.fx", "VS", "vs_4_0", pVsBlob, mVertexShader))
        return false;

    // Input layout
    hr = device->CreateInputLayout(sVertexLayout.data(),
                                   (UINT)sVertexLayout.size(),
                                   pVsBlob->GetBufferPointer(),
                                   pVsBlob->GetBufferSize(),
                                   &mVertexLayout);
    pVsBlob->Release();
    if (FAILED(hr))
        return false;

    // Pixel shaders
    if (!ctx.CreatePixelShader(L"../scene_shaders.fx", "PsPbrMetalness", "ps_4_0", mPsPbrMetalness))
        return false;
    if (!ctx.CreatePixelShader(L"../scene_shaders.fx", "PsPbrSpecularity", "ps_4_0", mPsPbrSpecularity))
        return false;
    if (!ctx.CreatePixelShader(L"../scene_shaders.fx", "PsConstEmissive", "ps_4_0", mPsConstEmmisive))
        return false;

    // Create constant buffers
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.ByteWidth = sizeof(CbNeverChanged);
    hr = device->CreateBuffer(&bd, nullptr, &mCbNeverChanged);
    if (FAILED(hr))
        return false;
    bd.ByteWidth = sizeof(CbChangedOnResize);
    hr = device->CreateBuffer(&bd, nullptr, &mCbChangedOnResize);
    if (FAILED(hr))
        return hr;
    bd.ByteWidth = sizeof(CbChangedEachFrame);
    hr = device->CreateBuffer(&bd, nullptr, &mCbChangedEachFrame);
    if (FAILED(hr))
        return hr;
    bd.ByteWidth = sizeof(CbChangedPerSceneNode);
    hr = device->CreateBuffer(&bd, nullptr, &mCbChangedPerSceneNode);
    if (FAILED(hr))
        return hr;

    // Create sampler state
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_ANISOTROPIC; // D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&sampDesc, &mSamplerLinear);
    if (FAILED(hr))
        return hr;

    // Matrices
    mViewMtrx = XMMatrixLookAtLH(sViewData.eye, sViewData.at, sViewData.up);
    mProjectionMtrx = XMMatrixPerspectiveFovLH(XM_PIDIV4,
                                               (FLOAT)wndWidth / wndHeight,
                                               0.01f, 100.0f);

    // Update constant buffers which can be updated now

    CbNeverChanged cbNeverChanged;
    cbNeverChanged.ViewMtrx = XMMatrixTranspose(mViewMtrx);
    XMStoreFloat4(&cbNeverChanged.CameraPos, sViewData.eye);
    immCtx->UpdateSubresource(mCbNeverChanged, 0, NULL, &cbNeverChanged, 0, 0);

    CbChangedOnResize cbChangedOnResize;
    cbChangedOnResize.ProjectionMtrx = XMMatrixTranspose(mProjectionMtrx);
    immCtx->UpdateSubresource(mCbChangedOnResize, 0, NULL, &cbChangedOnResize, 0, 0);

    // Load scene

    if (!Load(ctx))
        return false;

    if (!mPointLightProxy.CreateSphere(ctx, 8, 16))
        return false;

    if (!mDefaultMaterial.CreatePbrSpecularity(ctx,
                                               nullptr,
                                               XMFLOAT4(0.5f, 0.5f, 0.5f, 1.f),
                                               nullptr,
                                               XMFLOAT4(0.f, 0.f, 0.f, 1.f)))
        return false;

    return true;
}


bool Scene::Load(IRenderingContext &ctx)
{
    switch (mSceneId)
    {
    case eExternalDebugTriangleWithoutIndices:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/TriangleWithoutIndices/TriangleWithoutIndices.gltf"))
            return false;
        AddScaleToRoots(3.);
        return true;

    case eExternalDebugTriangle:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/Triangle/Triangle.gltf"))
            return false;
        AddScaleToRoots(4.5);
        AddTranslationToRoots({ 0., -1.5, 0. });
        return true;

    case eExternalDebugSimpleMeshes:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/SimpleMeshes/SimpleMeshes.gltf"))
            return false;
        AddScaleToRoots(2.5);
        AddTranslationToRoots({ 0., -0.5, 0. });
        return true;

    case eExternalDebugBox:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/Box/Box.gltf"))
            return false;
        AddScaleToRoots(4.);
        AddTranslationToRoots({ 0., 0., 0. });
        return true;

    case eExternalDebugBoxInterleaved:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/BoxInterleaved/BoxInterleaved.gltf"))
            return false;
        AddScaleToRoots(4.);
        AddTranslationToRoots({ 0., 0., 0. });
        return true;

    case eExternalDebugBoxTextured:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/BoxTextured/BoxTextured.gltf"))
            return false;
        AddScaleToRoots(4.);
        AddTranslationToRoots({ 0., 0., 0. });
        return true;

    case eExternalDebugMetalRoughSpheres:
    case eExternalDebugMetalRoughSpheresNoTextures:
    {
        switch (mSceneId)
        {
        case eExternalDebugMetalRoughSpheres:
            if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/MetalRoughSpheres/MetalRoughSpheres.gltf"))
                return false;
            AddTranslationToRoots({ 0., 0.4, 1.6 });
            AddScaleToRoots(0.8);
            break;
        case eExternalDebugMetalRoughSpheresNoTextures:
            if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures.gltf"))
                return false;
            AddScaleToRoots(900);
            AddTranslationToRoots({ -2.5, -2.3, 1.5 });
            break;
        default:
            return false;
        }

        // debug lights
        const double amb = 0.5f;//0.3f;//
        mAmbientLight.luminance = XMFLOAT4(amb, amb, amb, 1.0f);
        const double lum = 2.f;//0.f;//
        mDirectLights[0].dir = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);
        const double ints = 0.f;//6.5f;//
        mPointLights[0].intensity = XMFLOAT4(ints, ints, ints, 1.0f);
        mPointLights[1].intensity = XMFLOAT4(ints, ints, ints, 1.0f);
        mPointLights[2].intensity = XMFLOAT4(ints, ints, ints, 1.0f);

        return true;
    }

    case eExternalDebug2CylinderEngine:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/2CylinderEngine/2CylinderEngine.gltf"))
            return false;
        AddScaleToRoots(0.012f);
        AddTranslationToRoots({ 0., 0.2, 0. });
        return true;

    case eExternalDebugDuck:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/Duck/Duck.gltf"))
            return false;
        AddScaleToRoots(3.8);
        AddTranslationToRoots({ -0.5, -3.3, 0. });
        return true;

    case eExternalDebugBoomBox:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/BoomBox/BoomBox.gltf"))
            return false;
        AddScaleToRoots(330.);
        AddTranslationToRoots({ 0., 0., 0. });
        return true;

    case eExternalDebugBoomBoxWithAxes:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/BoomBoxWithAxes/BoomBoxWithAxes.gltf"))
            return false;
        AddScaleToRoots(230.);
        AddTranslationToRoots({ 0., -2.2, 0. });
        return true;

    case eExternalDebugDamagedHelmet:
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/DamagedHelmet/DamagedHelmet.gltf"))
            return false;
        AddScaleToRoots(3.7);
        AddTranslationToRoots({ 0., 0.35, 0.3 });
        return true;

    case eExternalDebugFlightHelmet:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/FlightHelmet/FlightHelmet.gltf"))
            return false;
        AddScaleToRoots(11.0);
        AddTranslationToRoots({ 0., 1.2, 0. });
        return true;
        }

    case eExternalLowPolyDrifter:
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/Ivan Norman/Low-poly truck (car Drifter)/scene.gltf"))
            return false;
        AddScaleToRoots(0.015);
        AddTranslationToRoots({ 0.5, -1.2, 0. });
        return true;

    case eExternalWolfBaseMesh:
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/TheCharliEZM/Wolf base mesh/scene.gltf"))
            return false;
        AddScaleToRoots(0.008);
        AddTranslationToRoots({ 0.7, -2.4, 0. });
        return true;

    case eExternalNintendoGameBoyClassic:
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/hunter333d/Nintendo Game Boy Classic/scene.gltf"))
            return false;
        AddScaleToRoots(0.50);
        AddTranslationToRoots({ 0., -1.7, 0. });
        return true;

    case eExternalWeltron2001SpaceballRadio:
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/ArneDC/Prinzsound SM8 - Weltron 2001 Spaceball Radio/scene.gltf"))
            return false;
        AddScaleToRoots(.016);
        AddTranslationToRoots({ 0., -3.6, 0. });
        return true;

    case eExternalSpotMiniRigged:
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/Greg McKechnie/Spot Mini (Rigged)/scene.gltf"))
            return false;
        AddScaleToRoots(.00028f);
        AddTranslationToRoots({ 0., 0., 2.8 });
        return true;

    case eExternalMandaloriansHelmet:
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/arn204/The Mandalorian's Helmet/scene.gltf"))
            return false;
        AddTranslationToRoots({ -35., -70., 85. });
        AddScaleToRoots(.17f);
        return true;

    case eExternalTheRocket:
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/TuppsM/The Rocket/scene.gltf"))
            return false;
        AddScaleToRoots(.012);
        AddTranslationToRoots({ -0.1, -1., 0. });
        return true;

    case eExternalRoboV1:
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/_Sef_/Robo_V1/scene.gltf"))
            return false;
        AddTranslationToRoots({ 0., -100., -240. });
        AddScaleToRoots(.10);
        return true;

    case eHardwiredSimpleDebugSphere:
    {
        mMaterials.clear();
        mMaterials.resize(1, SceneMaterial());
        if (mMaterials.size() != 1)
            return false;

        auto &material0 = mMaterials[0];
        if (!material0.CreatePbrSpecularity(ctx,
                                            L"../Textures/vfx_debug_textures by Chris Judkins/debug_color_02.png",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f),
                                            nullptr,
                                            XMFLOAT4(0.f, 0.f, 0.f, 1.f)))
            return false;

        mRootNodes.clear();
        mRootNodes.resize(1, SceneNode(true));
        if (mRootNodes.size() != 1)
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive = node0.CreateEmptyPrimitive();
        if (!primitive)
            return false;

        if (!primitive->CreateSphere(ctx, 40, 80))
            return false;
        primitive->SetMaterialIdx(0);
        node0.AddScale({ 3.4f, 3.4f, 3.4f });

        mAmbientLight.luminance     = XMFLOAT4(0.10f, 0.10f, 0.10f, 1.0f);

        const float lum = 2.6f;
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        // coloured point lights
        mPointLights[0].intensity   = XMFLOAT4(4.000f, 1.800f, 1.200f, 1.0f); // red
        mPointLights[1].intensity   = XMFLOAT4(1.000f, 2.500f, 1.100f, 1.0f); // green
        mPointLights[2].intensity   = XMFLOAT4(1.200f, 1.800f, 4.000f, 1.0f); // blue

        return true;
    }

    case eHardwiredPbrMetalnesDebugSphere:
    {
        mMaterials.clear();
        mMaterials.resize(1, SceneMaterial());
        if (mMaterials.size() != 1)
            return false;

        auto &material0 = mMaterials[0];
        const XMFLOAT4 baseColorConstFactor(0.8f, 0.8f, 0.8f, 1.f);
        const float    metallicConstFactor  = 1.0f;
        const float    roughnessConstFactor =
                            //0.000f;
                            //0.001f;
                            //0.010f;
                            0.100f;
                            //0.200f;
                            //0.400f;
                            //0.800f;
                            //1.000f;
        if (!material0.CreatePbrMetalness(ctx,
                                          nullptr,
                                          baseColorConstFactor,
                                          nullptr,
                                          metallicConstFactor,
                                          roughnessConstFactor))
            return false;

        mRootNodes.clear();
        mRootNodes.resize(1, SceneNode(true));
        if (mRootNodes.size() != 1)
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive = node0.CreateEmptyPrimitive();
        if (!primitive)
            return false;

        if (!primitive->CreateSphere(ctx, 20, 40))//40, 80))
            return false;
        primitive->SetMaterialIdx(0);
        node0.AddScale(3.9f);
        node0.AddTranslation({ 0.f, -0.2f, 0.f });

        const float amb = 0.6f;
        mAmbientLight.luminance     = XMFLOAT4(amb, amb, amb, 1.0f);

        const float lum = 3.f;
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        // coloured point lights
        const float ints = 3.f;
        //mPointLights[0].intensity   = XMFLOAT4(4.000f * ints, 0.000f * ints, 0.000f * ints, 1.0f); // red
        //mPointLights[1].intensity   = XMFLOAT4(0.000f * ints, 2.500f * ints, 0.000f * ints, 1.0f); // green
        //mPointLights[2].intensity   = XMFLOAT4(0.000f * ints, 0.000f * ints, 4.000f * ints, 1.0f); // blue
        mPointLights[0].intensity   = XMFLOAT4(0.5f * ints, 0.5f * ints, 0.5f * ints, 1.0f);
        mPointLights[1].intensity   = XMFLOAT4(0.5f * ints, 0.5f * ints, 0.5f * ints, 1.0f);
        mPointLights[2].intensity   = XMFLOAT4(0.5f * ints, 0.5f * ints, 0.5f * ints, 1.0f);
        const float pointScale = 10.f;
        for (int i = 0; i < 3; ++i)
        {
            mPointLights[i].intensity.x *= pointScale;
            mPointLights[i].intensity.y *= pointScale;
            mPointLights[i].intensity.z *= pointScale;
        }

        return true;
    }

    case eHardwiredEarth:
    {
        mMaterials.clear();
        mMaterials.resize(1, SceneMaterial());
        if (mMaterials.size() != 1)
            return false;

        auto &material0 = mMaterials[0];
        if (!material0.CreatePbrSpecularity(ctx,
                                            L"../Textures/www.solarsystemscope.com/2k_earth_daymap.jpg",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f),
                                            L"../Textures/www.solarsystemscope.com/2k_earth_specular_map.tif",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f)))
            return false;

        mRootNodes.clear();
        mRootNodes.resize(1, SceneNode(true));
        if (mRootNodes.size() != 1)
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive = node0.CreateEmptyPrimitive();
        if (!primitive)
            return false;

        if (!primitive->CreateSphere(ctx, 40, 80))
            return false;
        primitive->SetMaterialIdx(0);
        node0.AddScale({ 3.4f, 3.4f, 3.4f });

        mAmbientLight.luminance     = XMFLOAT4(0.f, 0.f, 0.f, 1.0f);

        const float lum = 3.5f;
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        const float ints = 3.5f;
        mPointLights[0].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);
        mPointLights[1].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);
        mPointLights[2].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);

        return true;
    }

    case eHardwiredThreePlanets:
    {
        mMaterials.clear();
        mMaterials.resize(3, SceneMaterial());
        if (mMaterials.size() != 3)
            return false;

        mRootNodes.clear();
        mRootNodes.resize(3, SceneNode(true));
        if (mRootNodes.size() != 3)
            return false;

        auto &material0 = mMaterials[0];
        if (!material0.CreatePbrSpecularity(ctx,
                                            L"../Textures/www.solarsystemscope.com/2k_earth_daymap.jpg",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f),
                                            L"../Textures/www.solarsystemscope.com/2k_earth_specular_map.tif",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f)))
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive0 = node0.CreateEmptyPrimitive();
        if (!primitive0)
            return false;
        if (!primitive0->CreateSphere(ctx, 40, 80))
            return false;
        primitive0->SetMaterialIdx(0);
        node0.AddScale({ 2.2f, 2.2f, 2.2f });
        node0.AddTranslation({ 0.f, 0.f, -1.5f });

        auto &material1 = mMaterials[1];
        if (!material1.CreatePbrSpecularity(ctx, L"../Textures/www.solarsystemscope.com/2k_mars.jpg",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f),
                                            nullptr,
                                            XMFLOAT4(0.f, 0.f, 0.f, 1.f)))
            return false;

        auto &node1 = mRootNodes[1];
        auto primitive1 = node1.CreateEmptyPrimitive();
        if (!primitive1)
            return false;
        if (!primitive1->CreateSphere(ctx, 20, 40))
            return false;
        primitive1->SetMaterialIdx(1);
        node1.AddScale({ 1.2f, 1.2f, 1.2f });
        node1.AddTranslation({ -2.5f, 0.f, 2.0f });

        auto &material2 = mMaterials[2];
        if (!material2.CreatePbrSpecularity(ctx, L"../Textures/www.solarsystemscope.com/2k_jupiter.jpg",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f),
                                            nullptr,
                                            XMFLOAT4(0.f, 0.f, 0.f, 1.f)))
            return false;

        auto &node2 = mRootNodes[2];
        auto primitive2 = node2.CreateEmptyPrimitive();
        if (!primitive2)
            return false;
        if (!primitive2->CreateSphere(ctx, 20, 40))
            return false;
        primitive2->SetMaterialIdx(2);
        node2.AddScale({ 1.2f, 1.2f, 1.2f });
        node2.AddTranslation({ 2.5f, 0.f, 2.0f });

        const float amb = 0.f;
        mAmbientLight.luminance     = XMFLOAT4(amb, amb, amb, 1.0f);
        const float lum = 3.0f;
        mDirectLights[0].dir       = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);
        const float ints = 4.0f;
        mPointLights[0].intensity = XMFLOAT4(ints, ints, ints, 1.0f);
        mPointLights[1].intensity = XMFLOAT4(ints, ints, ints, 1.0f);
        mPointLights[2].intensity = XMFLOAT4(ints, ints, ints, 1.0f);

        return true;
    }

    default:
        return false;
    }
}


bool Scene::LoadExternal(IRenderingContext &ctx, const std::wstring &filePath)
{
    const auto fileExt = Utils::GetFilePathExt(filePath);
    if ((fileExt.compare(L"glb") == 0) ||
        (fileExt.compare(L"gltf") == 0))
    {
        return LoadGLTF(ctx, filePath);
    }
    else
    {
        Log::Error(L"The scene file has an unsupported file format extension (%s)!", fileExt.c_str());
        return false;
    }
}



const tinygltf::Accessor& GetPrimitiveAttrAccessor(bool &accessorLoaded,
                                                   const tinygltf::Model &model,
                                                   const std::map<std::string, int> &attributes,
                                                   const int primitiveIdx,
                                                   bool requiredData,
                                                   const std::string &attrName,
                                                   const std::wstring &logPrefix)
{
    static tinygltf::Accessor dummyAccessor;

    const auto attrIt = attributes.find(attrName);
    if (attrIt == attributes.end())
    {
        Log::Write(requiredData ? Log::eError : Log::eDebug,
                   L"%sNo %s attribute present in primitive %d!",
                   logPrefix.c_str(),
                   Utils::StringToWstring(attrName).c_str(),
                   primitiveIdx);
        accessorLoaded = false;
        return dummyAccessor;
    }

    const auto accessorIdx = attrIt->second;
    if ((accessorIdx < 0) || (accessorIdx >= model.accessors.size()))
    {
        Log::Error(L"%sInvalid %s accessor index (%d/%d)!",
                   logPrefix.c_str(),
                   Utils::StringToWstring(attrName).c_str(),
                   accessorIdx,
                   model.accessors.size());
        accessorLoaded = false;
        return dummyAccessor;
    }

    accessorLoaded = true;
    return model.accessors[accessorIdx];
}

template <typename ComponentType,
          size_t ComponentCount,
          typename TDataConsumer>
bool IterateGltfAccesorData(const tinygltf::Model &model,
                            const tinygltf::Accessor &accessor,
                            TDataConsumer DataConsumer,
                            const wchar_t *logPrefix,
                            const wchar_t *logDataName)
{
    Log::Debug(L"%s%s accesor \"%s\": view %d, offset %d, type %s<%s>, count %d",
               logPrefix,
               logDataName,
               Utils::StringToWstring(accessor.name).c_str(),
               accessor.bufferView,
               accessor.byteOffset,
               GltfUtils::TypeToWstring(accessor.type).c_str(),
               GltfUtils::ComponentTypeToWstring(accessor.componentType).c_str(),
               accessor.count);

    // Buffer view

    const auto bufferViewIdx = accessor.bufferView;

    if ((bufferViewIdx < 0) || (bufferViewIdx >= model.bufferViews.size()))
    {
        Log::Error(L"%sInvalid %s view buffer index (%d/%d)!",
                   logPrefix, logDataName, bufferViewIdx, model.bufferViews.size());
        return false;
    }

    const auto &bufferView = model.bufferViews[bufferViewIdx];

    //Log::Debug(L"%s%s buffer view %d \"%s\": buffer %d, offset %d, length %d, stride %d, target %s",
    //           logPrefix,
    //           logDataName,
    //           bufferViewIdx,
    //           Utils::StringToWstring(bufferView.name).c_str(),
    //           bufferView.buffer,
    //           bufferView.byteOffset,
    //           bufferView.byteLength,
    //           bufferView.byteStride,
    //           GltfUtils::TargetToWstring(bufferView.target).c_str());

    // Buffer

    const auto bufferIdx = bufferView.buffer;

    if ((bufferIdx < 0) || (bufferIdx >= model.buffers.size()))
    {
        Log::Error(L"%sInvalid %s buffer index (%d/%d)!",
                   logPrefix, logDataName, bufferIdx, model.buffers.size());
        return false;
    }

    const auto &buffer = model.buffers[bufferIdx];

    const auto byteEnd = bufferView.byteOffset + bufferView.byteLength;
    if (byteEnd > buffer.data.size())
    {
        Log::Error(L"%sAccessing data chunk outside %s buffer %d!",
                   logPrefix, logDataName, bufferIdx);
        return false;
    }

    //Log::Debug(L"%s%s buffer %d \"%s\": data %x, size %d, uri \"%s\"",
    //           logPrefix,
    //           logDataName,
    //           bufferIdx,
    //           Utils::StringToWstring(buffer.name).c_str(),
    //           buffer.data.data(),
    //           buffer.data.size(),
    //           Utils::StringToWstring(buffer.uri).c_str());

    // TODO: Check that buffer view is large enough to contain all data from accessor?

    // Data

    const auto componentSize = sizeof(ComponentType);
    const auto typeSize = ComponentCount * componentSize;
    const auto stride = bufferView.byteStride;
    const auto typeOffset = (stride == 0) ? typeSize : stride;

    auto ptr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
    int idx = 0;
    for (; idx < accessor.count; ++idx, ptr += typeOffset)
        DataConsumer(idx, ptr);

    return true;
}


bool Scene::LoadGLTF(IRenderingContext &ctx,
                     const std::wstring &filePath)
{
    using namespace std;

    Log::Debug(L"");
    const std::wstring logPrefix = L"LoadGLTF: ";

    tinygltf::Model model;
    if (!GltfUtils::LoadModel(model, filePath))
        return false;

    Log::Debug(L"");

    if (!LoadMaterialsFromGltf(ctx, model, logPrefix))
        return false;

    if (!LoadSceneFromGltf(ctx, model, logPrefix))
        return false;

    Log::Debug(L"");

    // debug lights
    const float amb = 0.5f;
    mAmbientLight.luminance = XMFLOAT4(amb, amb, amb, 1.0f);
    const float lum = 2.8f;
    mDirectLights[0].dir       = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
    mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);
    const float ints = 6.5f;
    mPointLights[0].intensity = XMFLOAT4(ints, ints, ints, 1.0f);
    mPointLights[1].intensity = XMFLOAT4(ints, ints, ints, 1.0f);
    mPointLights[2].intensity = XMFLOAT4(ints, ints, ints, 1.0f);

    return true;
}


bool Scene::LoadMaterialsFromGltf(IRenderingContext &ctx,
                                  const tinygltf::Model &model,
                                  const std::wstring &logPrefix)
{
    const auto &materials = model.materials;

    Log::Debug(L"%sMaterials: %d", logPrefix.c_str(), materials.size());

    const std::wstring materialLogPrefix = logPrefix + L"   ";
    const std::wstring valueLogPrefix = materialLogPrefix + L"   ";

    mMaterials.clear();
    mMaterials.reserve(materials.size());
    for (size_t matIdx = 0; matIdx < materials.size(); ++matIdx)
    {
        const auto &material = materials[matIdx];

        Log::Debug(L"%s%d/%d \"%s\"",
                   materialLogPrefix.c_str(),
                   matIdx,
                   materials.size(),
                   Utils::StringToWstring(material.name).c_str());

        SceneMaterial sceneMaterial;
        if (!sceneMaterial.LoadFromGltf(ctx, model, material, valueLogPrefix))
            return false;
        mMaterials.push_back(std::move(sceneMaterial));
    }

    return true;
}


bool Scene::LoadSceneFromGltf(IRenderingContext &ctx,
                              const tinygltf::Model &model,
                              const std::wstring &logPrefix)
{
    // Choose one scene
    if (model.scenes.size() < 1)
    {
        Log::Error(L"%sNo scenes present in the model!", logPrefix.c_str());
        return false;
    }
    if (model.scenes.size() > 1)
        Log::Warning(L"%sMore scenes present in the model. Loading just the first one.", logPrefix.c_str());
    const auto &scene = model.scenes[0];

    Log::Debug(L"%sScene 0 \"%s\": %d root node(s)",
               logPrefix.c_str(),
               Utils::StringToWstring(scene.name).c_str(),
               scene.nodes.size());

    // Nodes hierarchy
    mRootNodes.clear();
    mRootNodes.reserve(scene.nodes.size());
    for (const auto nodeIdx : scene.nodes)
    {
        SceneNode sceneNode(true);
        if (!LoadSceneNodeFromGLTF(ctx, sceneNode, model, nodeIdx, logPrefix + L"   "))
            return false;
        mRootNodes.push_back(std::move(sceneNode));
    }

    return true;
}


bool Scene::LoadSceneNodeFromGLTF(IRenderingContext &ctx,
                                  SceneNode &sceneNode,
                                  const tinygltf::Model &model,
                                  int nodeIdx,
                                  const std::wstring &logPrefix)
{
    if (nodeIdx >= model.nodes.size())
    {
        Log::Error(L"%sInvalid node index (%d/%d)!", logPrefix.c_str(), nodeIdx, model.nodes.size());
        return false;
    }

    const auto &node = model.nodes[nodeIdx];

    // Node itself
    if (!sceneNode.LoadFromGLTF(ctx, model, node, nodeIdx, logPrefix))
        return false;

    // Children
    sceneNode.mChildren.clear();
    sceneNode.mChildren.reserve(node.children.size());
    const std::wstring &childLogPrefix = logPrefix + L"   ";
    for (const auto childIdx : node.children)
    {
        if ((childIdx < 0) || (childIdx >= model.nodes.size()))
        {
            Log::Error(L"%sInvalid child node index (%d/%d)!", childLogPrefix.c_str(), childIdx, model.nodes.size());
            return false;
        }

        //Log::Debug(L"%sLoading child %d \"%s\"",
        //           childLogPrefix.c_str(),
        //           childIdx,
        //           Utils::StringToWstring(model.nodes[childIdx].name).c_str());

        SceneNode childNode;
        if (!LoadSceneNodeFromGLTF(ctx, childNode, model, childIdx, childLogPrefix))
            return false;
        sceneNode.mChildren.push_back(std::move(childNode));
    }

    return true;
}


void Scene::Destroy()
{
    Utils::ReleaseAndMakeNull(mVertexShader);
    Utils::ReleaseAndMakeNull(mPsPbrMetalness);
    Utils::ReleaseAndMakeNull(mPsPbrSpecularity);
    Utils::ReleaseAndMakeNull(mPsConstEmmisive);
    Utils::ReleaseAndMakeNull(mVertexLayout);
    Utils::ReleaseAndMakeNull(mCbNeverChanged);
    Utils::ReleaseAndMakeNull(mCbChangedOnResize);
    Utils::ReleaseAndMakeNull(mCbChangedEachFrame);
    Utils::ReleaseAndMakeNull(mCbChangedPerSceneNode);
    Utils::ReleaseAndMakeNull(mSamplerLinear);

    mRootNodes.clear();
    mPointLightProxy.Destroy();
}


void Scene::AnimateFrame(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    // Scene geometry
    for (auto &node : mRootNodes)
        node.Animate(ctx);

    // Directional lights (are steady for now)
    for (auto &dirLight : mDirectLights)
        dirLight.dirTransf = dirLight.dir;

    // Animate point lights

    const float time = ctx.GetFrameAnimationTime();
    const float period = 15.f; //seconds
    const float totalAnimPos = time / period;
    const float angle = totalAnimPos * XM_2PI;

    const auto pointCount = mPointLights.size();
    for (int i = 0; i < pointCount; i++)
    {
        const float lightRelOffset = (float)i / pointCount;

        const float orbitRadius =
            (mSceneId == eHardwiredThreePlanets)
            ? 5.2f
            : 5.5f;
        const float rotationAngle = -2.f * angle - lightRelOffset * XM_2PI;
        const float orbitInclination =
            (mSceneId == eHardwiredThreePlanets)
            ? (lightRelOffset - 0.5f) * XM_PIDIV2
            : lightRelOffset * XM_PI;

        const XMMATRIX translationMtrx  = XMMatrixTranslation(orbitRadius, 0.f, 0.f);
        const XMMATRIX rotationMtrx     = XMMatrixRotationY(rotationAngle);
        const XMMATRIX inclinationMtrx  = XMMatrixRotationZ(orbitInclination);
        const XMMATRIX transfMtrx = translationMtrx * rotationMtrx * inclinationMtrx;

        const XMVECTOR lightVec = XMLoadFloat4(&mPointLights[i].pos);
        const XMVECTOR lightVecTransf = XMVector3Transform(lightVec, transfMtrx);
        XMStoreFloat4(&mPointLights[i].posTransf, lightVecTransf);
    }
}


void Scene::RenderFrame(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    auto immCtx = ctx.GetImmediateContext();

    // Frame constant buffer
    CbChangedEachFrame cbEachFrame;
    cbEachFrame.AmbientLightLuminance = mAmbientLight.luminance;
    for (int i = 0; i < mDirectLights.size(); i++)
    {
        cbEachFrame.DirectLightDirs[i]       = mDirectLights[i].dirTransf;
        cbEachFrame.DirectLightLuminances[i] = mDirectLights[i].luminance;
    }
    for (int i = 0; i < mPointLights.size(); i++)
    {
        cbEachFrame.PointLightPositions[i]   = mPointLights[i].posTransf;
        cbEachFrame.PointLightIntensities[i] = mPointLights[i].intensity;
    }
    immCtx->UpdateSubresource(mCbChangedEachFrame, 0, nullptr, &cbEachFrame, 0, 0);

    // Setup vertex shader
    immCtx->VSSetShader(mVertexShader, nullptr, 0);
    immCtx->VSSetConstantBuffers(0, 1, &mCbNeverChanged);
    immCtx->VSSetConstantBuffers(1, 1, &mCbChangedOnResize);
    immCtx->VSSetConstantBuffers(2, 1, &mCbChangedEachFrame);
    immCtx->VSSetConstantBuffers(3, 1, &mCbChangedPerSceneNode);

    // Setup pixel shader data (shader itself is chosen later for each material)
    immCtx->PSSetConstantBuffers(0, 1, &mCbNeverChanged);
    immCtx->PSSetConstantBuffers(2, 1, &mCbChangedEachFrame);
    immCtx->PSSetConstantBuffers(3, 1, &mCbChangedPerSceneNode);
    immCtx->PSSetSamplers(0, 1, &mSamplerLinear);

    // Scene geometry
    for (auto &node : mRootNodes)
        RenderNode(ctx, node, XMMatrixIdentity());

    // Proxy geometry for point lights
    for (int i = 0; i < mPointLights.size(); i++)
    {
        CbChangedPerSceneNode cbPerSceneNode;

        const float radius = 0.07f;
        XMMATRIX lightScaleMtrx = XMMatrixScaling(radius, radius, radius);
        XMMATRIX lightTrnslMtrx = XMMatrixTranslationFromVector(XMLoadFloat4(&mPointLights[i].posTransf));
        XMMATRIX lightMtrx = lightScaleMtrx * lightTrnslMtrx;
        cbPerSceneNode.WorldMtrx = XMMatrixTranspose(lightMtrx);

        const float radius2 = radius * radius;
        cbPerSceneNode.MeshColor = {
            mPointLights[i].intensity.x / radius2,
            mPointLights[i].intensity.y / radius2,
            mPointLights[i].intensity.z / radius2,
            mPointLights[i].intensity.w / radius2,
        };

        immCtx->UpdateSubresource(mCbChangedPerSceneNode, 0, nullptr, &cbPerSceneNode, 0, 0);

        immCtx->PSSetShader(mPsConstEmmisive, nullptr, 0);
        mPointLightProxy.DrawGeometry(ctx, mVertexLayout);
    }
}


void Scene::AddScaleToRoots(double scale)
{
    for (auto &rootNode : mRootNodes)
        rootNode.AddScale(scale);
}


void Scene::AddScaleToRoots(const std::vector<double> &vec)
{
    for (auto &rootNode : mRootNodes)
        rootNode.AddScale(vec);
}


void Scene::AddRotationQuaternionToRoots(const std::vector<double> &vec)
{
    for (auto &rootNode : mRootNodes)
        rootNode.AddRotationQuaternion(vec);
}


void Scene::AddTranslationToRoots(const std::vector<double> &vec)
{
    for (auto &rootNode : mRootNodes)
        rootNode.AddTranslation(vec);
}


void Scene::AddMatrixToRoots(const std::vector<double> &vec)
{
    for (auto &rootNode : mRootNodes)
        rootNode.AddMatrix(vec);
}


void Scene::RenderNode(IRenderingContext &ctx,
                       const SceneNode &node,
                       const XMMATRIX &parentWorldMtrx)
{
    if (!ctx.IsValid())
        return;

    auto immCtx = ctx.GetImmediateContext();

    const auto worldMtrx = node.GetWorldMtrx() * parentWorldMtrx;

    // Update per-node constant buffer
    CbChangedPerSceneNode cbPerSceneNode;
    cbPerSceneNode.WorldMtrx = XMMatrixTranspose(worldMtrx);
    cbPerSceneNode.MeshColor = { 0.f, 1.f, 0.f, 1.f, };
    immCtx->UpdateSubresource(mCbChangedPerSceneNode, 0, nullptr, &cbPerSceneNode, 0, 0);

    // Draw current node
    for (auto &primitive : node.mPrimitives)
    {
        const SceneMaterial &material = [&]()
        {
            const int matIdx = primitive.GetMaterialIdx();
            if (matIdx >= 0 && matIdx < mMaterials.size())
                return mMaterials[matIdx];
            else
                return mDefaultMaterial;
        }();

        switch (material.GetWorkflow())
        {
        case MaterialWorkflow::kPbrMetalness:
            immCtx->PSSetShader(mPsPbrMetalness, nullptr, 0);
            immCtx->PSSetShaderResources(0, 1, material.GetBaseColorSRV());
            immCtx->PSSetShaderResources(1, 1, material.GetMetallicRoughnessSRV());
            break;
        case MaterialWorkflow::kPbrSpecularity:
            immCtx->PSSetShader(mPsPbrSpecularity, nullptr, 0);
            immCtx->PSSetShaderResources(2, 1, material.GetBaseColorSRV());
            immCtx->PSSetShaderResources(3, 1, material.GetSpecularSRV());
            break;
        default:
            continue;
        }

        primitive.DrawGeometry(ctx, mVertexLayout);
    }

    // Children
    for (auto &child : node.mChildren)
        RenderNode(ctx, child, worldMtrx);
}

bool Scene::GetAmbientColor(float(&rgba)[4])
{
    rgba[0] = mAmbientLight.luminance.x;
    rgba[1] = mAmbientLight.luminance.y;
    rgba[2] = mAmbientLight.luminance.z;
    rgba[3] = mAmbientLight.luminance.w;
    return true;
}


ScenePrimitive::ScenePrimitive()
{}

ScenePrimitive::ScenePrimitive(const ScenePrimitive &src) :
    mVertices(src.mVertices),
    mIndices(src.mIndices),
    mTopology(src.mTopology),
    mVertexBuffer(src.mVertexBuffer),
    mIndexBuffer(src.mIndexBuffer),
    mMaterialIdx(src.mMaterialIdx)
{
    // We are creating new references of device resources
    Utils::SafeAddRef(mVertexBuffer);
    Utils::SafeAddRef(mIndexBuffer);
}

ScenePrimitive::ScenePrimitive(ScenePrimitive &&src) :
    mVertices(std::move(src.mVertices)),
    mIndices(std::move(src.mIndices)),
    mTopology(Utils::Exchange(src.mTopology, D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)),
    mVertexBuffer(Utils::Exchange(src.mVertexBuffer, nullptr)),
    mIndexBuffer(Utils::Exchange(src.mIndexBuffer, nullptr)),
    mMaterialIdx(Utils::Exchange(src.mMaterialIdx, -1))
{}

ScenePrimitive& ScenePrimitive::operator =(const ScenePrimitive &src)
{
    mVertices = src.mVertices;
    mIndices = src.mIndices;
    mTopology = src.mTopology;
    mVertexBuffer = src.mVertexBuffer;
    mIndexBuffer = src.mIndexBuffer;

    // We are creating new references of device resources
    Utils::SafeAddRef(mVertexBuffer);
    Utils::SafeAddRef(mIndexBuffer);

    mMaterialIdx = src.mMaterialIdx;

    return *this;
}

ScenePrimitive& ScenePrimitive::operator =(ScenePrimitive &&src)
{
    mVertices = std::move(src.mVertices);
    mIndices = std::move(src.mIndices);
    mTopology = Utils::Exchange(src.mTopology, D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);
    mVertexBuffer = Utils::Exchange(src.mVertexBuffer, nullptr);
    mIndexBuffer = Utils::Exchange(src.mIndexBuffer, nullptr);

    mMaterialIdx = Utils::Exchange(src.mMaterialIdx, -1);

    return *this;
}

ScenePrimitive::~ScenePrimitive()
{
    Destroy();
}


bool ScenePrimitive::CreateCube(IRenderingContext & ctx)
{
    if (!GenerateCubeGeometry())
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;

    return true;
}


bool ScenePrimitive::CreateOctahedron(IRenderingContext & ctx)
{
    if (!GenerateOctahedronGeometry())
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;

    return true;
}


bool ScenePrimitive::CreateSphere(IRenderingContext & ctx,
                                  const WORD vertSegmCount,
                                  const WORD stripCount)
{
    if (!GenerateSphereGeometry(vertSegmCount, stripCount))
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;

    return true;
}


bool ScenePrimitive::GenerateCubeGeometry()
{
    mVertices =
    {
        // Up
        SceneVertex{ XMFLOAT3(-1.0f, 1.0f, -1.0f),  XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, 1.0f, -1.0f),   XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, 1.0f,  1.0f),   XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f, 1.0f,  1.0f),  XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT2(0.0f, 1.0f) },

        // Down
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, -1.0f, -1.0f),  XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, -1.0f,  1.0f),  XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },

        // Side 1
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },

        // Side 3
        SceneVertex{ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f),  XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f),  XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f),  XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f),  XMFLOAT2(0.0f, 1.0f) },

        // Side 2
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, -1.0f, -1.0f),  XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f,  1.0f, -1.0f),  XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },

        // Side 4
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f, 1.0f),  XMFLOAT3(0.0f, 0.0f, 1.0f),  XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, -1.0f, 1.0f),   XMFLOAT3(0.0f, 0.0f, 1.0f),  XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f,  1.0f, 1.0f),   XMFLOAT3(0.0f, 0.0f, 1.0f),  XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f,  1.0f, 1.0f),  XMFLOAT3(0.0f, 0.0f, 1.0f),  XMFLOAT2(0.0f, 1.0f) },
    };

    mIndices =
    {
        // Up
        3, 1, 0,
        2, 1, 3,

        // Down
        6, 4, 5,
        7, 4, 6,

        // Side 1
        11, 9, 8,
        10, 9, 11,

        // Side 3
        14, 12, 13,
        15, 12, 14,

        // Side 2
        19, 17, 16,
        18, 17, 19,

        // Side 4
        22, 20, 21,
        23, 20, 22
    };

    mTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    return true;
}


bool ScenePrimitive::GenerateOctahedronGeometry()
{
    mVertices =
    {
        // Noth pole
        SceneVertex{ XMFLOAT3( 0.0f, 1.0f, 0.0f),  XMFLOAT3( 0.0f, 1.0f, 0.0f),  XMFLOAT2(0.0f, 0.0f) },

        // Points on equator
        SceneVertex{ XMFLOAT3( 1.0f, 0.0f, 0.0f),  XMFLOAT3( 1.0f, 0.0f, 0.0f),  XMFLOAT2(0.00f, 0.5f) },
        SceneVertex{ XMFLOAT3( 0.0f, 0.0f, 1.0f),  XMFLOAT3( 0.0f, 0.0f, 1.0f),  XMFLOAT2(0.25f, 0.5f) },
        SceneVertex{ XMFLOAT3(-1.0f, 0.0f, 0.0f),  XMFLOAT3(-1.0f, 0.0f, 0.0f),  XMFLOAT2(0.50f, 0.5f) },
        SceneVertex{ XMFLOAT3( 0.0f, 0.0f,-1.0f),  XMFLOAT3( 0.0f, 0.0f,-1.0f),  XMFLOAT2(0.75f, 0.5f) },

        // South pole
        SceneVertex{ XMFLOAT3( 0.0f,-1.0f, 0.0f),  XMFLOAT3( 0.0f,-1.0f, 0.0f),  XMFLOAT2(1.0f, 1.0f) },
    };

    mIndices =
    {
        // Band ++
        0, 2, 1,
        1, 2, 5,

        // Band -+
        0, 3, 2,
        2, 3, 5,

        // Band --
        0, 4, 3,
        3, 4, 5,

        // Band +-
        0, 1, 4,
        4, 1, 5,
    };

    mTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    return true;
}


bool ScenePrimitive::GenerateSphereGeometry(const WORD vertSegmCount, const WORD stripCount)
{
    if (vertSegmCount < 2)
    {
        Log::Error(L"Spherical stripe must have at least 2 vertical segments");
        return false;
    }
    if (stripCount < 3)
    {
        Log::Error(L"Sphere must have at least 3 stripes");
        return false;
    }

    const WORD horzLineCount = vertSegmCount - 1;
    const WORD vertexCountPerStrip = 2 /*poles*/ + horzLineCount;
    const WORD vertexCount = (stripCount + 1) * vertexCountPerStrip;
    const WORD indexCount  = stripCount * (2 /*poles*/ + 2 * horzLineCount + 1 /*strip restart*/);

    // Vertices
    mVertices.reserve(vertexCount);
    const float stripSizeAng = XM_2PI / stripCount;
    const float stripSizeRel =    1.f / stripCount;
    const float vertSegmSizeAng = XM_PI / vertSegmCount;
    const float vertSegmSizeRel =   1.f / vertSegmCount;
    for (WORD strip = 0; strip <= stripCount; strip++) // first and last vertices need to be replicated due to texture stitching
    {
        // Inner segments
        const float phi = strip * stripSizeAng;
        const float xBase = cos(phi);
        const float zBase = sin(phi);
        const float uLine = strip * stripSizeRel * 1.000001f;
        for (WORD line = 0; line < horzLineCount; line++)
        {
            const float theta = (line + 1) * vertSegmSizeAng;
            const float ringRadius = sin(theta);
            const float y = cos(theta);
            const XMFLOAT3 pt(xBase * ringRadius, y, zBase * ringRadius);
            const float v = (line + 1) * vertSegmSizeRel;
            mVertices.push_back(SceneVertex{ pt, pt,  XMFLOAT2(uLine, v) }); // position==normal
        }

        // Poles
        const XMFLOAT3 northPole(0.0f,  1.0f, 0.0f);
        const XMFLOAT3 southPole(0.0f, -1.0f, 0.0f);
        const float uPole = uLine + stripSizeRel / 2;
        mVertices.push_back(SceneVertex{ northPole,  northPole,  XMFLOAT2(uPole, 0.0f) }); // position==normal
        mVertices.push_back(SceneVertex{ southPole,  southPole,  XMFLOAT2(uPole, 1.0f) }); // position==normal
    }

    assert(mVertices.size() == vertexCount);

    // Indices
    mIndices.reserve(indexCount);
    for (WORD strip = 0; strip < stripCount; strip++)
    {
        const WORD idxOffset = strip * vertexCountPerStrip;
        mIndices.push_back(idxOffset + vertexCountPerStrip - 2); // north pole
        for (WORD line = 0; line < horzLineCount; line++)
        {
            mIndices.push_back((idxOffset + line + vertexCountPerStrip) % vertexCount); // next strip, same line
            mIndices.push_back( idxOffset + line);
        }
        mIndices.push_back(idxOffset + vertexCountPerStrip - 1); // south pole
        mIndices.push_back(static_cast<uint32_t>(-1)); // strip restart
    }

    assert(mIndices.size() == indexCount);

    mTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    //mTopology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; // debug

    Log::Debug(L"ScenePrimitive::GenerateSphereGeometry: "
               L"%d segments, %d strips => %d triangles, %d vertices, %d indices",
               vertSegmCount, stripCount,
               stripCount * (2 * horzLineCount),
               vertexCount, indexCount);

    return true;
}


bool ScenePrimitive::LoadFromGLTF(IRenderingContext & ctx,
                                  const tinygltf::Model &model,
                                  const tinygltf::Mesh &mesh,
                                  const int primitiveIdx,
                                  const std::wstring &logPrefix)
{
    if (!LoadDataFromGLTF(model, mesh, primitiveIdx, logPrefix))
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;

    return true;
}


bool ScenePrimitive::LoadDataFromGLTF(const tinygltf::Model &model,
                                      const tinygltf::Mesh &mesh,
                                      const int primitiveIdx,
                                      const std::wstring &logPrefix)
{
    bool success = false;

    const auto &primitive = mesh.primitives[primitiveIdx];

    Log::Debug(L"%sPrimitive %d/%d: mode %s, attributes [%s], indices %d, material %d",
               logPrefix.c_str(),
               primitiveIdx,
               mesh.primitives.size(),
               GltfUtils::ModeToWstring(primitive.mode).c_str(),
               GltfUtils::StringIntMapToWstring(primitive.attributes).c_str(),
               primitive.indices,
               primitive.material);

    const auto &attrs = primitive.attributes;

    const std::wstring &subItemsLogPrefix = logPrefix + L"   ";
    const std::wstring &dataConsumerLogPrefix = subItemsLogPrefix + L"   ";

    // Positions

    auto &posAccessor = GetPrimitiveAttrAccessor(success, model, attrs, primitiveIdx,
                                                 true, "POSITION", subItemsLogPrefix.c_str());
    if (!success)
        return false;

    if ((posAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) ||
        (posAccessor.type != TINYGLTF_TYPE_VEC3))
    {
        Log::Error(L"%sUnsupported POSITION data type!", subItemsLogPrefix.c_str());
        return false;
    }

    mVertices.clear();
    mVertices.reserve(posAccessor.count);
    if (mVertices.capacity() < posAccessor.count)
    {
        Log::Error(L"%sUnable to allocate %d vertices!", subItemsLogPrefix.c_str(), posAccessor.count);
        return false;
    }

    auto PositionDataConsumer = [this, &dataConsumerLogPrefix](int itemIdx, const unsigned char *ptr)
    {
        auto pos = *reinterpret_cast<const XMFLOAT3*>(ptr);

        itemIdx; // unused param
        //Log::Debug(L"%s%d: pos [%.4f, %.4f, %.4f]",
        //           dataConsumerLogPrefix.c_str(),
        //           itemIdx,
        //           pos.x, pos.y, pos.z);

        mVertices.push_back(SceneVertex{ XMFLOAT3(pos.x, pos.y, pos.z),
                                         XMFLOAT3(0.0f, 0.0f, 1.0f), // TODO: Leave invalid?
                                         XMFLOAT2(0.0f, 0.0f) });
    };

    if (!IterateGltfAccesorData<float, 3>(model,
                                          posAccessor,
                                          PositionDataConsumer,
                                          subItemsLogPrefix.c_str(),
                                          L"Position"))
        return false;

    // Normals

    auto &normalAccessor = GetPrimitiveAttrAccessor(success, model, attrs, primitiveIdx,
                                                    false, "NORMAL", subItemsLogPrefix.c_str());
    if (success)
    {
        if ((normalAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) ||
            (normalAccessor.type != TINYGLTF_TYPE_VEC3))
        {
            Log::Error(L"%sUnsupported NORMAL data type!", subItemsLogPrefix.c_str());
            return false;
        }

        if (normalAccessor.count != posAccessor.count)
        {
            Log::Error(L"%sNormals count (%d) is different from position count (%d)!",
                       subItemsLogPrefix.c_str(), normalAccessor.count, posAccessor.count);
            return false;
        }

        auto NormalDataConsumer = [this, &dataConsumerLogPrefix](int itemIdx, const unsigned char *ptr)
        {
            auto normal = *reinterpret_cast<const XMFLOAT3*>(ptr);

            //Log::Debug(L"%s%d: normal [%.4f, %.4f, %.4f]",
            //           dataConsumerLogPrefix.c_str(),
            //           itemIdx, normal.x, normal.y, normal.z);

            mVertices[itemIdx].Normal = normal;
        };

        if (!IterateGltfAccesorData<float, 3>(model,
                                              normalAccessor,
                                              NormalDataConsumer,
                                              subItemsLogPrefix.c_str(),
                                              L"Normal"))
            return false;
    }
    //else
    //{
    //    // No normals provided
    //    // TODO: Generate?
    //}

    // Texture coordinates

    auto &texCoord0Accessor = GetPrimitiveAttrAccessor(success, model, attrs, primitiveIdx,
                                                       false, "TEXCOORD_0", subItemsLogPrefix.c_str());
    if (success)
    {
        if ((texCoord0Accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) ||
            (texCoord0Accessor.type != TINYGLTF_TYPE_VEC2))
        {
            Log::Error(L"%sUnsupported TEXCOORD_0 data type!", subItemsLogPrefix.c_str());
            return false;
        }

        if (texCoord0Accessor.count != posAccessor.count)
        {
            Log::Error(L"%sTexture coords count (%d) is different from position count (%d)!",
                       subItemsLogPrefix.c_str(), texCoord0Accessor.count, posAccessor.count);
            return false;
        }

        auto TexCoord0DataConsumer = [this, &dataConsumerLogPrefix](int itemIdx, const unsigned char *ptr)
        {
            auto texCoord0 = *reinterpret_cast<const XMFLOAT2*>(ptr);

            //Log::Debug(L"%s%d: texCoord0 [%.1f, %.1f]",
            //           dataConsumerLogPrefix.c_str(), itemIdx, texCoord0.x, texCoord0.y);

            mVertices[itemIdx].Tex = texCoord0;
        };

        if (!IterateGltfAccesorData<float, 2>(model,
                                              texCoord0Accessor,
                                              TexCoord0DataConsumer,
                                              subItemsLogPrefix.c_str(),
                                              L"Texture coordinates"))
            return false;
    }

    // Indices

    const auto indicesAccessorIdx = primitive.indices;
    if (indicesAccessorIdx >= model.accessors.size())
    {
        Log::Error(L"%sInvalid indices accessor index (%d/%d)!",
                   subItemsLogPrefix.c_str(), indicesAccessorIdx, model.accessors.size());
        return false;
    }
    if (indicesAccessorIdx < 0)
    {
        Log::Error(L"%sNon-indexed geometry is not supported!", subItemsLogPrefix.c_str());
        return false;
    }

    const auto &indicesAccessor = model.accessors[indicesAccessorIdx];

    if (indicesAccessor.type != TINYGLTF_TYPE_SCALAR)
    {
        Log::Error(L"%sUnsupported indices data type (must be scalar)!", subItemsLogPrefix.c_str());
        return false;
    }
    if ((indicesAccessor.componentType < TINYGLTF_COMPONENT_TYPE_BYTE) ||
        (indicesAccessor.componentType > TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT))
    {
        Log::Error(L"%sUnsupported indices data component type (%d)!",
                   subItemsLogPrefix.c_str(), indicesAccessor.componentType);
        return false;
    }

    mIndices.clear();
    mIndices.reserve(indicesAccessor.count);
    if (mIndices.capacity() < indicesAccessor.count)
    {
        Log::Error(L"%sUnable to allocate %d indices!", subItemsLogPrefix.c_str(), indicesAccessor.count);
        return false;
    }

    const auto indicesComponentType = indicesAccessor.componentType;
    auto IndexDataConsumer =
        [this, &dataConsumerLogPrefix, indicesComponentType]
        (int itemIdx, const unsigned char *ptr)
    {
        switch (indicesComponentType)
        {
        case TINYGLTF_COMPONENT_TYPE_BYTE:              mIndices.push_back(*reinterpret_cast<const int8_t*>(ptr)); break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:     mIndices.push_back(*reinterpret_cast<const uint8_t*>(ptr)); break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:             mIndices.push_back(*reinterpret_cast<const int16_t*>(ptr)); break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:    mIndices.push_back(*reinterpret_cast<const uint16_t*>(ptr)); break;
        case TINYGLTF_COMPONENT_TYPE_INT:               mIndices.push_back(*reinterpret_cast<const int32_t*>(ptr)); break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:      mIndices.push_back(*reinterpret_cast<const uint32_t*>(ptr)); break;
        }

        // debug
        itemIdx; // unused param
        //Log::Debug(L"%s%d: %d",
        //           dataConsumerLogPrefix.c_str(),
        //           itemIdx,
        //           mIndices.back());
    };

    // TODO: Wrap into a function IterateGltfAccesorData(componentType, ...)? std::forward()?
    switch (indicesComponentType)
    {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        IterateGltfAccesorData<const int8_t, 1>(model,
                                                indicesAccessor,
                                                IndexDataConsumer,
                                                subItemsLogPrefix.c_str(),
                                                L"Indices");
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        IterateGltfAccesorData<uint8_t, 1>(model,
                                           indicesAccessor,
                                           IndexDataConsumer,
                                           subItemsLogPrefix.c_str(),
                                           L"Indices");
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        IterateGltfAccesorData<int16_t, 1>(model,
                                           indicesAccessor,
                                           IndexDataConsumer,
                                           subItemsLogPrefix.c_str(),
                                           L"Indices");
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        IterateGltfAccesorData<uint16_t, 1>(model,
                                            indicesAccessor,
                                            IndexDataConsumer,
                                            subItemsLogPrefix.c_str(),
                                            L"Indices");
        break;
    case TINYGLTF_COMPONENT_TYPE_INT:
        IterateGltfAccesorData<int32_t, 1>(model,
                                           indicesAccessor,
                                           IndexDataConsumer,
                                           subItemsLogPrefix.c_str(),
                                           L"Indices");
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        IterateGltfAccesorData<uint32_t, 1>(model,
                                            indicesAccessor,
                                            IndexDataConsumer,
                                            subItemsLogPrefix.c_str(),
                                            L"Indices");
        break;
    }
    if (mIndices.size() != indicesAccessor.count)
    {
        Log::Error(L"%sFailed to load indices (loaded %d instead of %d))!",
                   subItemsLogPrefix.c_str(), mIndices.size(), indicesAccessor.count);
        return false;
    }

    // DX primitive topology
    mTopology = GltfUtils::ModeToTopology(primitive.mode);
    if (mTopology == D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
    {
        Log::Error(L"%sUnsupported primitive topology!", subItemsLogPrefix.c_str());
        return false;
    }

    // Material
    const auto matIdx = primitive.material;
    if (matIdx >= 0)
    {
        if (matIdx >= model.materials.size())
        {
            Log::Error(L"%sInvalid material index (%d/%d)!",
                       subItemsLogPrefix.c_str(), matIdx, model.materials.size());
            return false;
        }

        mMaterialIdx = matIdx;
    }

    return true;
}


bool ScenePrimitive::CreateDeviceBuffers(IRenderingContext & ctx)
{
    DestroyDeviceBuffers();

    auto device = ctx.GetDevice();
    if (!device)
        return false;

    HRESULT hr = S_OK;

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));

    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(initData));

    // Vertex buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = (UINT)(sizeof(SceneVertex) * mVertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = mVertices.data();
    hr = device->CreateBuffer(&bd, &initData, &mVertexBuffer);
    if (FAILED(hr))
    {
        DestroyDeviceBuffers();
        return false;
    }

    // Index buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(uint32_t) * (UINT)mIndices.size();
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = mIndices.data();
    hr = device->CreateBuffer(&bd, &initData, &mIndexBuffer);
    if (FAILED(hr))
    {
        DestroyDeviceBuffers();
        return false;
    }

    return true;
}


void ScenePrimitive::Destroy()
{
    DestroyGeomData();
    DestroyDeviceBuffers();
}


void ScenePrimitive::DestroyGeomData()
{
    mVertices.clear();
    mIndices.clear();
    mTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}


void ScenePrimitive::DestroyDeviceBuffers()
{
    Utils::ReleaseAndMakeNull(mVertexBuffer);
    Utils::ReleaseAndMakeNull(mIndexBuffer);
}


void ScenePrimitive::DrawGeometry(IRenderingContext &ctx, ID3D11InputLayout* vertexLayout) const
{
    auto immCtx = ctx.GetImmediateContext();

    immCtx->IASetInputLayout(vertexLayout);
    UINT stride = sizeof(SceneVertex);
    UINT offset = 0;
    immCtx->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
    immCtx->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    immCtx->IASetPrimitiveTopology(mTopology);

    immCtx->DrawIndexed((UINT)mIndices.size(), 0, 0);
}


SceneNode::SceneNode(bool isRootNode) :
    mIsRootNode(isRootNode),
    mLocalMtrx(XMMatrixIdentity()),
    mWorldMtrx(XMMatrixIdentity())
{}

ScenePrimitive* SceneNode::CreateEmptyPrimitive()
{
    mPrimitives.clear();
    mPrimitives.resize(1);
    if (mPrimitives.size() != 1)
        return nullptr;

    return &mPrimitives[0];
}

void SceneNode::SetIdentity()
{
    mLocalMtrx = XMMatrixIdentity();
}

void SceneNode::AddScale(double scale)
{
    AddScale({ scale, scale, scale });
}

void SceneNode::AddScale(const std::vector<double> &vec)
{
    if (vec.size() != 3)
    {
        if (vec.size() != 0)
            Log::Warning(L"SceneNode::AddScale: vector of incorrect size (%d instead of 3)",
                         vec.size());
        return;
    }

    const auto mtrx = XMMatrixScaling((float)vec[0], (float)vec[1], (float)vec[2]);

    mLocalMtrx = mLocalMtrx * mtrx;
}

void SceneNode::AddRotationQuaternion(const std::vector<double> &vec)
{
    if (vec.size() != 4)
    {
        if (vec.size() != 0)
            Log::Warning(L"SceneNode::AddRotationQuaternion: vector of incorrect size (%d instead of 4)",
                         vec.size());
        return;
    }

    const XMFLOAT4 quaternion((float)vec[0], (float)vec[1], (float)vec[2], (float)vec[3]);
    const auto xmQuaternion = XMLoadFloat4(&quaternion);
    const auto mtrx = XMMatrixRotationQuaternion(xmQuaternion);

    mLocalMtrx = mLocalMtrx * mtrx;
}

void SceneNode::AddTranslation(const std::vector<double> &vec)
{
    if (vec.size() != 3)
    {
        if (vec.size() != 0)
            Log::Warning(L"SceneNode::AddTranslation: vector of incorrect size (%d instead of 3)",
                         vec.size());
        return;
    }

    const auto mtrx = XMMatrixTranslation((float)vec[0], (float)vec[1], (float)vec[2]);

    mLocalMtrx = mLocalMtrx * mtrx;
}

void SceneNode::AddMatrix(const std::vector<double> &vec)
{
    if (vec.size() != 16)
    {
        if (vec.size() != 0)
            Log::Warning(L"SceneNode::AddMatrix: vector of incorrect size (%d instead of 16)",
                         vec.size());
        return;
    }

    const auto mtrx = XMMatrixSet(
        (float)vec[0],  (float)vec[1],  (float)vec[2],  (float)vec[3],
        (float)vec[4],  (float)vec[5],  (float)vec[6],  (float)vec[7],
        (float)vec[8],  (float)vec[9],  (float)vec[10], (float)vec[11],
        (float)vec[12], (float)vec[13], (float)vec[14], (float)vec[15]);

    mLocalMtrx = mLocalMtrx * mtrx;
}


bool SceneNode::LoadFromGLTF(IRenderingContext & ctx,
                             const tinygltf::Model &model,
                             const tinygltf::Node &node,
                             int nodeIdx,
                             const std::wstring &logPrefix)
{
    // debug
    if (Log::sLoggingLevel >= Log::eDebug)
    {
        std::wstring transforms;
        if (!node.rotation.empty())
            transforms += L"rotation ";
        if (!node.scale.empty())
            transforms += L"scale ";
        if (!node.translation.empty())
            transforms += L"translation ";
        if (!node.matrix.empty())
            transforms += L"matrix ";
        if (transforms.empty())
            transforms = L"none";
        Log::Debug(L"%sNode %d/%d \"%s\": mesh %d, transform %s, children %d",
                   logPrefix.c_str(), 
                   nodeIdx,
                   model.nodes.size(),
                   Utils::StringToWstring(node.name).c_str(),
                   node.mesh,
                   transforms.c_str(),
                   node.children.size());
    }

    const std::wstring &subItemsLogPrefix = logPrefix + L"   ";

    // Local transformation
    SetIdentity();
    if (node.matrix.size() == 16)
    {
        AddMatrix(node.matrix);

        // Sanity checking
        if (!node.scale.empty())
            Log::Warning(L"%sNode %d/%d \"%s\": node.scale is not empty when tranformation matrix is provided. Ignoring.",
                         logPrefix.c_str(),
                         nodeIdx,
                         model.nodes.size(),
                         Utils::StringToWstring(node.name).c_str());
        if (!node.rotation.empty())
            Log::Warning(L"%sNode %d/%d \"%s\": node.rotation is not empty when tranformation matrix is provided. Ignoring.",
                         logPrefix.c_str(),
                         nodeIdx,
                         model.nodes.size(),
                         Utils::StringToWstring(node.name).c_str());
        if (!node.translation.empty())
            Log::Warning(L"%sNode %d/%d \"%s\": node.translation is not empty when tranformation matrix is provided. Ignoring.",
                         logPrefix.c_str(),
                         nodeIdx,
                         model.nodes.size(),
                         Utils::StringToWstring(node.name).c_str());
    }
    else
    {
        AddScale(node.scale);
        AddRotationQuaternion(node.rotation);
        AddTranslation(node.translation);
    }

    // Mesh

    const auto meshIdx = node.mesh;
    if (meshIdx >= (int)model.meshes.size())
    {
        Log::Error(L"%sInvalid mesh index (%d/%d)!", subItemsLogPrefix.c_str(), meshIdx, model.meshes.size());
        return false;
    }

    if (meshIdx >= 0)
    {
        const auto &mesh = model.meshes[meshIdx];

        Log::Debug(L"%sMesh %d/%d \"%s\": %d primitive(s)",
                   subItemsLogPrefix.c_str(),
                   meshIdx,
                   model.meshes.size(),
                   Utils::StringToWstring(mesh.name).c_str(),
                   mesh.primitives.size());

        // Primitives
        const auto primitivesCount = mesh.primitives.size();
        mPrimitives.reserve(primitivesCount);
        for (size_t i = 0; i < primitivesCount; ++i)
        {
            ScenePrimitive primitive;
            if (!primitive.LoadFromGLTF(ctx, model, mesh, (int)i, subItemsLogPrefix + L"   "))
                return false;
            mPrimitives.push_back(std::move(primitive));
        }
    }

    return true;
}


void SceneNode::Animate(IRenderingContext &ctx)
{
    if (mIsRootNode)
    {
        const float time = ctx.GetFrameAnimationTime();
        const float period = 15.f; //seconds
        const float totalAnimPos = time / period;
        const float angle = totalAnimPos * XM_2PI;

        const XMMATRIX rotMtrx = XMMatrixRotationY(angle);

        mWorldMtrx = mLocalMtrx * rotMtrx;
    }
    else
        mWorldMtrx = mLocalMtrx;

    for (auto &child : mChildren)
        child.Animate(ctx);
}


SceneTexture::SceneTexture(XMFLOAT4 defaultConstFactor) :
    mConstFactor(defaultConstFactor),
    srv(nullptr)
{}

SceneTexture::SceneTexture(const SceneTexture &src) :
    mConstFactor(src.mConstFactor),
    srv(src.srv)
{
    // We are creating new reference of device resource
    Utils::SafeAddRef(srv);
}

SceneTexture& SceneTexture::operator =(const SceneTexture &src)
{
    mConstFactor = src.mConstFactor;
    srv          = src.srv;

    // We are creating new reference of device resource
    Utils::SafeAddRef(srv);

    return *this;
}

SceneTexture::SceneTexture(SceneTexture &&src) :
    mConstFactor(Utils::Exchange(src.mConstFactor, XMFLOAT4(0.f, 0.f, 0.f, 0.f))),
    srv(Utils::Exchange(src.srv, nullptr))
{}

SceneTexture& SceneTexture::operator =(SceneTexture &&src)
{
    mConstFactor = Utils::Exchange(src.mConstFactor, XMFLOAT4(0.f, 0.f, 0.f, 0.f));
    srv          = Utils::Exchange(src.srv, nullptr);

    return *this;
}

SceneTexture::~SceneTexture()
{
    Utils::ReleaseAndMakeNull(srv);
}


bool SceneTexture::Create(IRenderingContext &ctx, const wchar_t *path, XMFLOAT4 constFactor)
{
    auto device = ctx.GetDevice();
    if (!device)
        return false;

    HRESULT hr = S_OK;

    mConstFactor = constFactor;

    if (path)
    {
        // TODO: Pre-multiply by const factor
        hr = D3DX11CreateShaderResourceViewFromFile(device, path, nullptr, nullptr, &srv, nullptr);
        if (FAILED(hr))
            return false;
    }
    else
        SceneUtils::CreateConstantTextureSRV(ctx, srv, mConstFactor);

    return true;
}


bool SceneTexture::LoadFloat4FactorFromGltf(const char *paramName,
                                            const tinygltf::ParameterMap &params,
                                            const std::wstring &logPrefix)
{
    auto paramIt = params.find(paramName);
    if (paramIt != params.end())
    {
        auto &param = paramIt->second;
        if (param.number_array.size() != 4)
        {
            Log::Error(L"%sIncorrect \"%s\" material parameter (size %d instead of 4)!",
                       logPrefix.c_str(),
                       Utils::StringToWstring(paramName).c_str(),
                       param.number_array.size());
            return false;
        }

        mConstFactor = XMFLOAT4((float)param.number_array[0],
                                (float)param.number_array[1],
                                (float)param.number_array[2],
                                (float)param.number_array[3]);

        Log::Debug(L"%s%s: %s",
                   logPrefix.c_str(),
                   Utils::StringToWstring(paramName).c_str(),
                   GltfUtils::ParameterValueToWstring(param).c_str());
    }

    return true;
}


bool SceneTexture::LoadFloatFactorFromGltf(const char *paramName,
                                           uint32_t component,
                                           const tinygltf::ParameterMap &params,
                                           const std::wstring &logPrefix)
{
    auto paramIt = params.find(paramName);
    if (paramIt != params.end())
    {
        auto &param = paramIt->second;
        if (!param.has_number_value)
        {
            Log::Error(L"%sIncorrect \"%s\" material parameter type (must be float)!",
                       logPrefix.c_str(),
                       Utils::StringToWstring(paramName).c_str());
            return false;
        }

        switch (component)
        {
        case 0: mConstFactor.x = (float)param.number_value; break;
        case 1: mConstFactor.y = (float)param.number_value; break;
        case 2: mConstFactor.z = (float)param.number_value; break;
        case 3: mConstFactor.w = (float)param.number_value; break;
        default:
            Log::Error(L"%sIncorrect component index for \"%s\" material parameter (%d >= 4)!",
                       logPrefix.c_str(),
                       Utils::StringToWstring(paramName).c_str(),
                       component);
            return false;
        }

        Log::Debug(L"%s%s: %s",
                   logPrefix.c_str(),
                   Utils::StringToWstring(paramName).c_str(),
                   GltfUtils::ParameterValueToWstring(param).c_str());
    }

    return true;
}


// Multiplies the values with const factor and creates texture
bool SceneTexture::LoadTextureFromGltf(const char *paramName,
                                       IRenderingContext &ctx,
                                       const tinygltf::Model &model,
                                       const tinygltf::ParameterMap &params,
                                       const std::wstring &logPrefix)
{
    auto paramIt = params.find(paramName);
    if (paramIt != params.end())
    {
        const auto &param = paramIt->second;
        const auto &textures = model.textures;
        const auto &images = model.images;

        const auto textureIndex = param.TextureIndex();
        if ((textureIndex < 0) || (textureIndex >= textures.size()))
        {
            Log::Error(L"%sInvalid texture index (%d/%d) in \"%s\" parameter!",
                       logPrefix.c_str(),
                       textureIndex,
                       textures.size(),
                       Utils::StringToWstring(paramName).c_str());
            return false;
        }

        const auto &texture = textures[textureIndex];

        const auto texSource = texture.source;
        if ((texSource < 0) || (texSource >= images.size()))
        {
            Log::Error(L"%sInvalid source image index (%d/%d) in texture %d!",
                       logPrefix.c_str(),
                       texSource,
                       images.size(),
                       textureIndex);
            return false;
        }

        const auto &image = images[texSource];

        Log::Debug(L"%s%s: \"%s\"/\"%s\", %dx%d, %dx%db %s, data %dB",
                   logPrefix.c_str(),
                   Utils::StringToWstring(paramName).c_str(),
                   Utils::StringToWstring(image.name).c_str(),
                   Utils::StringToWstring(image.uri).c_str(),
                   image.width,
                   image.height,
                   image.component,
                   image.bits,
                   GltfUtils::ComponentTypeToWstring(image.pixel_type).c_str(),
                   image.image.size());

        const auto srcPixelSize = image.component * image.bits / 8;
        const auto expectedSrcDataSize = image.width * image.height * srcPixelSize;
        if (image.width <= 0 ||
            image.height <= 0 ||
            image.component != 4 ||
            image.bits != 8 ||
            image.pixel_type != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
            image.image.size() != expectedSrcDataSize)
        {
            Log::Error(L"%sInvalid image \"%s\": \"%s\", %dx%d, %dx%db %s, data %dB",
                       logPrefix.c_str(),
                       Utils::StringToWstring(image.name).c_str(),
                       Utils::StringToWstring(image.uri).c_str(),
                       image.width,
                       image.height,
                       image.component,
                       image.bits,
                       GltfUtils::ComponentTypeToWstring(image.pixel_type).c_str(),
                       image.image.size());
            return false;
        }

        std::vector<unsigned char> floatImage;
        if (!SceneUtils::ConvertImageToFloat(floatImage, image, mConstFactor))
        {
            Log::Error(L"%sFailed to convert image \"%s\" to float format: \"%s\", %dx%d, %dx%db %s, data %dB",
                       logPrefix.c_str(),
                       Utils::StringToWstring(image.name).c_str(),
                       Utils::StringToWstring(image.uri).c_str(),
                       image.width,
                       image.height,
                       image.component,
                       image.bits,
                       GltfUtils::ComponentTypeToWstring(image.pixel_type).c_str(),
                       image.image.size());
            return false;
        }

        if (!SceneUtils::CreateTextureSrvFromData(ctx,
                                                  srv,
                                                  image.width,
                                                  image.height,
                                                  DXGI_FORMAT_R32G32B32A32_FLOAT,
                                                  floatImage.data(),
                                                  image.width * 4 * sizeof(float)))
        {
            Log::Error(L"%sFailed to create texture & SRV for image \"%s\": \"%s\", %dx%d",
                       logPrefix.c_str(),
                       Utils::StringToWstring(image.name).c_str(),
                       Utils::StringToWstring(image.uri).c_str(),
                       image.width,
                       image.height);
            return false;
        }

        // TODO: Sampler
    }
    else
    {
        // Constant texture
        if (!SceneUtils::CreateConstantTextureSRV(ctx, srv, mConstFactor))
        {
            Log::Error(L"%sFailed to create constant texture for \"%s\"!",
                       logPrefix.c_str(),
                       Utils::StringToWstring(paramName).c_str());
            return false;
        }
    }

    return true;
}


SceneMaterial::SceneMaterial() :
    mWorkflow(MaterialWorkflow::eNone),

    mBaseColorTexture(XMFLOAT4(1.f, 1.f, 1.f, 1.f)),
    mMetallicRoughnessTexture(XMFLOAT4(1.f, 1.f, 1.f, 1.f)),

    mSpecularTexture(XMFLOAT4(1.f, 1.f, 1.f, 1.f))
{}


bool SceneMaterial::CreatePbrSpecularity(IRenderingContext &ctx,
                                         const wchar_t * diffuseTexPath,
                                         XMFLOAT4 diffuseConstFactor,
                                         const wchar_t * specularTexPath,
                                         XMFLOAT4 specularConstFactor)
{
    if (!mBaseColorTexture.Create(ctx, diffuseTexPath, diffuseConstFactor))
        return false;

    if (!mSpecularTexture.Create(ctx, specularTexPath, specularConstFactor))
        return false;

    mWorkflow = MaterialWorkflow::kPbrSpecularity;

    return true;
}

bool SceneMaterial::CreatePbrMetalness(IRenderingContext &ctx,
                                       const wchar_t * baseColorTexPath,
                                       XMFLOAT4 baseColorConstFactor,
                                       const wchar_t * metallicRoughnessTexPath,
                                       float metallicConstFactor,
                                       float roughnessConstFactor)
{
    if (!mBaseColorTexture.Create(ctx,
                                  baseColorTexPath,
                                  baseColorConstFactor))
        return false;

    if (!mMetallicRoughnessTexture.Create(ctx,
                                          metallicRoughnessTexPath,
                                          XMFLOAT4(0.f, roughnessConstFactor, metallicConstFactor, 0.f)))
        return false;

    mWorkflow = MaterialWorkflow::kPbrMetalness;

    return true;
}

bool SceneMaterial::LoadFromGltf(IRenderingContext &ctx,
                                 const tinygltf::Model &model,
                                 const tinygltf::Material &material,
                                 const std::wstring &logPrefix)
{
    if (Log::sLoggingLevel >= Log::eDebug)
    {
        for (const auto &value : material.values)
        {
            Log::Debug(L"%s%s: %s",
                       logPrefix.c_str(),
                       Utils::StringToWstring(value.first).c_str(),
                       GltfUtils::ParameterValueToWstring(value.second).c_str());
        }
        for (const auto &value : material.additionalValues)
        {
            Log::Debug(L"%s%s*: %s",
                       logPrefix.c_str(),
                       Utils::StringToWstring(value.first).c_str(),
                       GltfUtils::ParameterValueToWstring(value.second).c_str());
        }
        Log::Debug(L"%s---", logPrefix.c_str());
    }

    auto &values = material.values;

    if (!mBaseColorTexture.LoadFloat4FactorFromGltf("baseColorFactor", values, logPrefix))
        return false;
    if (!mBaseColorTexture.LoadTextureFromGltf("baseColorTexture", ctx, model, values, logPrefix))
        return false;

    if (!mMetallicRoughnessTexture.LoadFloatFactorFromGltf("metallicFactor", 2, values, logPrefix))
        return false;
    if (!mMetallicRoughnessTexture.LoadFloatFactorFromGltf("roughnessFactor", 1, values, logPrefix))
        return false;
    if (!mMetallicRoughnessTexture.LoadTextureFromGltf("metallicRoughnessTexture", ctx, model, values, logPrefix))
        return false;

    mWorkflow = MaterialWorkflow::kPbrMetalness;

    return true;
};
