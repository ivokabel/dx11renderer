#include "scene.hpp"

#include "log.hpp"
#include "utils.hpp"
#include "constants.hpp"

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


struct SceneVertex
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
    XMFLOAT2 Tex;
};


class SceneObject
{
public:

    SceneObject();
    ~SceneObject();

    bool CreateCube(IRenderingContext & ctx,
                    const XMFLOAT4 pos = XMFLOAT4(0, 0, 0, 1),
                    const float scale = 1.f,
                    const wchar_t * diffuseTexPath = nullptr);
    bool CreateOctahedron(IRenderingContext & ctx,
                          const XMFLOAT4 pos = XMFLOAT4(0, 0, 0, 1),
                          const float scale = 1.f,
                          const wchar_t * diffuseTexPath = nullptr);
    bool CreateSphere(IRenderingContext & ctx,
                      const WORD vertSegmCount = 40,
                      const WORD stripCount = 80,
                      const XMFLOAT4 pos = XMFLOAT4(0, 0, 0, 1),
                      const float scale = 1.f,
                      const wchar_t * diffuseTexPath = nullptr,
                      const wchar_t * specularTexPath = nullptr);

    void Animate(IRenderingContext &ctx);
    void DrawGeometry(IRenderingContext &ctx, ID3D11InputLayout* vertexLayout);

    ID3D11ShaderResourceView* const* GetDiffuseSRV()  const { return &mDiffuseSRV; };
    ID3D11ShaderResourceView* const* GetSpecularSRV() const { return &mSpecularSRV; };

    XMMATRIX GetWorldMtrx() const { return mWorldMtrx; }

    void Destroy();

private:

    bool GenerateCubeGeometry();
    bool GenerateOctahedronGeometry();
    bool GenerateSphereGeometry(const WORD vertSegmCount, const WORD stripCount);

    bool CreateDeviceBuffers(IRenderingContext &ctx);
    bool LoadTextures(IRenderingContext &ctx,
                      const wchar_t * diffuseTexPath,
                      const wchar_t * specularTexPath = nullptr);

    void DestroyGeomData();
    void DestroyDeviceBuffers();
    void DestroyTextures();

private:

    // Geometry data
    std::vector<SceneVertex>    mVertices;
    std::vector<WORD>           mIndices;
    D3D11_PRIMITIVE_TOPOLOGY    mTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

    // Device geometry data
    ID3D11Buffer*               mVertexBuffer = nullptr;
    ID3D11Buffer*               mIndexBuffer = nullptr;

    // Animation
    float                       mScale = 1.f;
    XMFLOAT4                    mPos = XMFLOAT4(0, 0, 0, 1);
    XMMATRIX                    mWorldMtrx;

    // Textures
    ID3D11ShaderResourceView*   mDiffuseSRV = nullptr;
    ID3D11Texture2D*            mSpecularTex = nullptr;
    ID3D11ShaderResourceView*   mSpecularSRV = nullptr;
};

std::vector<SceneObject> sSceneObjects;
SceneObject sPointLightProxy;


struct {
    XMVECTOR eye;
    XMVECTOR at;
    XMVECTOR up;
} sViewData = {
    XMVectorSet(0.0f, 4.0f, -10.0f, 1.0f),
    XMVectorSet(0.0f, -0.2f, 0.0f, 1.0f),
    XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f),
};


struct AmbientLight
{
    XMFLOAT4 luminance; // omnidirectional luminance: lm * sr-1 * m-2

    AmbientLight() :
        luminance{}
    {}
};


// Directional light
struct DirectLight
{
    XMFLOAT4 dir;
    XMFLOAT4 dirTransf;
    XMFLOAT4 luminance; // lm * sr-1 * m-2

    DirectLight() :
        dir{},
        dirTransf{},
        luminance{}
    {}
};


struct PointLight
{
    XMFLOAT4 pos;
    XMFLOAT4 posTransf;
    XMFLOAT4 intensity; // luminuous intensity [cd = lm * sr-1] = luminuous flux / 4Pi

    PointLight() :
        pos{},
        posTransf{},
        intensity{}
    {}
};


AmbientLight sAmbientLight;
std::array<DirectLight, DIRECT_LIGHTS_COUNT> sDirectLights;
std::array<PointLight, POINT_LIGHTS_COUNT> sPointLights;


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

