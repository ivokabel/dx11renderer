#include "scene.hpp"

#include "tangent_calculator.hpp"
#include "scene_utils.hpp"
#include "gltf_utils.hpp"
#include "utils.hpp"
#include "log.hpp"

#include "Libs/tinygltf-2.5.0/tiny_gltf.h" // just the interfaces (no implementation)

#include <cassert>
#include <array>
#include <vector>

#define UNUSED_COLOR XMFLOAT4(1.f, 0.f, 1.f, 1.f)
#define STRIP_BREAK static_cast<uint32_t>(-1)

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
#define AUTO_ALIGN      D3D11_APPEND_ALIGNED_ELEMENT
#define VERTEX_DATA     D3D11_INPUT_PER_VERTEX_DATA
const std::vector<InputElmDesc> sVertexLayoutDesc =
{
    InputElmDesc{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, AUTO_ALIGN, VERTEX_DATA, 0 },
    InputElmDesc{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, AUTO_ALIGN, VERTEX_DATA, 0 },
    InputElmDesc{ "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, AUTO_ALIGN, VERTEX_DATA, 0 },
    InputElmDesc{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, AUTO_ALIGN, VERTEX_DATA, 0 },
};

struct CbScene
{
    XMMATRIX ViewMtrx;
    XMFLOAT4 CameraPos;
    XMMATRIX ProjectionMtrx;
};

struct CbFrame
{
    XMFLOAT4 AmbientLightLuminance;

    XMFLOAT4 DirectLightDirs[DIRECT_LIGHTS_MAX_COUNT];
    XMFLOAT4 DirectLightLuminances[DIRECT_LIGHTS_MAX_COUNT];

    XMFLOAT4 PointLightPositions[POINT_LIGHTS_MAX_COUNT];
    XMFLOAT4 PointLightIntensities[POINT_LIGHTS_MAX_COUNT];

    int32_t  DirectLightsCount; // at the end to avoid 16-byte packing issues
    int32_t  PointLightsCount;  // at the end to avoid 16-byte packing issues
    int32_t  dummy_padding[2];  // padding to 16 bytes multiple
};

struct CbSceneNode
{
    XMMATRIX WorldMtrx;
    XMFLOAT4 MeshColor; // May be eventually replaced by the emmisive component of the standard surface shader
};

struct CbScenePrimitive
{
    // Metallness
    XMFLOAT4 BaseColorFactor;
    XMFLOAT4 MetallicRoughnessFactor;

    // Specularity
    XMFLOAT4 DiffuseColorFactor;
    XMFLOAT4 SpecularFactor;

    // Both workflows
    float    NormalTexScale;
    float    OcclusionTexStrength;
    float    padding[2];  // padding to 16 bytes
    XMFLOAT4 EmissionFactor;
};

Scene::Scene(const SceneId sceneId) :
    mSceneId(sceneId)
{
    mViewData.eye = XMVectorSet(0.0f,  4.0f, 10.0f, 1.0f);
    mViewData.at  = XMVectorSet(0.0f, -0.2f,  0.0f, 1.0f);
    mViewData.up  = XMVectorSet(0.0f,  1.0f,  0.0f, 1.0f);
}

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
    hr = device->CreateInputLayout(sVertexLayoutDesc.data(),
                                   (UINT)sVertexLayoutDesc.size(),
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
    bd.ByteWidth = sizeof(CbScene);
    hr = device->CreateBuffer(&bd, nullptr, &mCbScene);
    if (FAILED(hr))
        return false;
    bd.ByteWidth = sizeof(CbFrame);
    hr = device->CreateBuffer(&bd, nullptr, &mCbFrame);
    if (FAILED(hr))
        return hr;
    bd.ByteWidth = sizeof(CbSceneNode);
    hr = device->CreateBuffer(&bd, nullptr, &mCbSceneNode);
    if (FAILED(hr))
        return hr;
    bd.ByteWidth = sizeof(CbScenePrimitive);
    hr = device->CreateBuffer(&bd, nullptr, &mCbScenePrimitive);
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

    // Matrices
    mViewMtrx = XMMatrixLookAtLH(mViewData.eye, mViewData.at, mViewData.up);
    mProjectionMtrx = XMMatrixPerspectiveFovLH(XM_PIDIV4,
                                               (FLOAT)wndWidth / wndHeight,
                                               0.01f, 100.0f);

    // Scene constant buffer can be updated now
    CbScene cbScene;
    cbScene.ViewMtrx = XMMatrixTranspose(mViewMtrx);
    XMStoreFloat4(&cbScene.CameraPos, mViewData.eye);
    cbScene.ProjectionMtrx = XMMatrixTranspose(mProjectionMtrx);
    immCtx->UpdateSubresource(mCbScene, 0, NULL, &cbScene, 0, 0);

    return true;
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

    SetupDefaultLights();

    Log::Debug(L"");

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


const SceneMaterial& Scene::GetMaterial(const ScenePrimitive &primitive) const
{
    const int idx = primitive.GetMaterialIdx();

    if (idx >= 0 && idx < mMaterials.size())
        return mMaterials[idx];
    else
        return mDefaultMaterial;
}


void Scene::Destroy()
{
    Utils::ReleaseAndMakeNull(mVertexShader);

    Utils::ReleaseAndMakeNull(mPsPbrMetalness);
    Utils::ReleaseAndMakeNull(mPsPbrSpecularity);
    Utils::ReleaseAndMakeNull(mPsConstEmmisive);

    Utils::ReleaseAndMakeNull(mVertexLayout);

    Utils::ReleaseAndMakeNull(mCbScene);
    Utils::ReleaseAndMakeNull(mCbFrame);
    Utils::ReleaseAndMakeNull(mCbSceneNode);
    Utils::ReleaseAndMakeNull(mCbScenePrimitive);

    Utils::ReleaseAndMakeNull(mSamplerLinear);

    mRootNodes.clear();
    mPointLightProxy.Destroy();
}


void Scene::AnimateFrame(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    // debug: Materials
    for (auto &material : mMaterials)
        material.Animate(ctx);

    // Scene geometry
    for (auto &node : mRootNodes)
        node.Animate(ctx);

    // Directional lights (are steady for now)
    for (auto &dirLight : mDirectLights)
        dirLight.dirTransf = dirLight.dir;

    // Animate point lights (harwired animation for now)

    const float time = ctx.GetFrameAnimationTime();
    const float period = 15.f; //seconds
    const float totalAnimPos = time / period;
    const float angle = totalAnimPos * XM_2PI;

    const auto pointCount = mPointLights.size();
    for (int i = 0; i < pointCount; i++)
    {
        const float lightRelOffsetCircular = (float)i / pointCount;
        const float lightRelOffsetInterval = (float)i / (pointCount - 1);

        const float rotationAngle = -2.f * angle - lightRelOffsetCircular * XM_2PI;
        const float orbitInclination = Utils::Lerp(mPointLights[i].orbitInclinationMin,
                                                   mPointLights[i].orbitInclinationMax,
                                                   lightRelOffsetInterval);

        const XMMATRIX translationMtrx  = XMMatrixTranslation(mPointLights[i].orbitRadius, 0.f, 0.f);
        const XMMATRIX rotationMtrx     = XMMatrixRotationY(rotationAngle);
        const XMMATRIX inclinationMtrx  = XMMatrixRotationZ(orbitInclination);
        const XMMATRIX transfMtrx = translationMtrx * rotationMtrx * inclinationMtrx;

        const XMFLOAT4 basePos{ 0.f, 0.f, 0.f, 0.f };
        const XMVECTOR lightVec = XMLoadFloat4(&basePos);
        const XMVECTOR lightVecTransf = XMVector3Transform(lightVec, transfMtrx);
        XMStoreFloat4(&mPointLights[i].posTransf, lightVecTransf);
    }
}


void Scene::RenderFrame(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    if (mPointLights.size() > POINT_LIGHTS_MAX_COUNT)
        return;
    if (mDirectLights.size() > DIRECT_LIGHTS_MAX_COUNT)
        return;

    auto immCtx = ctx.GetImmediateContext();

    // Frame constant buffer
    CbFrame cbFrame;
    cbFrame.AmbientLightLuminance = mAmbientLight.luminance;
    cbFrame.DirectLightsCount = (int32_t)mDirectLights.size();
    for (int i = 0; i < cbFrame.DirectLightsCount; i++)
    {
        cbFrame.DirectLightDirs[i]       = mDirectLights[i].dirTransf;
        cbFrame.DirectLightLuminances[i] = mDirectLights[i].luminance;
    }
    cbFrame.PointLightsCount = (int32_t)mPointLights.size();
    for (int i = 0; i < cbFrame.PointLightsCount; i++)
    {
        cbFrame.PointLightPositions[i]   = mPointLights[i].posTransf;
        cbFrame.PointLightIntensities[i] = mPointLights[i].intensity;
    }
    immCtx->UpdateSubresource(mCbFrame, 0, nullptr, &cbFrame, 0, 0);

    // Setup vertex shader
    immCtx->VSSetShader(mVertexShader, nullptr, 0);
    immCtx->VSSetConstantBuffers(0, 1, &mCbScene);
    immCtx->VSSetConstantBuffers(1, 1, &mCbFrame);
    immCtx->VSSetConstantBuffers(2, 1, &mCbSceneNode);

    // Setup pixel shader data (shader itself is chosen later for each material)
    immCtx->PSSetConstantBuffers(0, 1, &mCbScene);
    immCtx->PSSetConstantBuffers(1, 1, &mCbFrame);
    immCtx->PSSetConstantBuffers(2, 1, &mCbSceneNode);
    immCtx->PSSetConstantBuffers(3, 1, &mCbScenePrimitive);
    immCtx->PSSetSamplers(0, 1, &mSamplerLinear);

    // Scene geometry
    for (auto &node : mRootNodes)
        RenderNode(ctx, node, XMMatrixIdentity());

    // Proxy geometry for point lights
    for (int i = 0; i < mPointLights.size(); i++)
    {
        CbSceneNode cbSceneNode;

        const float radius = 0.07f;
        XMMATRIX lightScaleMtrx = XMMatrixScaling(radius, radius, radius);
        XMMATRIX lightTrnslMtrx = XMMatrixTranslationFromVector(XMLoadFloat4(&mPointLights[i].posTransf));
        XMMATRIX lightMtrx = lightScaleMtrx * lightTrnslMtrx;
        cbSceneNode.WorldMtrx = XMMatrixTranspose(lightMtrx);

        const float radius2 = radius * radius;
        cbSceneNode.MeshColor = {
            mPointLights[i].intensity.x / radius2,
            mPointLights[i].intensity.y / radius2,
            mPointLights[i].intensity.z / radius2,
            mPointLights[i].intensity.w / radius2,
        };

        immCtx->UpdateSubresource(mCbSceneNode, 0, nullptr, &cbSceneNode, 0, 0);

        immCtx->PSSetShader(mPsConstEmmisive, nullptr, 0);
        mPointLightProxy.DrawGeometry(ctx, mVertexLayout);
    }
}


void Scene::SetupDefaultLights()
{
    const uint8_t amb = 120;
    mAmbientLight.luminance = SceneUtils::SrgbColorToFloat(amb, amb, amb);

    const float lum = 3.0f;
    mDirectLights.resize(1);
    mDirectLights[0].dir = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
    mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);

    SetupPointLights(3);
}


bool Scene::SetupPointLights(size_t count,
                             float intensity,
                             float orbitRadius,
                             float orbitInclMin,
                             float orbitInclMax)
{
    if (count > POINT_LIGHTS_MAX_COUNT)
    {
        Log::Error(L"SetupPointLights: requested number of point lights (%d) exceeds the limit (%d)", count, POINT_LIGHTS_MAX_COUNT);
        return false;
    }

    mPointLights.resize(count);

    for (auto &light : mPointLights)
    {
        light.intensity = XMFLOAT4(intensity, intensity, intensity, 1.0f);
        light.orbitRadius = orbitRadius;
        light.orbitInclinationMin = orbitInclMin;
        light.orbitInclinationMax = orbitInclMax;
    }

    return true;
}


bool Scene::SetupPointLights(const std::initializer_list<XMFLOAT4> &intensities,
                             float orbitRadius,
                             float orbitInclMin,
                             float orbitInclMax)
{
    if (intensities.size() > POINT_LIGHTS_MAX_COUNT)
    {
        Log::Error(L"SetupPointLights: requested number of point lights (%d) exceeds the limit (%d)",
                   intensities.size(), POINT_LIGHTS_MAX_COUNT);
        return false;
    }

    if (intensities.size() != intensities.size())
    {
        Log::Error(L"SetupPointLights: provided intensities count (%d) doesn't match the light count (%d)",
                   intensities.size(), POINT_LIGHTS_MAX_COUNT);
        return false;
    }

    mPointLights.resize(intensities.size());

    auto itLight = mPointLights.begin();
    auto itIntensity = intensities.begin();
    for (; itLight != mPointLights.end() && itIntensity != intensities.end(); ++itLight, ++itIntensity)
    {
        auto &light = *itLight;

        light.intensity = *itIntensity;
        light.orbitRadius = orbitRadius;
        light.orbitInclinationMin = orbitInclMin;
        light.orbitInclinationMax = orbitInclMax;
    }

    return true;
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

    // Per-node constant buffer
    CbSceneNode cbSceneNode;
    cbSceneNode.WorldMtrx = XMMatrixTranspose(worldMtrx);
    cbSceneNode.MeshColor = { 0.f, 1.f, 0.f, 1.f, };
    immCtx->UpdateSubresource(mCbSceneNode, 0, nullptr, &cbSceneNode, 0, 0);

    // Draw current node
    for (auto &primitive : node.mPrimitives)
    {
        auto &material = GetMaterial(primitive);

        switch (material.GetWorkflow())
        {
        case MaterialWorkflow::kPbrMetalness:
        {
            immCtx->PSSetShader(mPsPbrMetalness, nullptr, 0);
            immCtx->PSSetShaderResources(0, 1, &material.GetBaseColorTexture().srv);
            immCtx->PSSetShaderResources(1, 1, &material.GetMetallicRoughnessTexture().srv);
            immCtx->PSSetShaderResources(4, 1, &material.GetNormalTexture().srv);
            immCtx->PSSetShaderResources(5, 1, &material.GetOcclusionTexture().srv);
            immCtx->PSSetShaderResources(6, 1, &material.GetEmissionTexture().srv);

            CbScenePrimitive cbScenePrimitive;
            cbScenePrimitive.BaseColorFactor            = material.GetBaseColorFactor();
            cbScenePrimitive.MetallicRoughnessFactor    = material.GetMetallicRoughnessFactor();
            cbScenePrimitive.DiffuseColorFactor         = UNUSED_COLOR;
            cbScenePrimitive.SpecularFactor             = UNUSED_COLOR;
            cbScenePrimitive.NormalTexScale             = material.GetNormalTexture().GetScale();
            cbScenePrimitive.OcclusionTexStrength       = material.GetOcclusionTexture().GetStrength();
            cbScenePrimitive.EmissionFactor             = material.GetEmissionFactor();
            immCtx->UpdateSubresource(mCbScenePrimitive, 0, nullptr, &cbScenePrimitive, 0, 0);
            break;
        }
        case MaterialWorkflow::kPbrSpecularity:
        {
            immCtx->PSSetShader(mPsPbrSpecularity, nullptr, 0);
            immCtx->PSSetShaderResources(2, 1, &material.GetBaseColorTexture().srv);
            immCtx->PSSetShaderResources(3, 1, &material.GetSpecularTexture().srv);
            immCtx->PSSetShaderResources(4, 1, &material.GetNormalTexture().srv);
            immCtx->PSSetShaderResources(5, 1, &material.GetOcclusionTexture().srv);
            immCtx->PSSetShaderResources(6, 1, &material.GetEmissionTexture().srv);

            CbScenePrimitive cbScenePrimitive;
            cbScenePrimitive.DiffuseColorFactor         = material.GetBaseColorFactor();
            cbScenePrimitive.SpecularFactor             = material.GetSpecularFactor();
            cbScenePrimitive.BaseColorFactor            = UNUSED_COLOR;
            cbScenePrimitive.MetallicRoughnessFactor    = UNUSED_COLOR;
            cbScenePrimitive.NormalTexScale             = material.GetNormalTexture().GetScale();
            cbScenePrimitive.OcclusionTexStrength       = material.GetOcclusionTexture().GetStrength();
            cbScenePrimitive.EmissionFactor             = material.GetEmissionFactor();
            immCtx->UpdateSubresource(mCbScenePrimitive, 0, nullptr, &cbScenePrimitive, 0, 0);
            break;
        }
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
    mIsTangentPresent(src.mIsTangentPresent),
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
    mIsTangentPresent(Utils::Exchange(src.mIsTangentPresent, false)),
    mTopology(Utils::Exchange(src.mTopology, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)),
    mVertexBuffer(Utils::Exchange(src.mVertexBuffer, nullptr)),
    mIndexBuffer(Utils::Exchange(src.mIndexBuffer, nullptr)),
    mMaterialIdx(Utils::Exchange(src.mMaterialIdx, -1))
{}

ScenePrimitive& ScenePrimitive::operator =(const ScenePrimitive &src)
{
    mVertices = src.mVertices;
    mIndices = src.mIndices;
    mIsTangentPresent = src.mIsTangentPresent;
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
    mIsTangentPresent = Utils::Exchange(src.mIsTangentPresent, false);
    mTopology = Utils::Exchange(src.mTopology, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    mVertexBuffer = Utils::Exchange(src.mVertexBuffer, nullptr);
    mIndexBuffer = Utils::Exchange(src.mIndexBuffer, nullptr);

    mMaterialIdx = Utils::Exchange(src.mMaterialIdx, -1);

    return *this;
}

ScenePrimitive::~ScenePrimitive()
{
    Destroy();
}


bool ScenePrimitive::CreateQuad(IRenderingContext & ctx)
{
    if (!GenerateQuadGeometry())
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;

    return true;
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


bool ScenePrimitive::GenerateQuadGeometry()
{
    mVertices =
    {
        SceneVertex{ XMFLOAT3(-1.0f, 0.0f, -1.0f),  XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3( 1.0f, 0.0f, -1.0f),  XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3( 1.0f, 0.0f,  1.0f),  XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f, 0.0f,  1.0f),  XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 1.0f) },
    };
    
    mIndices =
    {
        3, 1, 0,
        2, 1, 3,
    };

    mTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    CalculateTangentsIfNeeded();

    return true;
}


bool ScenePrimitive::GenerateCubeGeometry()
{
    mVertices =
    {
        // Up
        SceneVertex{ XMFLOAT3(-1.0f, 1.0f, -1.0f),  XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, 1.0f, -1.0f),   XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, 1.0f,  1.0f),   XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f, 1.0f,  1.0f),  XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 1.0f) },

        // Down
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, -1.0f, -1.0f),  XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, -1.0f,  1.0f),  XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 1.0f) },

        // Side 1
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 1.0f) },

        // Side 3
        SceneVertex{ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 1.0f) },

        // Side 2
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, -1.0f, -1.0f),  XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f,  1.0f, -1.0f),  XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 1.0f) },

        // Side 4
        SceneVertex{ XMFLOAT3(-1.0f, -1.0f, 1.0f),  XMFLOAT3(0.0f, 0.0f, 1.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f, -1.0f, 1.0f),   XMFLOAT3(0.0f, 0.0f, 1.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 0.0f) },
        SceneVertex{ XMFLOAT3(1.0f,  1.0f, 1.0f),   XMFLOAT3(0.0f, 0.0f, 1.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 1.0f) },
        SceneVertex{ XMFLOAT3(-1.0f,  1.0f, 1.0f),  XMFLOAT3(0.0f, 0.0f, 1.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 1.0f) },
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

    CalculateTangentsIfNeeded();

    return true;
}


