#include "scene.hpp"

#include "log.hpp"
#include "utils.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include "Libs/tinygltf-2.2.0/tiny_gltf.h"

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
    XMVectorSet(0.0f, 4.0f, -10.0f, 1.0f),
    XMVectorSet(0.0f, -0.2f, 0.0f, 1.0f),
    XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f),
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

    // Pixel shader - illuminated surface
    if (!ctx.CreatePixelShader(L"../scene_shaders.fx", "PsIllumSurf", "ps_4_0", mPixelShaderIllum))
        return false;

    // Pixel shader - light-emitting surface
    if (!ctx.CreatePixelShader(L"../scene_shaders.fx", "PsEmissiveSurf", "ps_4_0", mPixelShaderSolid))
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

    return true;
}


bool Scene::Load(IRenderingContext &ctx)
{
    switch (mSceneId)
    {
    case eExternalDebugTriangleWithoutIndices:
        return LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/TriangleWithoutIndices/TriangleWithoutIndices.gltf");

    case eExternalDebugTriangle:
        return LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/Triangle/Triangle.gltf");

    case eExternalDebugSimpleMeshes:
        return LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/SimpleMeshes/SimpleMeshes.gltf");

    case eExternalDebugBox:
        return LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/Box/Box.gltf");

    case eExternalDebugBoxInterleaved:
        return LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/BoxInterleaved/BoxInterleaved.gltf");

    case eHardwiredSimpleDebugSphere:
    {
        mRootNodes.clear();
        mRootNodes.resize(1, SceneNode(true));
        if (mRootNodes.size() != 1)
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive = node0.CreateEmptyPrimitive();
        if (!primitive)
            return false;

        if (!primitive->CreateSphere(ctx, 40, 80,
                                     L"../Textures/vfx_debug_textures by Chris Judkins/debug_color_02.png"))
            return false;
        node0.AddScale({ 3.2f, 3.2f, 3.2f });

        mAmbientLight.luminance     = XMFLOAT4(0.10f, 0.10f, 0.10f, 1.0f);

        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(2.6f, 2.6f, 2.6f, 1.0f);

        // coloured point lights
        mPointLights[0].intensity   = XMFLOAT4(4.000f, 1.800f, 1.200f, 1.0f); // red
        mPointLights[1].intensity   = XMFLOAT4(1.000f, 2.500f, 1.100f, 1.0f); // green
        mPointLights[2].intensity   = XMFLOAT4(1.200f, 1.800f, 4.000f, 1.0f); // blue

        return true;
    }

    case eHardwiredEarth:
    {
        mRootNodes.clear();
        mRootNodes.resize(1, SceneNode(true));
        if (mRootNodes.size() != 1)
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive = node0.CreateEmptyPrimitive();
        if (!primitive)
            return false;

        if (!primitive->CreateSphere(ctx, 40, 80,
                                     L"../Textures/www.solarsystemscope.com/2k_earth_daymap.jpg",
                                     L"../Textures/www.solarsystemscope.com/2k_earth_specular_map.tif"))
            return false;
        node0.AddScale({ 3.2f, 3.2f, 3.2f });

        mAmbientLight.luminance     = XMFLOAT4(0.f, 0.f, 0.f, 1.0f);

        const double lum = 3.5f;
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        const double ints = 3.5f;
        mPointLights[0].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);
        mPointLights[1].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);
        mPointLights[2].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);

        return true;
    }

    case eHardwiredThreePlanets:
    {
        mRootNodes.clear();
        mRootNodes.resize(3, SceneNode(true));
        if (mRootNodes.size() != 3)
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive0 = node0.CreateEmptyPrimitive();
        if (!primitive0)
            return false;
        if (!primitive0->CreateSphere(ctx, 40, 80,
                                      L"../Textures/www.solarsystemscope.com/2k_earth_daymap.jpg",
                                      L"../Textures/www.solarsystemscope.com/2k_earth_specular_map.tif"))
            return false;
        node0.AddScale({ 2.2f, 2.2f, 2.2f });
        node0.AddTranslation({ 0.f, 0.f, -1.5f });

        auto &node1 = mRootNodes[1];
        auto primitive1 = node1.CreateEmptyPrimitive();
        if (!primitive1)
            return false;
        if (!primitive1->CreateSphere(ctx, 20, 40,
                                      L"../Textures/www.solarsystemscope.com/2k_mars.jpg"))
            return false;
        node1.AddScale({ 1.2f, 1.2f, 1.2f });
        node1.AddTranslation({ -2.5f, 0.f, 2.0f });

        auto &node2 = mRootNodes[2];
        auto primitive2 = node2.CreateEmptyPrimitive();
        if (!primitive2)
            return false;
        if (!primitive2->CreateSphere(ctx, 20, 40,
                                      L"../Textures/www.solarsystemscope.com/2k_jupiter.jpg"))
            return false;
        node2.AddScale({ 1.2f, 1.2f, 1.2f });
        node2.AddTranslation({ 2.5f, 0.f, 2.0f });

        mAmbientLight.luminance     = XMFLOAT4(0.00f, 0.00f, 0.00f, 1.0f);

        const double lum = 3.0f;
        mDirectLights[0].dir       = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);

        const double ints = 4.0f;
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