struct CbChangedPerObject
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
    bd.ByteWidth = sizeof(CbChangedPerObject);
    hr = device->CreateBuffer(&bd, nullptr, &mCbChangedPerObject);
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

    if (!sPointLightProxy.CreateSphere(ctx, 8, 16))
        return false;

    return true;
}


bool Scene::Load(IRenderingContext &ctx)
{
    switch (mSceneId)
    {
    case eExternal:
    {
        return LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/Triangle/Triangle.gltf");
        //return LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/TriangleWithoutIndices/TriangleWithoutIndices.gltf");
    }

    case eHardwiredSimpleDebugSphere:
    {
        sSceneObjects.resize(1);
        if (sSceneObjects.size() != 1)
            return false;

        if (!sSceneObjects[0].CreateSphere(ctx,
                                           40, 80,
                                           XMFLOAT4(0.f, 0.f, 0.f, 1.f),
                                           3.2f,
                                           L"../Textures/vfx_debug_textures by Chris Judkins/debug_color_02.png"))
            return false;

        sAmbientLight.luminance     = XMFLOAT4(0.10f, 0.10f, 0.10f, 1.0f);

        sDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        sDirectLights[0].luminance  = XMFLOAT4(2.6f, 2.6f, 2.6f, 1.0f);

        // coloured point lights
        sPointLights[0].intensity   = XMFLOAT4(4.000f, 1.800f, 1.200f, 1.0f); // red
        sPointLights[1].intensity   = XMFLOAT4(1.000f, 2.500f, 1.100f, 1.0f); // green
        sPointLights[2].intensity   = XMFLOAT4(1.200f, 1.800f, 4.000f, 1.0f); // blue

        return true;
    }

    case eHardwiredEarth:
    {
        sSceneObjects.resize(1);
        if (sSceneObjects.size() != 1)
            return false;

        if (!sSceneObjects[0].CreateSphere(ctx,
                                           40, 80,
                                           XMFLOAT4(0.f, 0.f, 0.f, 1.f),
                                           3.2f,
                                           L"../Textures/www.solarsystemscope.com/2k_earth_daymap.jpg",
                                           L"../Textures/www.solarsystemscope.com/2k_earth_specular_map.tif"))
            return false;

        sAmbientLight.luminance     = XMFLOAT4(0.f, 0.f, 0.f, 1.0f);

        const double lum = 3.5f;
        sDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        sDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        const double ints = 3.5f;
        sPointLights[0].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);
        sPointLights[1].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);
        sPointLights[2].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);

        return true;
    }

    case eHardwiredThreePlanets:
    {
        sSceneObjects.resize(3);
        if (sSceneObjects.size() != 3)
            return false;

        if (!sSceneObjects[0].CreateSphere(ctx,
                                           40, 80,
                                           XMFLOAT4(0.f, 0.f, -1.5f, 1.f),
                                           2.2f,
                                           L"../Textures/www.solarsystemscope.com/2k_earth_daymap.jpg",
                                           L"../Textures/www.solarsystemscope.com/2k_earth_specular_map.tif"))
            return false;

        if (!sSceneObjects[1].CreateSphere(ctx,
                                           20, 40,
                                           XMFLOAT4(-2.5f, 0.f, 2.0f, 1.f),
                                           1.2f,
                                           L"../Textures/www.solarsystemscope.com/2k_mars.jpg"))
            return false;

        if (!sSceneObjects[2].CreateSphere(ctx,
                                           20, 40,
                                           XMFLOAT4(2.5f, 0.f, 2.0f, 1.f),
                                           1.2f,
                                           L"../Textures/www.solarsystemscope.com/2k_jupiter.jpg"))
            return false;

        sAmbientLight.luminance     = XMFLOAT4(0.00f, 0.00f, 0.00f, 1.0f);

        const double lum = 3.0f;
        sDirectLights[0].dir = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        sDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);

        const double ints = 4.0f;
        sPointLights[0].intensity = XMFLOAT4(ints, ints, ints, 1.0f);
        sPointLights[1].intensity = XMFLOAT4(ints, ints, ints, 1.0f);
        sPointLights[2].intensity = XMFLOAT4(ints, ints, ints, 1.0f);

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
    string filePathA = Utils::WideStringToString(filePath);

    // debug: tiny glTF test
    //{
    //    std::stringstream ss;
    //    cout_redirect cr(ss.rdbuf());
    //    TinyGltfTest(filePathA.c_str());
    //    Log::Debug(L"LoadGltfModel: TinyGltfTest output:\n\n%s", Utils::StringToWideString(ss.str()).c_str());
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
        Log::Debug(L"LoadGltfModel: Error: %s", Utils::StringToWideString(errA).c_str());

    if (!warnA.empty())
        Log::Debug(L"LoadGltfModel: Warning: %s", Utils::StringToWideString(warnA).c_str());

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