bool ScenePrimitive::GenerateOctahedronGeometry()
{
    mVertices =
    {
        // Noth pole
        SceneVertex{ XMFLOAT3( 0.0f, 1.0f, 0.0f),  XMFLOAT3( 0.0f, 1.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.0f, 0.0f) },

        // Points on equator
        SceneVertex{ XMFLOAT3( 1.0f, 0.0f, 0.0f),  XMFLOAT3( 1.0f, 0.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.00f, 0.5f) },
        SceneVertex{ XMFLOAT3( 0.0f, 0.0f, 1.0f),  XMFLOAT3( 0.0f, 0.0f, 1.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.25f, 0.5f) },
        SceneVertex{ XMFLOAT3(-1.0f, 0.0f, 0.0f),  XMFLOAT3(-1.0f, 0.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.50f, 0.5f) },
        SceneVertex{ XMFLOAT3( 0.0f, 0.0f,-1.0f),  XMFLOAT3( 0.0f, 0.0f,-1.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(0.75f, 0.5f) },

        // South pole
        SceneVertex{ XMFLOAT3( 0.0f,-1.0f, 0.0f),  XMFLOAT3( 0.0f,-1.0f, 0.0f),  XMFLOAT4(0,0,0,0), XMFLOAT2(1.0f, 1.0f) },
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

    CalculateTangentsIfNeeded();

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
            mVertices.push_back(SceneVertex{ pt, pt,  XMFLOAT4(0,0,0,0), XMFLOAT2(uLine, v) }); // position==normal
        }

        // Poles
        const XMFLOAT3 northPole(0.0f,  1.0f, 0.0f);
        const XMFLOAT3 southPole(0.0f, -1.0f, 0.0f);
        const float uPole = uLine + stripSizeRel / 2;
        mVertices.push_back(SceneVertex{ northPole,  northPole,  XMFLOAT4(0,0,0,0), XMFLOAT2(uPole, 0.0f) }); // position==normal
        mVertices.push_back(SceneVertex{ southPole,  southPole,  XMFLOAT4(0,0,0,0), XMFLOAT2(uPole, 1.0f) }); // position==normal
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
        mIndices.push_back(STRIP_BREAK);
    }

    assert(mIndices.size() == indexCount);

    mTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    //mTopology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; // debug

    CalculateTangentsIfNeeded();

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
    const auto &attrs = primitive.attributes;
    const auto subItemsLogPrefix = logPrefix + L"   ";
    const auto dataConsumerLogPrefix = subItemsLogPrefix + L"   ";

    Log::Debug(L"%sPrimitive %d/%d: mode %s, attributes [%s], indices %d, material %d",
               logPrefix.c_str(),
               primitiveIdx,
               mesh.primitives.size(),
               GltfUtils::ModeToWstring(primitive.mode).c_str(),
               GltfUtils::StringIntMapToWstring(primitive.attributes).c_str(),
               primitive.indices,
               primitive.material);

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
        mVertices.clear();
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
                                         XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f),  // debug; TODO: Leave invalid?
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

    // Tangents
    auto &tangentAccessor = GetPrimitiveAttrAccessor(success, model, attrs, primitiveIdx,
                                                     false, "TANGENT", subItemsLogPrefix.c_str());
    if (success)
    {
        if ((tangentAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) ||
            (tangentAccessor.type != TINYGLTF_TYPE_VEC4))
        {
            Log::Error(L"%sUnsupported TANGENT data type!", subItemsLogPrefix.c_str());
            return false;
        }

        if (tangentAccessor.count != posAccessor.count)
        {
            Log::Error(L"%sTangents count (%d) is different from position count (%d)!",
                       subItemsLogPrefix.c_str(), tangentAccessor.count, posAccessor.count);
            return false;
        }

        auto TangentDataConsumer = [this, &dataConsumerLogPrefix](int itemIdx, const unsigned char *ptr)
        {
            auto tangent = *reinterpret_cast<const XMFLOAT4*>(ptr);

            //Log::Debug(L"%s%d: tangent [%7.4f, %7.4f, %7.4f] * %.1f",
            //           dataConsumerLogPrefix.c_str(), itemIdx,
            //           tangent.x, tangent.y, tangent.z, tangent.w);

            if ((tangent.w != 1.f) && (tangent.w != -1.f))
                Log::Warning(L"%s%d: tangent w component (handedness) is not equal to 1 or -1 but to %7.4f",
                           dataConsumerLogPrefix.c_str(), itemIdx, tangent.w);

            mVertices[itemIdx].Tangent = tangent;
        };

        if (!IterateGltfAccesorData<float, 4>(model,
                                              tangentAccessor,
                                              TangentDataConsumer,
                                              subItemsLogPrefix.c_str(),
                                              L"Tangent"))
            return false;

        mIsTangentPresent = true;
    }
    else
    {
        Log::Debug(L"%sTangents are not present", subItemsLogPrefix.c_str());
    }

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

    // TODO: Wrap into IterateGltfAccesorData(componentType, ...)? std::forward()?
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
    if (mTopology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
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

    CalculateTangentsIfNeeded(subItemsLogPrefix);

    return true;
}


