#include "scene.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <cassert>
#include <array>
#include <vector>

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


class SceneGeometry
{
public:

    ~SceneGeometry();

    bool GenerateCube();
    bool GenerateOctahedron();
    bool GenerateSphere();

    bool CreateDeviceBuffers(IRenderingContext &ctx);

    void Destroy();

private:

    void DestroyGeomData();
    void DestroyDeviceBuffers();

public:

    // Local data
    std::vector<SceneVertex>    vertices;
    std::vector<WORD>           indices;
    D3D11_PRIMITIVE_TOPOLOGY    topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

    // Device data
    ID3D11Buffer*               mVertexBuffer = nullptr;
    ID3D11Buffer*               mIndexBuffer = nullptr;
} sGeometry;


struct {
    XMVECTOR eye;
    XMVECTOR at;
    XMVECTOR up;
} sViewData = {
    XMVectorSet(0.0f, 4.0f, -10.0f, 0.0f),
    XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f),
    XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
};


struct AmbientLight
{
    const XMFLOAT4 color;

    AmbientLight() = delete;
    AmbientLight& operator =(const AmbientLight &a) = delete;
};


// Directional light
struct DirectLight
{
    const XMFLOAT4 dir;
          XMFLOAT4 dirTransf;
    const XMFLOAT4 color;

    DirectLight() = delete;
    DirectLight& operator =(const DirectLight &a) = delete;
};


struct PointLight
{
    const XMFLOAT4 pos;
          XMFLOAT4 posTransf;
    const XMFLOAT4 intensity; // luminuous intensity [cd = lm * sr-1] = luminuous flux / 4Pi

    PointLight() = delete;
    PointLight& operator =(const PointLight &a) = delete;
};


#define DIRECT_LIGHTS_COUNT 1
#define POINT_LIGHTS_COUNT  2

AmbientLight sAmbientLight{ XMFLOAT4(0.04f, 0.08f, 0.14f, 1.0f) };
//AmbientLight sAmbientLight{ XMFLOAT4(0.14f, 0.18f, 0.24f, 1.0f) };

std::array<DirectLight, DIRECT_LIGHTS_COUNT> sDirectLights =
{
    DirectLight{ XMFLOAT4(-0.577f, 0.577f,-0.577f, 1.0f), XMFLOAT4(0, 0, 0, 0), XMFLOAT4(0.30f, 0.30f, 0.30f, 1.0f) },
};

std::array<PointLight, POINT_LIGHTS_COUNT> sPointLights =
{
    PointLight{ XMFLOAT4(0.0f, -1.0f, -3.5f, 1.0f), XMFLOAT4(0, 0, 0, 0), XMFLOAT4(0.15f, 0.30f, 0.15f, 1.0f)/*cd = lm * sr-1]*/ },
    PointLight{ XMFLOAT4(0.0f, -1.0f,  3.5f, 1.0f), XMFLOAT4(0, 0, 0, 0), XMFLOAT4(0.30f, 0.15f, 0.15f, 1.0f)/*cd = lm * sr-1]*/ },
};


struct CbNeverChanged
{
    XMMATRIX View;
};

struct CbChangedOnResize
{
    XMMATRIX Projection;
};

struct CbChangedEachFrame
{
    // Transformations
    XMMATRIX World;
    XMFLOAT4 MeshColor;