template <typename ComponentType,
          size_t ComponentCount>
bool IterateGltfAccesorData(const tinygltf::Model &model,
                            const int accessorIdx,
                            const wchar_t *logPrefix = L"",
                            const wchar_t *logDataName = L"")
{
    if ((accessorIdx < 0) || (accessorIdx > model.accessors.size()))
    {
        Log::Error(L"%sInvalid %s accessor index (%d/%d)!",
                   logPrefix, logDataName, accessorIdx, model.accessors.size());
        return false;
    }

    const auto &accessor = model.accessors[accessorIdx];

    Log::Debug(L"%s%s accesor %d \"%s\": view %d, offset %d, type %s<%s>, count %d",
               logPrefix,
               logDataName,
               accessorIdx,
               Utils::StringToWideString(accessor.name).c_str(),
               accessor.bufferView,
               accessor.byteOffset,
               Utils::StringToWideString(TypeToString(accessor.type)).c_str(),
               Utils::StringToWideString(ComponentTypeToString(accessor.componentType)).c_str(),
               accessor.count);

    // Buffer view

    const auto bufferViewIdx = accessor.bufferView;

    if ((bufferViewIdx < 0) || (bufferViewIdx > model.bufferViews.size()))
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
    //           Utils::StringToWideString(bufferView.name).c_str(),
    //           bufferView.buffer,
    //           bufferView.byteOffset,
    //           bufferView.byteLength,
    //           bufferView.byteStride,
    //           Utils::StringToWideString(TargetToString(bufferView.target)).c_str());

    if (bufferView.byteStride != 0)
    {
        Log::Error(L"%sUnsupported byte stride (%d) for %s buffer!",
                   logPrefix, bufferView.byteStride, logDataName);
        return false;
    }

    // Buffer

    const auto bufferIdx = bufferView.buffer;

    if ((bufferIdx < 0) || (bufferIdx > model.buffers.size()))
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
    //           Utils::StringToWideString(buffer.name).c_str(),
    //           buffer.data.data(),
    //           buffer.data.size(),
    //           Utils::StringToWideString(buffer.uri).c_str());

    // TODO: Check that buffer view is large enough to contain all data from accessor?

    // Data

    if ((accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) &&
        (accessor.type == TINYGLTF_TYPE_VEC3))
    {
        const auto componentSize = sizeof(ComponentType);
        const auto typeSize = ComponentCount * componentSize;
        const auto stride = bufferView.byteStride;
        const auto typeOffset = (stride == 0) ? typeSize : stride;

        auto ptr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        int idx = 0;
        for (; idx < accessor.count; ++idx, ptr += typeOffset)
        {
            const XMFLOAT3 pos = *(XMFLOAT3*)ptr;

            Log::Debug(L"%s %d: pos [%.1f, %.1f, %.1f]",
                       logPrefix,
                       idx,
                       pos.x, pos.y, pos.z);
        }
        return true;
    }
    else
    {
        Log::Error(L"%sUnsupported %s data type!", logPrefix, logDataName);
        return false;
    }
}