bool ScenePrimitive::CalculateTangentsIfNeeded(const std::wstring &logPrefix)
{
    // TODO: if (material needs tangents && are not present) ... GetMaterial()
    // TODO: Requires position, normal, and texcoords
    // TODO: Only for triangles?
    if (!IsTangentPresent())
    {
        Log::Debug(L"%sComputing tangents...", logPrefix.c_str());

        if (!TangentCalculator::Calculate(*this))
        {
            Log::Error(L"%sTangents computation failed!", logPrefix.c_str());
            return false;
        }
        mIsTangentPresent = true;
    }

    return true;
}

size_t ScenePrimitive::GetVerticesPerFace() const
{
    switch (mTopology)
    {
    case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
        return 1;
    case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
        return 2;
    case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
        return 2;
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
        return 3;
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
        return 3;
    default:
        return 0;
    }
}


size_t ScenePrimitive::GetFacesCount() const
{
    FillFaceStripsCacheIfNeeded();

    switch (mTopology)
    {
    case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
    case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
        return mIndices.size() / GetVerticesPerFace();

    case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
        return mFaceStripsTotalCount;

    default:
        return 0; // Unsupported
    }
}


void ScenePrimitive::FillFaceStripsCacheIfNeeded() const
{
    if (mAreFaceStripsCached)
        return;

    switch (mTopology)
    {
    case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
    {
        mFaceStrips.clear();

        const auto count = mIndices.size();
        for (size_t i = 0; i < count; )
        {
            // Start
            while ((i < count) && (mIndices[i] == STRIP_BREAK))
            {
                ++i;
            }
            const size_t start = i;

            // Length
            size_t length = 0;
            while ((i < count) && (mIndices[i] != STRIP_BREAK))
            {
                ++length;
                ++i;
            }

            // Strip
            if (length >= GetVerticesPerFace())
            {
                const auto faceCount = length - (GetVerticesPerFace() - 1);
                mFaceStrips.push_back({ start, faceCount });
            }
        }

        mFaceStripsTotalCount = 0;
        for (const auto &strip : mFaceStrips)
            mFaceStripsTotalCount += strip.faceCount;

        mAreFaceStripsCached = true;
        return;
    }

    case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
    case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
    default:
        return;
    }
}