bool LoadGltfModel(tinygltf::Model &model, const std::wstring &filePath)
{
    using namespace std;

    // Convert to plain string for tinygltf
    string filePathA = Utils::WStringToString(filePath);

    // debug: tiny glTF test
    //{
    //    std::stringstream ss;
    //    cout_redirect cr(ss.rdbuf());
    //    TinyGltfTest(filePathA.c_str());
    //    Log::Debug(L"LoadGltfModel: TinyGltfTest output:\n\n%s", Utils::StringToWString(ss.str()).c_str());
    //}

    tinygltf::TinyGLTF tinyGltf;
    string errA, warnA;
    wstring ext = Utils::GetFilePathExt(filePath);

    bool ret = false;
    if (ext.compare(L"glb") == 0)
    {
        Log::Debug(L"LoadGltfModel: Reading binary glTF from \"%s\"", filePath.c_str());
        ret = tinyGltf.LoadBinaryFromFile(&model, &errA, &warnA, filePathA);
    }
    else
    {
        Log::Debug(L"LoadGltfModel: Reading ASCII glTF from \"%s\"", filePath.c_str());
        ret = tinyGltf.LoadASCIIFromFile(&model, &errA, &warnA, filePathA);
    }

    if (!errA.empty())
        Log::Debug(L"LoadGltfModel: Error: %s", Utils::StringToWString(errA).c_str());

    if (!warnA.empty())
        Log::Debug(L"LoadGltfModel: Warning: %s", Utils::StringToWString(warnA).c_str());

    if (ret)
        Log::Debug(L"LoadGltfModel: Succesfully loaded model");
    else
        Log::Error(L"LoadGltfModel: Failed to parse glTF file \"%s\"", filePath.c_str());

    return ret;
}


// debug
static std::string ModeToString(int mode)
{
    if (mode == TINYGLTF_MODE_POINTS)
        return "POINTS";
    else if (mode == TINYGLTF_MODE_LINE)
        return "LINE";
    else if (mode == TINYGLTF_MODE_LINE_LOOP)
        return "LINE_LOOP";
    else if (mode == TINYGLTF_MODE_TRIANGLES)
        return "TRIANGLES";
    else if (mode == TINYGLTF_MODE_TRIANGLE_FAN)
        return "TRIANGLE_FAN";
    else if (mode == TINYGLTF_MODE_TRIANGLE_STRIP)
        return "TRIANGLE_STRIP";
    else
        return "**UNKNOWN**";
}