bool Scene::LoadGLTF(IRenderingContext &ctx, const std::wstring &filePath)
{
    using namespace std;

    Log::Debug(L"");

    tinygltf::Model model;
    if (!LoadGltfModel(model, filePath))
        return false;

    // Scene
    if (model.scenes.size() < 1)
    {
        Log::Error(L"LoadGLTF: No scenes present in the model!");
        return false;
    }
    if (model.scenes.size() > 1)
        Log::Warning(L"LoadGLTF: More scenes present in the model. Loading just the first one.");
    const auto &scene = model.scenes[0];

    Log::Debug(L"");
    Log::Debug(L"LoadGLTF: Scene 0 \"%s\": %d node(s)",
               Utils::StringToWideString(scene.name).c_str(),
               scene.nodes.size());

    // Nodes
    // No children so far
    for (const auto nodeIdx : scene.nodes)
    {
        if (nodeIdx >= model.nodes.size())
        {
            Log::Error(L"LoadGLTF:  Invalid node index (%d/%d)!", nodeIdx, model.nodes.size());
            return false;
        }

        const auto &node = model.nodes[nodeIdx];

        Log::Debug(L"LoadGLTF:  Node %d/%d \"%s\": mesh %d, %d children",
                   nodeIdx,
                   model.nodes.size(),
                   Utils::StringToWideString(node.name).c_str(),
                   node.mesh,
                   node.children.size());

        // Children
        for (const auto childIdx : node.children)
        {
            if ((childIdx < 0) || (childIdx >= model.nodes.size()))
            {
                Log::Error(L"LoadGLTF:   Invalid child node index (%d/%d)!", childIdx, model.nodes.size());
                return false;
            }

            Log::Debug(L"LoadGLTF:   Child %d/%d \"%s\"",
                       childIdx,
                       model.nodes.size(),
                       Utils::StringToWideString(model.nodes[childIdx].name).c_str());
        }

        // Mesh
        const auto meshIdx = node.mesh;
        if (meshIdx >= model.meshes.size())
        {
            Log::Error(L"LoadGLTF:   Invalid mesh index (%d/%d)!", meshIdx, model.meshes.size());
            return false;
        }

        const auto &mesh = model.meshes[meshIdx];

        Log::Debug(L"LoadGLTF:   Mesh %d/%d \"%s\": %d primitive(s)",
                   meshIdx,
                   model.meshes.size(),
                   Utils::StringToWideString(mesh.name).c_str(),
                   mesh.primitives.size());

        // Primitives
        for (size_t i = 0; i < mesh.primitives.size(); ++i)
        {
            const auto &primitive = mesh.primitives[i];

            Log::Debug(L"LoadGLTF:    Primitive %d/%d: mode %s, attributes [%s], indices %d, material %d",
                       i,
                       mesh.primitives.size(),
                       Utils::StringToWideString(ModeToString(primitive.mode)).c_str(),
                       Utils::StringToWideString(StringIntMapToString(primitive.attributes)).c_str(),
                       primitive.indices,
                       primitive.material);

            const auto &attrs = primitive.attributes;

            // POSITION

            const auto positionIt = attrs.find("POSITION");
            if (positionIt == attrs.end())
            {
                Log::Error(L"LoadGLTF:     No POSITION attribute present in primitive %d!", i);
                return false;
            }

            if (!IterateGltfAccesorData<float, 3>(model,
                                                  positionIt->second,
                                                  L"LoadGLTF:     ",
                                                  L"POSITION"))
                return false;


            // Indices accessor

            const auto indicesIdx = primitive.indices;

            if (indicesIdx < 0)
            {
                Log::Error(L"LoadGLTF:     Non-indexed geometry is not supported!");
                return false;
            }

            if (indicesIdx > model.accessors.size())
            {
                Log::Error(L"LoadGLTF:     Invalid indices accessor index (%d/%d)!", indicesIdx, model.accessors.size());
                return false;
            }

            const auto &indicesAccessor = model.accessors[indicesIdx];

            Log::Debug(L"LoadGLTF:     Indices accesor %d \"%s\": view %d, offset %d, type %s<%s>, count %d",
                       indicesIdx,
                       Utils::StringToWideString(indicesAccessor.name).c_str(),
                       indicesAccessor.bufferView,
                       indicesAccessor.byteOffset,
                       Utils::StringToWideString(TypeToString(indicesAccessor.type)).c_str(),
                       Utils::StringToWideString(ComponentTypeToString(indicesAccessor.componentType)).c_str(),
                       indicesAccessor.count);

            // Indices buffer view

            const auto indicesViewIdx = indicesAccessor.bufferView;

            if ((indicesViewIdx < 0) || (indicesViewIdx > model.bufferViews.size()))
            {
                Log::Error(L"LoadGLTF:     Invalid indices view buffer index (%d/%d)!", indicesViewIdx, model.bufferViews.size());
                return false;
            }

            const auto &indicesView = model.bufferViews[indicesViewIdx];

            //Log::Debug(L"LoadGLTF:     Indices buffer view %d \"%s\": buffer %d, offset %d, length %d, stride %d, target %s",
            //           indicesViewIdx,
            //           Utils::StringToWideString(indicesView.name).c_str(),
            //           indicesView.buffer,
            //           indicesView.byteOffset,
            //           indicesView.byteLength,
            //           indicesView.byteStride,
            //           Utils::StringToWideString(TargetToString(indicesView.target)).c_str());

            if (indicesView.byteStride != 0)
            {
                Log::Error(L"LoadGLTF:     Unsupported byte stride (%d) for indices buffer!", indicesView.byteStride);
                return false;
            }

            // Indices buffer

            const auto indicesBufferIdx = indicesView.buffer;

            if ((indicesBufferIdx < 0) || (indicesBufferIdx > model.buffers.size()))
            {
                Log::Error(L"LoadGLTF:     Invalid indices buffer index (%d/%d)!", indicesBufferIdx, model.buffers.size());
                return false;
            }

            const auto &indicesBuffer = model.buffers[indicesBufferIdx];

            const auto indicesByteEnd = indicesView.byteOffset + indicesView.byteLength;
            if (indicesByteEnd > indicesBuffer.data.size())
            {
                Log::Error(L"LoadGLTF:     Accessing data chunk outside indices buffer %d!", indicesBufferIdx);
                return false;
            }

            //Log::Debug(L"LoadGLTF:     Indices buffer %d \"%s\": data %x, size %d, uri \"%s\"",
            //           indicesBufferIdx,
            //           Utils::StringToWideString(indicesBuffer.name).c_str(),
            //           indicesBuffer.data.data(),
            //           indicesBuffer.data.size(),
            //           Utils::StringToWideString(indicesBuffer.uri).c_str());

            // TODO: Check that buffer view is large enough to contain all data from accessor?

            auto const indicesViewData  = indicesBuffer.data.data() + indicesView.byteOffset;
            auto const firstIndexPtr    = indicesViewData + indicesAccessor.byteOffset;

            // Indices

            if ((indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) &&
                (indicesAccessor.type          == TINYGLTF_TYPE_SCALAR))
            {
                typedef unsigned short ComponentType;
                const auto componentSize    = sizeof(ComponentType);
                const auto typeSize         = 1 * componentSize;
                const auto stride           = indicesView.byteStride;
                const auto typeOffset       = (stride == 0) ? typeSize : stride;

                auto indexPtr = firstIndexPtr;
                int idxIdx = 0;
                for (; idxIdx < indicesAccessor.count; ++idxIdx, indexPtr += typeOffset)
                {
                    const auto index = *(ComponentType*)indexPtr;

                    Log::Debug(L"LoadGLTF:      %d: %d", idxIdx, index);
                }
            }
            else
            {
                Log::Error(L"LoadGLTF:    Unsupported indices data type!");
                return false;
            }
        }
    }

    // TODO
    ctx;

    Log::Debug(L"");

    return false;
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
    Utils::ReleaseAndMakeNull(mCbChangedPerObject);
    Utils::ReleaseAndMakeNull(mSamplerLinear);

    sSceneObjects.clear();

    sPointLightProxy.Destroy();
}