const size_t ScenePrimitive::GetVertexIndex(const int face, const int vertex) const
{
    if (vertex >= GetVerticesPerFace())
        return 0;

    switch (mTopology)
    {
    case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
    case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
    case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
        return vertex;

    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
    {
        const bool isOdd = (face % 2 == 1);

        if (isOdd && (vertex == 1))
            return 2;
        else if (isOdd && (vertex == 2))
            return 1;
        else
            return vertex;
    }

    default:
        return 0; // Unsupported
    }
}


const SceneVertex& ScenePrimitive::GetVertex(const int face, const int vertex) const
{
    static const SceneVertex invalidVert{};

    FillFaceStripsCacheIfNeeded();

    if (vertex >= GetVerticesPerFace())
        return invalidVert;

    switch (mTopology)
    {
    case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
    case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
    {
        const auto idx = face * GetVerticesPerFace() + vertex;
        if ((idx < 0) || (idx >= mIndices.size()))
            return invalidVert;
        return mVertices[mIndices[idx]];
    }

    case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
    {
        if (!mAreFaceStripsCached)
            return invalidVert;
        if (face >= mFaceStripsTotalCount)
            return invalidVert;

        // Strip
        // (naive impl for now, could be done in log time using cumulative counts and binary search)
        size_t strip = 0;
        size_t skippedFaces = 0;
        for (; strip < mFaceStrips.size(); strip++)
        {
            const auto currentFaceCount = mFaceStrips[strip].faceCount;
            if (face < (skippedFaces + currentFaceCount))
                break; // found
            skippedFaces += currentFaceCount;
        }
        if (strip >= mFaceStrips.size())
            return invalidVert;

        // Face & vertex
        const auto faceIdx   = face - skippedFaces;
        const auto vertexIdx = GetVertexIndex((int)faceIdx, vertex);
        const auto idx = mFaceStrips[strip].startIdx + faceIdx + vertexIdx;
        if ((idx < 0) || (idx >= mIndices.size()))
            return invalidVert;
        return mVertices[mIndices[idx]];
    }

    default:
        return invalidVert; // Unsupported
    }
}