    // Light sources
    XMFLOAT4 AmbientLight;
    XMFLOAT4 DirectLightDirs[DIRECT_LIGHTS_COUNT];
    XMFLOAT4 DirectLightColors[DIRECT_LIGHTS_COUNT];
    XMFLOAT4 PointLightDirs[POINT_LIGHTS_COUNT];
    XMFLOAT4 PointLightIntensities[POINT_LIGHTS_COUNT];
};


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
    ID3DBlob* pVSBlob = nullptr;
    if (!ctx.CreateVertexShader(L"../shaders.fx", "VS", "vs_4_0", pVSBlob, mVertexShader))
        return false;

    // Input layout
    hr = device->CreateInputLayout(sVertexLayout.data(),
                                   (UINT)sVertexLayout.size(),
                                   pVSBlob->GetBufferPointer(),
                                   pVSBlob->GetBufferSize(),
                                   &mVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr))
        return false;
    immCtx->IASetInputLayout(mVertexLayout);

    // Pixel shader - illuminated surface
    if (!ctx.CreatePixelShader(L"../shaders.fx", "PsIllumSurf", "ps_4_0", mPixelShaderIllum))
        return false;

    // Pixel shader - light-emitting surface
    if (!ctx.CreatePixelShader(L"../shaders.fx", "PsEmissiveSurf", "ps_4_0", mPixelShaderSolid))
        return false;

  //if (!sGeometry.GenerateCube())
  //if (!sGeometry.GenerateOctahedron())
    if (!sGeometry.GenerateSphere())
        return false;
    if (!sGeometry.CreateDeviceBuffers(ctx))
        return false;

    // Set geometry buffers
    UINT stride = sizeof(SceneVertex);
    UINT offset = 0;
    immCtx->IASetVertexBuffers(0, 1, &sGeometry.mVertexBuffer, &stride, &offset);
    immCtx->IASetIndexBuffer(sGeometry.mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    immCtx->IASetPrimitiveTopology(sGeometry.topology);

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

    // Load texture
    hr = D3DX11CreateShaderResourceViewFromFile(device, L"../uv_grid_ash.dds", nullptr, nullptr, &mTextureRV, nullptr);
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
    mMainObjectWorldMtrx = XMMatrixIdentity();
    mViewMtrx = XMMatrixLookAtLH(sViewData.eye, sViewData.at, sViewData.up);
    mProjectionMtrx = XMMatrixPerspectiveFovLH(XM_PIDIV4,
                                                 (FLOAT)wndWidth / wndHeight,
                                                 0.01f, 100.0f);

    // Update constant buffers which can be updated now

    CbNeverChanged cbNeverChanged;
    cbNeverChanged.View = XMMatrixTranspose(mViewMtrx);
    immCtx->UpdateSubresource(mCbNeverChanged, 0, NULL, &cbNeverChanged, 0, 0);

    CbChangedOnResize cbChangedOnResize;
    cbChangedOnResize.Projection = XMMatrixTranspose(mProjectionMtrx);
    immCtx->UpdateSubresource(mCbChangedOnResize, 0, NULL, &cbChangedOnResize, 0, 0);

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
    Utils::ReleaseAndMakeNull(mTextureRV);
    Utils::ReleaseAndMakeNull(mSamplerLinear);
    sGeometry.Destroy();
}


void Scene::Animate(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    const float time = ctx.GetCurrentAnimationTime();
    const float period = 20.f; //seconds
    const float totalAnimPos = time / period;
    const float angle = totalAnimPos * XM_2PI;

    // Rotate main object
    XMMATRIX mainObjectScaleMtrx = XMMatrixScaling(2.6f, 2.6f, 2.6f);
    //XMMATRIX mainObjectTrnslMtrx = XMMatrixTranslation(0.f, cos(5 * totalAnimPos * XM_2PI) - 1.5f, 0.f);
    XMMATRIX mainObjectRotMtrx = XMMatrixRotationY(angle);
    mMainObjectWorldMtrx = mainObjectScaleMtrx * mainObjectRotMtrx /** mainObjectTrnslMtrx*/;

    // Update mesh color
    mMeshColor.x = ( sinf( totalAnimPos * 1.0f ) + 1.0f ) * 0.5f;
    mMeshColor.y = ( cosf( totalAnimPos * 3.0f ) + 1.0f ) * 0.5f;
    mMeshColor.z = ( sinf( totalAnimPos * 5.0f ) + 1.0f ) * 0.5f;

    // Directional light is steady
    sDirectLights[0].dirTransf = sDirectLights[0].dir;

    // Point lights are rotated
    for (int i = 0; i < sPointLights.size(); i++)
    {
        XMMATRIX rotationMtrx = XMMatrixRotationY(-2.f * angle);
        XMVECTOR lightDirVec = XMLoadFloat4(&sPointLights[i].pos);
        lightDirVec = XMVector3Transform(lightDirVec, rotationMtrx);
        XMStoreFloat4(&sPointLights[i].posTransf, lightDirVec);
    }
}