void Scene::Animate(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    for (auto &object : sSceneObjects)
        object.Animate(ctx);

    // Directional lights are steady (for now)
    for (auto &dirLight : sDirectLights)
        dirLight.dirTransf = dirLight.dir;

    // Animate point lights

    const float time = ctx.GetCurrentAnimationTime();
    const float period = 15.f; //seconds
    const float totalAnimPos = time / period;
    const float angle = totalAnimPos * XM_2PI;

    const auto pointCount = sPointLights.size();
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

        const XMVECTOR lightVec = XMLoadFloat4(&sPointLights[i].pos);
        const XMVECTOR lightVecTransf = XMVector3Transform(lightVec, transfMtrx);
        XMStoreFloat4(&sPointLights[i].posTransf, lightVecTransf);
    }
}


void Scene::Render(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    auto immCtx = ctx.GetImmediateContext();

    // Frame constant buffer
    CbChangedEachFrame cbEachFrame;
    cbEachFrame.AmbientLightLuminance = sAmbientLight.luminance;
    for (int i = 0; i < sDirectLights.size(); i++)
    {
        cbEachFrame.DirectLightDirs[i]       = sDirectLights[i].dirTransf;
        cbEachFrame.DirectLightLuminances[i] = sDirectLights[i].luminance;
    }
    for (int i = 0; i < sPointLights.size(); i++)
    {
        cbEachFrame.PointLightPositions[i]   = sPointLights[i].posTransf;
        cbEachFrame.PointLightIntensities[i] = sPointLights[i].intensity;
    }
    immCtx->UpdateSubresource(mCbChangedEachFrame, 0, nullptr, &cbEachFrame, 0, 0);

    // Setup vertex shader
    immCtx->VSSetShader(mVertexShader, nullptr, 0);
    immCtx->VSSetConstantBuffers(0, 1, &mCbNeverChanged);
    immCtx->VSSetConstantBuffers(1, 1, &mCbChangedOnResize);
    immCtx->VSSetConstantBuffers(2, 1, &mCbChangedEachFrame);
    immCtx->VSSetConstantBuffers(3, 1, &mCbChangedPerObject);

    // Setup pixel shader
    immCtx->PSSetShader(mPixelShaderIllum, nullptr, 0);
    immCtx->PSSetConstantBuffers(0, 1, &mCbNeverChanged);
    immCtx->PSSetConstantBuffers(2, 1, &mCbChangedEachFrame);
    immCtx->PSSetConstantBuffers(3, 1, &mCbChangedPerObject);
    immCtx->PSSetSamplers(0, 1, &mSamplerLinear);

    // Draw all scene objects
    for (auto &object : sSceneObjects)
    {
        // Per-object constant buffer
        CbChangedPerObject cbPerObject;
        cbPerObject.WorldMtrx = XMMatrixTranspose(object.GetWorldMtrx());
        cbPerObject.MeshColor = { 0.f, 1.f, 0.f, 1.f, };
        immCtx->UpdateSubresource(mCbChangedPerObject, 0, nullptr, &cbPerObject, 0, 0);

        // Draw
        immCtx->PSSetShaderResources(0, 1, object.GetDiffuseSRV());
        immCtx->PSSetShaderResources(1, 1, object.GetSpecularSRV());
        object.DrawGeometry(ctx, mVertexLayout);
    }

    // Proxy geometry for point lights
    for (int i = 0; i < sPointLights.size(); i++)
    {
        CbChangedPerObject cbPerObject;

        const float radius = 0.07f;
        XMMATRIX lightScaleMtrx = XMMatrixScaling(radius, radius, radius);
        XMMATRIX lightTrnslMtrx = XMMatrixTranslationFromVector(XMLoadFloat4(&sPointLights[i].posTransf));
        XMMATRIX lightMtrx = lightScaleMtrx * lightTrnslMtrx;
        cbPerObject.WorldMtrx = XMMatrixTranspose(lightMtrx);

        const float radius2 = radius * radius;
        cbPerObject.MeshColor = {
            sPointLights[i].intensity.x / radius2,
            sPointLights[i].intensity.y / radius2,
            sPointLights[i].intensity.z / radius2,
            sPointLights[i].intensity.w / radius2,
        };

        immCtx->UpdateSubresource(mCbChangedPerObject, 0, nullptr, &cbPerObject, 0, 0);

        immCtx->PSSetShader(mPixelShaderSolid, nullptr, 0);
        sPointLightProxy.DrawGeometry(ctx, mVertexLayout);
    }
}