static D3D11_PRIMITIVE_TOPOLOGY GltfModeToTopology(int mode)
{
    switch (mode)
    {
    case TINYGLTF_MODE_POINTS:
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    case TINYGLTF_MODE_LINE:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case TINYGLTF_MODE_LINE_STRIP:
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case TINYGLTF_MODE_TRIANGLES:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case TINYGLTF_MODE_TRIANGLE_STRIP:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    //case TINYGLTF_MODE_LINE_LOOP:
    //case TINYGLTF_MODE_TRIANGLE_FAN:
    default:
        return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}


static std::string StringIntMapToString(const std::map<std::string, int> &m)
{
    std::stringstream ss;
    bool first = true;
    for (auto item : m)
    {
        if (!first)
            ss << ", ";
        else
            first = false;
        ss << item.first << ": " << item.second;
    }
    return ss.str();
}

static std::string TypeToString(int ty) {
    if (ty == TINYGLTF_TYPE_SCALAR)
        return "SCALAR";
    else if (ty == TINYGLTF_TYPE_VECTOR)
        return "VECTOR";
    else if (ty == TINYGLTF_TYPE_VEC2)
        return "VEC2";
    else if (ty == TINYGLTF_TYPE_VEC3)
        return "VEC3";
    else if (ty == TINYGLTF_TYPE_VEC4)
        return "VEC4";
    else if (ty == TINYGLTF_TYPE_MATRIX)
        return "MATRIX";
    else if (ty == TINYGLTF_TYPE_MAT2)
        return "MAT2";
    else if (ty == TINYGLTF_TYPE_MAT3)
        return "MAT3";
    else if (ty == TINYGLTF_TYPE_MAT4)
        return "MAT4";
    return "**UNKNOWN**";
}

static std::string ComponentTypeToString(int ty) {
    if (ty == TINYGLTF_COMPONENT_TYPE_BYTE)
        return "BYTE";
    else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
        return "UNSIGNED_BYTE";
    else if (ty == TINYGLTF_COMPONENT_TYPE_SHORT)
        return "SHORT";
    else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
        return "UNSIGNED_SHORT";
    else if (ty == TINYGLTF_COMPONENT_TYPE_INT)
        return "INT";
    else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        return "UNSIGNED_INT";
    else if (ty == TINYGLTF_COMPONENT_TYPE_FLOAT)
        return "FLOAT";
    else if (ty == TINYGLTF_COMPONENT_TYPE_DOUBLE)
        return "DOUBLE";

    return "**UNKNOWN**";
}

static std::string TargetToString(int target) {
    if (target == 34962)
        return "GL_ARRAY_BUFFER";
    else if (target == 34963)
        return "GL_ELEMENT_ARRAY_BUFFER";
    else
        return "**UNKNOWN**";
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
                   Utils::StringToWString(attrName).c_str(),
                   primitiveIdx);
        accessorLoaded = false;
        return dummyAccessor;
    }

    const auto accessorIdx = attrIt->second;
    if ((accessorIdx < 0) || (accessorIdx >= model.accessors.size()))
    {
        Log::Error(L"%sInvalid %s accessor index (%d/%d)!",
                   logPrefix.c_str(),
                   Utils::StringToWString(attrName).c_str(),
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
               Utils::StringToWString(accessor.name).c_str(),
               accessor.bufferView,
               accessor.byteOffset,
               Utils::StringToWString(TypeToString(accessor.type)).c_str(),
               Utils::StringToWString(ComponentTypeToString(accessor.componentType)).c_str(),
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
    //           Utils::StringToWString(bufferView.name).c_str(),
    //           bufferView.buffer,
    //           bufferView.byteOffset,
    //           bufferView.byteLength,
    //           bufferView.byteStride,
    //           Utils::StringToWString(TargetToString(bufferView.target)).c_str());

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
    //           Utils::StringToWString(buffer.name).c_str(),
    //           buffer.data.data(),
    //           buffer.data.size(),
    //           Utils::StringToWString(buffer.uri).c_str());

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


bool Scene::LoadGLTF(IRenderingContext &ctx, const std::wstring &filePath)
{
    using namespace std;

    Log::Debug(L"");
    const std::wstring logPrefix = L"LoadGLTF: ";

    tinygltf::Model model;
    if (!LoadGltfModel(model, filePath))
        return false;

    // Scene
    if (model.scenes.size() < 1)
    {
        Log::Error(L"%sNo scenes present in the model!", logPrefix.c_str());
        return false;
    }
    if (model.scenes.size() > 1)
        Log::Warning(L"%sMore scenes present in the model. Loading just the first one.", logPrefix.c_str());
    const auto &scene = model.scenes[0];

    Log::Debug(L"");
    Log::Debug(L"%sScene 0 \"%s\": %d node(s)",
               logPrefix.c_str(),
               Utils::StringToWString(scene.name).c_str(),
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

    Log::Debug(L"");

    // debug lights
    const double amb = 0.3f;
    mAmbientLight.luminance = XMFLOAT4(amb, amb, amb, 1.0f);
    const double lum = 5.f;
    mDirectLights[0].dir       = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
    mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);
    const double ints = 6.5f;
    mPointLights[0].intensity = XMFLOAT4(ints, ints, ints, 1.0f);
    mPointLights[1].intensity = XMFLOAT4(ints, ints, ints, 1.0f);
    mPointLights[2].intensity = XMFLOAT4(ints, ints, ints, 1.0f);

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
    sceneNode.children.clear();
    sceneNode.children.reserve(node.children.size());
    const std::wstring &childLogPrefix = logPrefix + L"   ";
    for (const auto childIdx : node.children)
    {
        if ((childIdx < 0) || (childIdx >= model.nodes.size()))
        {
            Log::Error(L"%sInvalid child node index (%d/%d)!", childLogPrefix.c_str(), childIdx, model.nodes.size());
            return false;
        }

        Log::Debug(L"%sLoading child %d \"%s\"",
                   childLogPrefix.c_str(),
                   childIdx,
                   Utils::StringToWString(model.nodes[childIdx].name).c_str());

        SceneNode childNode;
        if (!LoadSceneNodeFromGLTF(ctx, childNode, model, childIdx, childLogPrefix + L"   "))
            return false;
        sceneNode.children.push_back(std::move(childNode));
    }

    return true;
}


void Scene::Destroy()
{
    Utils::ReleaseAndMakeNull(mVertexShader);
    Utils::ReleaseAndMakeNull(mPixelShaderIllum);
    Utils::ReleaseAndMakeNull(mPixelShaderSolid);
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
            ? 4.8f
            : 4.4f;
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

    // Setup pixel shader
    immCtx->PSSetShader(mPixelShaderIllum, nullptr, 0);
    immCtx->PSSetConstantBuffers(0, 1, &mCbNeverChanged);
    immCtx->PSSetConstantBuffers(2, 1, &mCbChangedEachFrame);
    immCtx->PSSetConstantBuffers(3, 1, &mCbChangedPerSceneNode);
    immCtx->PSSetSamplers(0, 1, &mSamplerLinear);

    // Scene geometry
    for (auto &node : mRootNodes)
        RenderNodeGeometry(ctx, node, XMMatrixIdentity());

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

        immCtx->PSSetShader(mPixelShaderSolid, nullptr, 0);
        mPointLightProxy.DrawGeometry(ctx, mVertexLayout);
    }
}


void Scene::RenderNodeGeometry(IRenderingContext &ctx,
                               const SceneNode &node,
                               const XMMATRIX &parentWorldMtrx)
{
    if (!ctx.IsValid())
        return;

    auto immCtx = ctx.GetImmediateContext();

    const auto worldMtrx = parentWorldMtrx * node.GetWorldMtrx();

    // Update per-node constant buffer
    CbChangedPerSceneNode cbPerSceneNode;
    cbPerSceneNode.WorldMtrx = XMMatrixTranspose(worldMtrx);
    cbPerSceneNode.MeshColor = { 0.f, 1.f, 0.f, 1.f, };
    immCtx->UpdateSubresource(mCbChangedPerSceneNode, 0, nullptr, &cbPerSceneNode, 0, 0);

    // Draw current node
    for (auto &primitive : node.primitives)
    {
        immCtx->PSSetShaderResources(0, 1, primitive.GetDiffuseSRV());
        immCtx->PSSetShaderResources(1, 1, primitive.GetSpecularSRV());
        primitive.DrawGeometry(ctx, mVertexLayout);
    }

    // Children
    for (auto &child : node.children)
        RenderNodeGeometry(ctx, child, worldMtrx);
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
    mDiffuseSRV(src.mDiffuseSRV),
    mSpecularSRV(src.mSpecularSRV)
{
    // We are creating new references of device resources
    Utils::SaveAddRef(mVertexBuffer);
    Utils::SaveAddRef(mIndexBuffer);
    Utils::SaveAddRef(mDiffuseSRV);
    Utils::SaveAddRef(mSpecularSRV);
}

ScenePrimitive::ScenePrimitive(ScenePrimitive &&src) :
    mVertices(std::move(src.mVertices)),
    mIndices(std::move(src.mIndices)),
    mTopology(Utils::Exchange(src.mTopology, D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)),
    mVertexBuffer(Utils::Exchange(src.mVertexBuffer, nullptr)),
    mIndexBuffer(Utils::Exchange(src.mIndexBuffer, nullptr)),
    mDiffuseSRV(Utils::Exchange(src.mDiffuseSRV, nullptr)),
    mSpecularSRV(Utils::Exchange(src.mSpecularSRV, nullptr))
{}

ScenePrimitive& ScenePrimitive::operator =(const ScenePrimitive &src)
{
    mVertices = src.mVertices;
    mIndices = src.mIndices;
    mTopology = src.mTopology;
    mVertexBuffer = src.mVertexBuffer;
    mIndexBuffer = src.mIndexBuffer;
    mDiffuseSRV = src.mDiffuseSRV;
    mSpecularSRV = src.mSpecularSRV;

    // We are creating new references of device resources
    Utils::SaveAddRef(mVertexBuffer);
    Utils::SaveAddRef(mIndexBuffer);
    Utils::SaveAddRef(mDiffuseSRV);
    Utils::SaveAddRef(mSpecularSRV);

    return *this;
}

ScenePrimitive& ScenePrimitive::operator =(ScenePrimitive &&src)
{
    mVertices = std::move(src.mVertices);
    mIndices = std::move(src.mIndices);
    mTopology = Utils::Exchange(src.mTopology, D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);
    mVertexBuffer = Utils::Exchange(src.mVertexBuffer, nullptr);
    mIndexBuffer = Utils::Exchange(src.mIndexBuffer, nullptr);
    mDiffuseSRV = Utils::Exchange(src.mDiffuseSRV, nullptr);
    mSpecularSRV = Utils::Exchange(src.mSpecularSRV, nullptr);

    return *this;
}

ScenePrimitive::~ScenePrimitive()
{
    Destroy();
}


bool ScenePrimitive::CreateCube(IRenderingContext & ctx,
                                const wchar_t * diffuseTexPath)
{
    if (!GenerateCubeGeometry())
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;
    if (!LoadTextures(ctx, diffuseTexPath))
        return false;

    return true;
}


bool ScenePrimitive::CreateOctahedron(IRenderingContext & ctx,
                                      const wchar_t * diffuseTexPath)
{
    if (!GenerateOctahedronGeometry())
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;
    if (!LoadTextures(ctx, diffuseTexPath))
        return false;

    return true;
}


bool ScenePrimitive::CreateSphere(IRenderingContext & ctx,
                                  const WORD vertSegmCount,
                                  const WORD stripCount,
                                  const wchar_t * diffuseTexPath,
                                  const wchar_t * specularTexPath)
{
    if (!GenerateSphereGeometry(vertSegmCount, stripCount))
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;
    if (!LoadTextures(ctx, diffuseTexPath, specularTexPath))
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
        mIndices.push_back(static_cast<WORD>(-1)); // strip restart
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
    if (!LoadGeometryFromGLTF(model, mesh, primitiveIdx, logPrefix))
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;
    if (!LoadTextures(ctx))
        return false;

    return true;
}


bool ScenePrimitive::LoadGeometryFromGLTF(const tinygltf::Model &model,
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
               Utils::StringToWString(ModeToString(primitive.mode)).c_str(),
               Utils::StringToWString(StringIntMapToString(primitive.attributes)).c_str(),
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
        //Log::Debug(L"%s%d: pos [%.1f, %.1f, %.1f]",
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

            //Log::Debug(L"%s%d: normal [%.1f, %.1f, %.1f]",
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

            Log::Debug(L"%s%d: texCoord0 [%.1f, %.1f]",
                       dataConsumerLogPrefix.c_str(), itemIdx, texCoord0.x, texCoord0.y);

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

    if ((indicesAccessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ||
        (indicesAccessor.type != TINYGLTF_TYPE_SCALAR))
    {
        Log::Error(L"%sUnsupported indices data type!", subItemsLogPrefix.c_str());
        return false;
    }

    mIndices.clear();
    mIndices.reserve(indicesAccessor.count);
    if (mIndices.capacity() < indicesAccessor.count)
    {
        Log::Error(L"%sUnable to allocate %d indices!", subItemsLogPrefix.c_str(), indicesAccessor.count);
        return false;
    }

    auto IndexDataConsumer = [this, &dataConsumerLogPrefix](int itemIdx, const unsigned char *ptr)
    {
        auto index = *reinterpret_cast<const unsigned short*>(ptr);

        itemIdx; // unused param
        //Log::Debug(L"%s%d: %d", dataConsumerLogPrefix.c_str(), itemIdx, index);

        mIndices.push_back(index);
    };

    if (!IterateGltfAccesorData<unsigned short, 1>(model,
                                                   indicesAccessor,
                                                   IndexDataConsumer,
                                                   subItemsLogPrefix.c_str(),
                                                   L"Indices"))
        return false;

    // DX primitive topology

    mTopology = GltfModeToTopology(primitive.mode);
    if (mTopology == D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
    {
        Log::Error(L"%sUnsupported primitive topology!", subItemsLogPrefix.c_str());
        return false;
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

    // Vertex buffer
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = (UINT)(sizeof(SceneVertex) * mVertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(initData));
    initData.pSysMem = mVertices.data();
    hr = device->CreateBuffer(&bd, &initData, &mVertexBuffer);
    if (FAILED(hr))
    {
        DestroyDeviceBuffers();
        return false;
    }

    // Index buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * (UINT)mIndices.size();
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


bool ScenePrimitive::LoadTextures(IRenderingContext &ctx,
                                  const wchar_t * diffuseTexPath,
                                  const wchar_t * specularTexPath)
{
    HRESULT hr = S_OK;

    auto device = ctx.GetDevice();
    if (!device)
        return false;

    if (diffuseTexPath)
    {
        hr = D3DX11CreateShaderResourceViewFromFile(device, diffuseTexPath, nullptr, nullptr, &mDiffuseSRV, nullptr);
        if (FAILED(hr))
            return false;
    }
    else
    {
        static const auto grayColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.f);
        CreateConstantTextureShaderResourceView(ctx, mDiffuseSRV, grayColor);
    }

    if (specularTexPath)
    {
        hr = D3DX11CreateShaderResourceViewFromFile(device, specularTexPath, nullptr, nullptr, &mSpecularSRV, nullptr);
        if (FAILED(hr))
            return false;
    }
    else
    {
        static const auto blackColor = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
        CreateConstantTextureShaderResourceView(ctx, mSpecularSRV, blackColor);
    }

    return true;
}


bool ScenePrimitive::CreateConstantTextureShaderResourceView(IRenderingContext &ctx,
                                                             ID3D11ShaderResourceView *&srv,
                                                             XMFLOAT4 color)
{
    HRESULT hr = S_OK;
    ID3D11Texture2D *tex = nullptr;

    auto device = ctx.GetDevice();
    if (!device)
        return false;

    // 1x1 constant-valued texture
    D3D11_TEXTURE2D_DESC descTex;
    ZeroMemory(&descTex, sizeof(D3D11_TEXTURE2D_DESC));
    descTex.ArraySize = 1;
    descTex.Usage = D3D11_USAGE_IMMUTABLE;
    descTex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    descTex.Width = 1;
    descTex.Height = 1;
    descTex.MipLevels = 1;
    descTex.SampleDesc.Count = 1;
    descTex.SampleDesc.Quality = 0;
    descTex.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initData = { &color, sizeof(XMFLOAT4), 0 };
    hr = device->CreateTexture2D(&descTex, &initData, &tex);
    if (FAILED(hr))
        return false;

    // Shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    descSRV.Format = descTex.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MipLevels = 1;
    descSRV.Texture2D.MostDetailedMip = 0;
    hr = device->CreateShaderResourceView(tex, &descSRV, &srv);
    Utils::ReleaseAndMakeNull(tex);
    if (FAILED(hr))
        return false;

    return true;
}



void ScenePrimitive::Destroy()
{
    DestroyGeomData();
    DestroyDeviceBuffers();
    DestroyTextures();
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


void ScenePrimitive::DestroyTextures()
{
    Utils::ReleaseAndMakeNull(mDiffuseSRV);
    Utils::ReleaseAndMakeNull(mSpecularSRV);
}


void ScenePrimitive::DrawGeometry(IRenderingContext &ctx, ID3D11InputLayout* vertexLayout) const
{
    auto immCtx = ctx.GetImmediateContext();

    immCtx->IASetInputLayout(vertexLayout);
    UINT stride = sizeof(SceneVertex);
    UINT offset = 0;
    immCtx->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
    immCtx->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
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
    primitives.clear();
    primitives.resize(1);
    if (primitives.size() != 1)
        return nullptr;

    return &primitives[0];
}

void SceneNode::SetIdentity()
{
    mLocalMtrx = XMMatrixIdentity();
}

void SceneNode::AddScale(const std::vector<double> &vec)
{
    if (vec.size() != 3)
        return;

    const auto scaleMtrx = XMMatrixScaling((float)vec[0], (float)vec[1], (float)vec[2]);
    mLocalMtrx = mLocalMtrx * scaleMtrx;
}

void SceneNode::AddRotationQuaternion(const std::vector<double> &vec)
{
    if (vec.size() != 4)
        return;

    const XMFLOAT4 quaternion((float)vec[0], (float)vec[1], (float)vec[2], (float)vec[3]);
    const auto rotMtrx = XMMatrixRotationQuaternion(XMLoadFloat4(&quaternion));
    mLocalMtrx = mLocalMtrx * rotMtrx;
}

void SceneNode::AddTranslation(const std::vector<double> &vec)
{
    if (vec.size() != 3)
        return;

    const auto translMtrx = XMMatrixTranslation((float)vec[0], (float)vec[1], (float)vec[2]);
    mLocalMtrx = mLocalMtrx * translMtrx;
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
                   Utils::StringToWString(node.name).c_str(),
                   node.mesh,
                   transforms.c_str(),
                   node.children.size());
    }

    const std::wstring &subItemsLogPrefix = logPrefix + L"   ";

    // Local transformation
    if (node.matrix.size() == 4)
    {
        // TODO

        Log::Error(L"%sLocal transformation given by matrix is not yet supported!", subItemsLogPrefix.c_str());
        return false;
    }
    else
    {
        SetIdentity();
        if (mIsRootNode)
            AddScale({ 3., 3., 3. }); // debug
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
                   Utils::StringToWString(mesh.name).c_str(),
                   mesh.primitives.size());

        // Primitives
        const auto primitivesCount = mesh.primitives.size();
        primitives.reserve(primitivesCount);
        for (size_t i = 0; i < primitivesCount; ++i)
        {
            primitives.push_back(ScenePrimitive());
            if (!primitives.back().LoadFromGLTF(ctx, model, mesh, (int)i, subItemsLogPrefix + L"   "))
            {
                primitives.pop_back();
                return false;
            }
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

        mWorldMtrx = rotMtrx * mLocalMtrx;
    }
    else
        mWorldMtrx = mLocalMtrx;

    for (auto &child : children)
        child.Animate(ctx);
}