void Scene::Render(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    auto immCtx = ctx.GetImmediateContext();

    // Constant buffer - main object
    CbChangedEachFrame cbEachFrame;
    cbEachFrame.World = XMMatrixTranspose(mMainObjectWorldMtrx);
    cbEachFrame.MeshColor = mMeshColor;
    cbEachFrame.AmbientLight = sAmbientLight.color;
    for (int i = 0; i < sDirectLights.size(); i++)
    {
        cbEachFrame.DirectLightDirs[i]   = sDirectLights[i].dirTransf;
        cbEachFrame.DirectLightColors[i] = sDirectLights[i].color;
    }
    for (int i = 0; i < sPointLights.size(); i++)
    {
        cbEachFrame.PointLightDirs[i]   = sPointLights[i].posTransf;
        cbEachFrame.PointLightIntensities[i] = sPointLights[i].intensity;
    }
    immCtx->UpdateSubresource(mCbChangedEachFrame, 0, nullptr, &cbEachFrame, 0, 0);

    // Render main object
    immCtx->VSSetShader(mVertexShader, nullptr, 0);
    immCtx->VSSetConstantBuffers(0, 1, &mCbNeverChanged);
    immCtx->VSSetConstantBuffers(1, 1, &mCbChangedOnResize);
    immCtx->VSSetConstantBuffers(2, 1, &mCbChangedEachFrame);
    immCtx->PSSetShader(mPixelShaderIllum, nullptr, 0);
    immCtx->PSSetConstantBuffers(2, 1, &mCbChangedEachFrame);
    immCtx->PSSetShaderResources(0, 1, &mTextureRV);
    immCtx->PSSetSamplers(0, 1, &mSamplerLinear);
    immCtx->DrawIndexed((UINT)sGeometry.indices.size(), 0, 0);

    // Symbolic light geometry for directional lights
    for (int i = 0; i < sDirectLights.size(); i++)
    {
        XMMATRIX lightScaleMtrx = XMMatrixScaling(0.1f, 0.1f, 0.1f);
        XMMATRIX lightTrnslMtrx = XMMatrixTranslationFromVector(4.8f * XMLoadFloat4(&sDirectLights[i].dirTransf));
        XMMATRIX lightMtrx = lightScaleMtrx * lightTrnslMtrx;
        cbEachFrame.World = XMMatrixTranspose(lightMtrx);
        cbEachFrame.MeshColor = sDirectLights[i].color;
        immCtx->UpdateSubresource(mCbChangedEachFrame, 0, nullptr, &cbEachFrame, 0, 0);

        immCtx->PSSetShader(mPixelShaderSolid, nullptr, 0);
        immCtx->DrawIndexed((UINT)sGeometry.indices.size(), 0, 0);
    }

    // Symbolic light geometry for point lights
    for (int i = 0; i < sPointLights.size(); i++)
    {
        const float radius = 0.05f;
        XMMATRIX lightScaleMtrx = XMMatrixScaling(radius, radius, radius);
        XMMATRIX lightTrnslMtrx = XMMatrixTranslationFromVector(XMLoadFloat4(&sPointLights[i].posTransf));
        XMMATRIX lightMtrx = lightScaleMtrx * lightTrnslMtrx;
        cbEachFrame.World = XMMatrixTranspose(lightMtrx);
        const float radius2 = radius * radius;
        cbEachFrame.MeshColor = {
            sPointLights[i].intensity.x / radius2,
            sPointLights[i].intensity.y / radius2,
            sPointLights[i].intensity.z / radius2,
            sPointLights[i].intensity.w / radius2,
        };
        immCtx->UpdateSubresource(mCbChangedEachFrame, 0, nullptr, &cbEachFrame, 0, 0);

        immCtx->PSSetShader(mPixelShaderSolid, nullptr, 0);
        immCtx->DrawIndexed((UINT)sGeometry.indices.size(), 0, 0);
    }
}