bool Scene::GetAmbientColor(float(&rgba)[4])
{
    rgba[0] = sAmbientLight.luminance.x;
    rgba[1] = sAmbientLight.luminance.y;
    rgba[2] = sAmbientLight.luminance.z;
    rgba[3] = sAmbientLight.luminance.w;
    return true;
}


SceneObject::SceneObject() :
    mWorldMtrx(XMMatrixIdentity())
{}

SceneObject::~SceneObject()
{
    Destroy();
}


bool SceneObject::CreateCube(IRenderingContext & ctx,
                             const XMFLOAT4 pos,
                             const float scale,
                             const wchar_t * diffuseTexPath)
{
    mScale = scale;
    mPos = pos;

    if (!GenerateCubeGeometry())
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;
    if (!LoadTextures(ctx, diffuseTexPath))
        return false;

    return true;
}


bool SceneObject::CreateOctahedron(IRenderingContext & ctx,
                                   const XMFLOAT4 pos,
                                   const float scale,
                                   const wchar_t * diffuseTexPath)
{
    mScale = scale;
    mPos = pos;

    if (!GenerateOctahedronGeometry())
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;
    if (!LoadTextures(ctx, diffuseTexPath))
        return false;

    return true;
}


bool SceneObject::CreateSphere(IRenderingContext & ctx,
                               const WORD vertSegmCount,
                               const WORD stripCount,
                               const XMFLOAT4 pos,
                               const float scale,
                               const wchar_t * diffuseTexPath,
                               const wchar_t * specularTexPath)
{
    mScale = scale;
    mPos = pos;

    if (!GenerateSphereGeometry(vertSegmCount, stripCount))
        return false;
    if (!CreateDeviceBuffers(ctx))
        return false;
    if (!LoadTextures(ctx, diffuseTexPath, specularTexPath))
        return false;

    return true;
}