SceneVertex& ScenePrimitive::GetVertex(const int face, const int vertex)
{
    return
        const_cast<SceneVertex &>(
            static_cast<const ScenePrimitive&>(*this).
                GetVertex(face, vertex));
}


void ScenePrimitive::GetPosition(float outpos[],
                                 const int face,
                                 const int vertex) const
{
    const auto &pos = GetVertex(face, vertex).Pos;
    outpos[0] = pos.x;
    outpos[1] = pos.y;
    outpos[2] = pos.z;
}


void ScenePrimitive::GetNormal(float outnormal[],
                               const int face,
                               const int vertex) const
{
    const auto &normal = GetVertex(face, vertex).Normal;
    outnormal[0] = normal.x;
    outnormal[1] = normal.y;
    outnormal[2] = normal.z;
}


void ScenePrimitive::GetTextCoord(float outuv[],
                                  const int face,
                                  const int vertex) const
{
    const auto &tex = GetVertex(face, vertex).Tex;
    outuv[0] = tex.x;
    outuv[1] = tex.y;
}


void ScenePrimitive::SetTangent(const float intangent[],
                                const float sign,
                                const int face,
                                const int vertex)
{
    auto &tangent = GetVertex(face, vertex).Tangent;
    tangent.x = intangent[0];
    tangent.y = intangent[1];
    tangent.z = intangent[2];
    tangent.w = sign;
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
    mTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
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


SceneTexture::SceneTexture(const std::wstring &name,
                           ValueType valueType,
                           XMFLOAT4 neutralValue) :
    mName(name),
    mValueType(valueType),
    mNeutralValue(neutralValue),
    mIsLoaded(false),
    srv(nullptr)
{}

SceneTexture::SceneTexture(const SceneTexture &src) :
    mName(src.mName),
    mValueType(src.mValueType),
    mNeutralValue(src.mNeutralValue),
    mIsLoaded(src.mIsLoaded),
    srv(src.srv)
{
    // We are creating new reference of device resource
    Utils::SafeAddRef(srv);
}

SceneTexture& SceneTexture::operator =(const SceneTexture &src)
{
    mName           = src.mName;
    mValueType      = src.mValueType;
    mNeutralValue   = src.mNeutralValue;
    mIsLoaded       = src.mIsLoaded;
    srv             = src.srv;

    // We are creating new reference of device resource
    Utils::SafeAddRef(srv);

    return *this;
}

SceneTexture::SceneTexture(SceneTexture &&src) :
    mName(src.mName),
    mValueType(src.mValueType),
    mNeutralValue(Utils::Exchange(src.mNeutralValue, XMFLOAT4(0.f, 0.f, 0.f, 0.f))),
    mIsLoaded(Utils::Exchange(src.mIsLoaded, false)),
    srv(Utils::Exchange(src.srv, nullptr))
{}

SceneTexture& SceneTexture::operator =(SceneTexture &&src)
{
    mName           = src.mName;
    mValueType      = src.mValueType;
    mNeutralValue   = Utils::Exchange(src.mNeutralValue, XMFLOAT4(0.f, 0.f, 0.f, 0.f));
    mIsLoaded       = Utils::Exchange(src.mIsLoaded, false);
    srv             = Utils::Exchange(src.srv, nullptr);

    return *this;
}

SceneTexture::~SceneTexture()
{
    Utils::ReleaseAndMakeNull(srv);
}


bool SceneTexture::Create(IRenderingContext &ctx, const wchar_t *path)
{
    auto device = ctx.GetDevice();
    if (!device)
        return false;

    HRESULT hr = S_OK;

    if (path)
    {
        D3DX11_IMAGE_LOAD_INFO ili;
        ili.Usage = D3D11_USAGE_IMMUTABLE;
#ifdef CONVERT_SRGB_INPUT_TO_LINEAR
        if (mValueType == SceneTexture::eSrgb)
        {
            ili.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            ili.Filter = D3DX11_FILTER_SRGB | D3DX11_FILTER_NONE;
        }
        else
#endif
        {
            ili.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        hr = D3DX11CreateShaderResourceViewFromFile(device, path, &ili, nullptr, &srv, nullptr);
        if (FAILED(hr))
            return false;
    }
    else
    {
        // Neutral constant texture
        if (!CreateNeutral(ctx))
            return false;
    }

    return true;
}


bool SceneTexture::CreateNeutral(IRenderingContext &ctx)
{
    return SceneUtils::CreateConstantTextureSRV(ctx, srv, mNeutralValue);
}


bool SceneTexture::LoadTextureFromGltf(const int textureIndex,
                                       IRenderingContext &ctx,
                                       const tinygltf::Model &model,
                                       const std::wstring &logPrefix)
{
    const auto &textures = model.textures;
    const auto &images = model.images;

    if (textureIndex >= (int)textures.size())
    {
        Log::Error(L"%sInvalid texture index (%d/%d) in \"%s\""
                   L"!",
                   logPrefix.c_str(),
                   textureIndex,
                   textures.size(),
                   GetName().c_str()
        );
        return false;
    }

    if (textureIndex < 0)
    {
        // No texture - load neutral constant one

        Log::Debug(L"%s%s: Not specified - creating neutral constant texture",
                   logPrefix.c_str(),
                   GetName().c_str()
        );

        if (!SceneUtils::CreateConstantTextureSRV(ctx, srv, mNeutralValue))
        {
            Log::Error(L"%sFailed to create neutral constant texture for \"%s\"!",
                       logPrefix.c_str(),
                       GetName().c_str());
            return false;
        }

        return true;
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

    Log::Debug(L"%s%s: \"%s\"/\"%s\", %dx%d, %dx%db %s, data size %dB",
               logPrefix.c_str(),
               GetName().c_str(),
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
        Log::Error(L"%sInvalid image \"%s\": \"%s\", %dx%d, %dx%db %s, data size %dB",
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

    DXGI_FORMAT dataFormat;
#ifdef CONVERT_SRGB_INPUT_TO_LINEAR
    if (mValueType == SceneTexture::eSrgb)
        dataFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    else
#endif
        dataFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    if (!SceneUtils::CreateTextureSrvFromData(ctx,
                                              srv,
                                              image.width,
                                              image.height,
                                              dataFormat,
                                              image.image.data(),
                                              image.width * 4 * sizeof(uint8_t)))
    {
        Log::Error(L"%sFailed to create texture & SRV for image \"%s\": \"%s\", %dx%d",
                   logPrefix.c_str(),
                   Utils::StringToWstring(image.name).c_str(),
                   Utils::StringToWstring(image.uri).c_str(),
                   image.width,
                   image.height);
        return false;
    }

    mIsLoaded = true;

    // TODO: Sampler

    return true;
}


bool SceneNormalTexture::CreateNeutral(IRenderingContext &ctx)
{
    mScale = 1.f;
    return SceneTexture::CreateNeutral(ctx);
}


bool SceneNormalTexture::LoadTextureFromGltf(const tinygltf::NormalTextureInfo &normalTextureInfo,
                                             const tinygltf::Model &model,
                                             IRenderingContext &ctx,
                                             const std::wstring &logPrefix)
{
    if (!SceneTexture::LoadTextureFromGltf(normalTextureInfo.index, ctx, model, logPrefix))
        return false;

    mScale = (float)normalTextureInfo.scale;

    Log::Debug(L"%s%s: scale %f",
               logPrefix.c_str(),
               GetName().c_str(),
               mScale);

    return true;

}


bool SceneOcclusionTexture::CreateNeutral(IRenderingContext &ctx)
{
    mStrength = 1.f;
    return SceneTexture::CreateNeutral(ctx);
}


bool SceneOcclusionTexture::LoadTextureFromGltf(const tinygltf::OcclusionTextureInfo &occlusionTextureInfo,
                                                const tinygltf::Model &model,
                                                IRenderingContext &ctx,
                                                const std::wstring &logPrefix)
{
    if (!SceneTexture::LoadTextureFromGltf(occlusionTextureInfo.index, ctx, model, logPrefix))
        return false;

    mStrength = (float)occlusionTextureInfo.strength;

    Log::Debug(L"%s%s: strength %f",
               logPrefix.c_str(),
               GetName().c_str(),
               mStrength);

    return true;

}


SceneMaterial::SceneMaterial() :
    mWorkflow(MaterialWorkflow::kNone),
    mBaseColorTexture(L"BaseColorTexture", SceneTexture::eSrgb, XMFLOAT4(1.f, 1.f, 1.f, 1.f)),
    mBaseColorFactor(XMFLOAT4(1.f, 1.f, 1.f, 1.f)),
    mMetallicRoughnessTexture(L"MetallicRoughnessTexture", SceneTexture::eLinear, XMFLOAT4(1.f, 1.f, 1.f, 1.f)),
    mMetallicRoughnessFactor(XMFLOAT4(1.f, 1.f, 1.f, 1.f)),

    mSpecularTexture(L"SpecularTexture", SceneTexture::eLinear, XMFLOAT4(1.f, 1.f, 1.f, 1.f)),
    mSpecularFactor(XMFLOAT4(1.f, 1.f, 1.f, 1.f)),

    mNormalTexture(L"NormalTexture"),
    mOcclusionTexture(L"OcclusionTexture"),
    mEmissionTexture(L"EmissionTexture", SceneTexture::eSrgb, XMFLOAT4(0.f, 0.f, 0.f, 1.f)),
    mEmissionFactor(XMFLOAT4(0.f, 0.f, 0.f, 1.f))
{}


bool SceneMaterial::CreatePbrSpecularity(IRenderingContext &ctx,
                                         const wchar_t * diffuseTexPath,
                                         XMFLOAT4 diffuseFactor,
                                         const wchar_t * specularTexPath,
                                         XMFLOAT4 specularFactor)
{
    if (!mBaseColorTexture.Create(ctx, diffuseTexPath))
        return false;
    mBaseColorFactor = diffuseFactor;

    if (!mSpecularTexture.Create(ctx, specularTexPath))
        return false;
    mSpecularFactor = specularFactor;

    if (!mNormalTexture.CreateNeutral(ctx))
        return false;

    if (!mOcclusionTexture.CreateNeutral(ctx))
        return false;

    if (!mEmissionTexture.CreateNeutral(ctx))
        return false;
    mEmissionFactor = XMFLOAT4(0.f, 0.f, 0.f, 1.f);

    mWorkflow = MaterialWorkflow::kPbrSpecularity;

    return true;
}


bool SceneMaterial::CreatePbrMetalness(IRenderingContext &ctx,
                                       const wchar_t * baseColorTexPath,
                                       XMFLOAT4 baseColorFactor,
                                       const wchar_t * metallicRoughnessTexPath,
                                       float metallicFactor,
                                       float roughnessFactor)
{
    if (!mBaseColorTexture.Create(ctx, baseColorTexPath))
        return false;
    mBaseColorFactor = baseColorFactor;

    if (!mMetallicRoughnessTexture.Create(ctx, metallicRoughnessTexPath))
        return false;
    mMetallicRoughnessFactor = XMFLOAT4(0.f, roughnessFactor, metallicFactor, 0.f);

    if (!mNormalTexture.CreateNeutral(ctx))
        return false;

    if (!mOcclusionTexture.CreateNeutral(ctx))
        return false;

    if (!mEmissionTexture.CreateNeutral(ctx))
        return false;
    mEmissionFactor = XMFLOAT4(0.f, 0.f, 0.f, 1.f);

    mWorkflow = MaterialWorkflow::kPbrMetalness;

    return true;
}


bool SceneMaterial::LoadFromGltf(IRenderingContext &ctx,
                                 const tinygltf::Model &model,
                                 const tinygltf::Material &material,
                                 const std::wstring &logPrefix)
{
    auto &pbrMR = material.pbrMetallicRoughness;

    if (!mBaseColorTexture.LoadTextureFromGltf(pbrMR.baseColorTexture.index, ctx, model, logPrefix))
        return false;

    GltfUtils::FloatArrayToColor(mBaseColorFactor, pbrMR.baseColorFactor);

    Log::Debug(L"%s%s: %s",
               logPrefix.c_str(),
               L"BaseColorFactor",
               GltfUtils::ColorToWstring(mBaseColorFactor).c_str());

    if (!mMetallicRoughnessTexture.LoadTextureFromGltf(pbrMR.metallicRoughnessTexture.index, ctx, model, logPrefix))
        return false;

    GltfUtils::FloatToColorComponent<2>(mMetallicRoughnessFactor, pbrMR.metallicFactor);
    GltfUtils::FloatToColorComponent<1>(mMetallicRoughnessFactor, pbrMR.roughnessFactor);

    Log::Debug(L"%s%s: %s",
               logPrefix.c_str(),
               L"MetallicRoughnessFactor",
               GltfUtils::ColorToWstring(mBaseColorFactor).c_str());

    if (!mNormalTexture.LoadTextureFromGltf(material.normalTexture, model, ctx, logPrefix))
        return false;

    if (!mOcclusionTexture.LoadTextureFromGltf(material.occlusionTexture, model, ctx, logPrefix))
        return false;

    if (!mEmissionTexture.LoadTextureFromGltf(material.emissiveTexture.index, ctx, model, logPrefix))
        return false;

    GltfUtils::FloatArrayToColor(mEmissionFactor, material.emissiveFactor);

    Log::Debug(L"%s%s: %s",
               logPrefix.c_str(),
               L"EmissionFactor",
               GltfUtils::ColorToWstring(mEmissionFactor).c_str());

    mWorkflow = MaterialWorkflow::kPbrMetalness;

    return true;
}


void SceneMaterial::Animate(IRenderingContext &ctx)
{
    const float totalAnimPos = ctx.GetFrameAnimationTime() / 3.f/*seconds*/;

    // debug
    //auto val = cos(totalAnimPos * XM_2PI) * 0.5f + 0.5f;
    ////mNormalTexture.SetScale(val);
    ////mOcclusionTexture.SetStrength(val >= .2f ? 1.f : 0.f);
    //mEmissionFactor = XMFLOAT4(val, val, val, 1.f);
}