bool Scene::GetAmbientColor(float(&rgba)[4])
{
    rgba[0] = sAmbientLight.color.x;
    rgba[1] = sAmbientLight.color.y;
    rgba[2] = sAmbientLight.color.z;
    rgba[3] = sAmbientLight.color.w;
    return true;
}

SceneGeometry::~SceneGeometry()
{
    Destroy();
}

bool SceneGeometry::GenerateCube()
{
    vertices =
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

    indices =
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

    topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    return true;
}


bool SceneGeometry::GenerateOctahedron()
{
    vertices =
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

    indices =
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

    topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    return true;
}


bool SceneGeometry::GenerateSphere()
{
    static const WORD vertSegmCount = 40;
    static const WORD stripCount = 80;

    static_assert(vertSegmCount >= 2, "Spherical stripe must have at least 2 vertical segments");
    static_assert(stripCount >= 3, "Sphere must have at least 3 stripes");

    const WORD horzLineCount = vertSegmCount - 1;
    const WORD vertexCountPerStrip = 2 /*poles*/ + horzLineCount;
    const WORD vertexCount = (stripCount + 1) * vertexCountPerStrip;
    const WORD indexCount  = stripCount * (2 /*poles*/ + 2 * horzLineCount + 1 /*strip restart*/);

    // Vertices
    vertices.reserve(vertexCount);
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
            vertices.push_back(SceneVertex{ pt, pt,  XMFLOAT2(uLine, v) }); // position==normal
        }

        // Poles
        const XMFLOAT3 northPole(0.0f,  1.0f, 0.0f);
        const XMFLOAT3 southPole(0.0f, -1.0f, 0.0f);
        const float uPole = uLine + stripSizeRel / 2;
        vertices.push_back(SceneVertex{ northPole,  northPole,  XMFLOAT2(uPole, 0.0f) }); // position==normal
        vertices.push_back(SceneVertex{ southPole,  southPole,  XMFLOAT2(uPole, 1.0f) }); // position==normal
    }

    assert(vertices.size() == vertexCount);

    // Indices
    indices.reserve(indexCount);
    for (WORD strip = 0; strip < stripCount; strip++)
    {
        const WORD idxOffset = strip * vertexCountPerStrip;
        indices.push_back(idxOffset + vertexCountPerStrip - 2); // north pole
        for (WORD line = 0; line < horzLineCount; line++)
        {
            indices.push_back((idxOffset + line + vertexCountPerStrip) % vertexCount); // next strip, same line
            indices.push_back( idxOffset + line);
        }
        indices.push_back(idxOffset + vertexCountPerStrip - 1); // south pole
        indices.push_back(static_cast<WORD>(-1)); // strip restart
    }

    assert(indices.size() == indexCount);

    topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    //topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; // debug

    Log::Debug(L"SceneGeometry::GenerateSphere: "
               L"%d segments, %d strips => %d triangles, %d vertices, %d indices",
               vertSegmCount, stripCount,
               stripCount * (2 * horzLineCount),
               vertexCount, indexCount);

    return true;
}


bool SceneGeometry::CreateDeviceBuffers(IRenderingContext & ctx)
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
    bd.ByteWidth = (UINT)(sizeof(SceneVertex) * sGeometry.vertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(initData));
    initData.pSysMem = sGeometry.vertices.data();
    hr = device->CreateBuffer(&bd, &initData, &sGeometry.mVertexBuffer);
    if (FAILED(hr))
    {
        DestroyDeviceBuffers();
        return false;
    }

    // Index buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * (UINT)sGeometry.indices.size();
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = sGeometry.indices.data();
    hr = device->CreateBuffer(&bd, &initData, &sGeometry.mIndexBuffer);
    if (FAILED(hr))
    {
        DestroyDeviceBuffers();
        return false;
    }

    return true;
}


void SceneGeometry::Destroy()
{
    DestroyGeomData();
    DestroyDeviceBuffers();
}

void SceneGeometry::DestroyGeomData()
{
    vertices.clear();
    indices.clear();
    topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

void SceneGeometry::DestroyDeviceBuffers()
{
    Utils::ReleaseAndMakeNull(mVertexBuffer);
    Utils::ReleaseAndMakeNull(mIndexBuffer);
}