bool SceneObject::GenerateCubeGeometry()
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


bool SceneObject::GenerateOctahedronGeometry()
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


bool SceneObject::GenerateSphereGeometry(const WORD vertSegmCount, const WORD stripCount)
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

    Log::Debug(L"SceneObject::GenerateSphereGeometry: "
               L"%d segments, %d strips => %d triangles, %d vertices, %d indices",
               vertSegmCount, stripCount,
               stripCount * (2 * horzLineCount),
               vertexCount, indexCount);

    return true;
}


bool SceneObject::CreateDeviceBuffers(IRenderingContext & ctx)
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


bool SceneObject::LoadTextures(IRenderingContext &ctx,
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

    if (specularTexPath)
    {
        hr = D3DX11CreateShaderResourceViewFromFile(device, specularTexPath, nullptr, nullptr, &mSpecularSRV, nullptr);
        if (FAILED(hr))
            return false;
    }
    else
    {
        // Default 1x1 zero-value texture
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
        static const XMFLOAT4 blackColor = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
        D3D11_SUBRESOURCE_DATA initData = { &blackColor, sizeof(XMFLOAT4), 0 };
        hr = device->CreateTexture2D(&descTex, &initData, &mSpecularTex);
        if (FAILED(hr))
            return false;

        // Shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
        descSRV.Format = descTex.Format;
        descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        descSRV.Texture2D.MipLevels = 1;
        descSRV.Texture2D.MostDetailedMip = 0;
        hr = device->CreateShaderResourceView(mSpecularTex, &descSRV, &mSpecularSRV);
        if (FAILED(hr))
            return false;
    }

    return true;
}


void SceneObject::Destroy()
{
    DestroyGeomData();
    DestroyDeviceBuffers();
    DestroyTextures();
}


void SceneObject::DestroyGeomData()
{
    mVertices.clear();
    mIndices.clear();
    mTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}


void SceneObject::DestroyDeviceBuffers()
{
    Utils::ReleaseAndMakeNull(mVertexBuffer);
    Utils::ReleaseAndMakeNull(mIndexBuffer);
}


void SceneObject::DestroyTextures()
{
    Utils::ReleaseAndMakeNull(mDiffuseSRV);
    Utils::ReleaseAndMakeNull(mSpecularTex);
    Utils::ReleaseAndMakeNull(mSpecularSRV);
}


void SceneObject::Animate(IRenderingContext &ctx)
{
    const float time = ctx.GetCurrentAnimationTime();
    const float period = 15.f; //seconds
    const float totalAnimPos = time / period;
    const float angle = totalAnimPos * XM_2PI;

    XMMATRIX shiftMtrx = XMMatrixTranslationFromVector(XMLoadFloat4(&mPos));
    XMMATRIX scaleMtrx = XMMatrixScaling(mScale, mScale, mScale);
    XMMATRIX rotMtrx = XMMatrixRotationY(angle);
    mWorldMtrx = scaleMtrx * rotMtrx * shiftMtrx;
}


void SceneObject::DrawGeometry(IRenderingContext &ctx, ID3D11InputLayout* vertexLayout)
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
