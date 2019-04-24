#include "scene.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <array>

Scene::~Scene() {}


typedef D3D11_INPUT_ELEMENT_DESC InputElmDesc;
const std::array<InputElmDesc, 3> sVertexLayout =
{
    InputElmDesc{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    InputElmDesc{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    InputElmDesc{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};


struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
    XMFLOAT2 Tex;
};


const std::array<SimpleVertex, 6 * 4> sVertices =
{
    // Up
    SimpleVertex{ XMFLOAT3( -1.0f, 1.0f, -1.0f ),  XMFLOAT3( 0.0f, 1.0f, 0.0f ),  XMFLOAT2( 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f, 1.0f, -1.0f ),  XMFLOAT3( 0.0f, 1.0f, 0.0f ),  XMFLOAT2( 1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f, 1.0f,  1.0f ),  XMFLOAT3( 0.0f, 1.0f, 0.0f ),  XMFLOAT2( 1.0f, 1.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f, 1.0f,  1.0f ),  XMFLOAT3( 0.0f, 1.0f, 0.0f ),  XMFLOAT2( 0.0f, 1.0f ) },

    // Down
    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ), XMFLOAT2( 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ), XMFLOAT2( 1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f, -1.0f,  1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ), XMFLOAT2( 1.0f, 1.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f,  1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ), XMFLOAT2( 0.0f, 1.0f ) },

    // Side 1
    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f,  1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ), XMFLOAT2( 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ), XMFLOAT2( 1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f,  1.0f, -1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ), XMFLOAT2( 1.0f, 1.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f,  1.0f,  1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ), XMFLOAT2( 0.0f, 1.0f ) },

    // Side 3
    SimpleVertex{ XMFLOAT3(  1.0f, -1.0f,  1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ),  XMFLOAT2( 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f, -1.0f, -1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ),  XMFLOAT2( 1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f,  1.0f, -1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ),  XMFLOAT2( 1.0f, 1.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f,  1.0f,  1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ),  XMFLOAT2( 0.0f, 1.0f ) },

    // Side 2
    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f,  1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ), XMFLOAT2( 1.0f, 1.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f,  1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ), XMFLOAT2( 0.0f, 1.0f ) },

    // Side 4
    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f, 1.0f ),  XMFLOAT3( 0.0f, 0.0f, 1.0f ),  XMFLOAT2( 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f, -1.0f, 1.0f ),  XMFLOAT3( 0.0f, 0.0f, 1.0f ),  XMFLOAT2( 1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3(  1.0f,  1.0f, 1.0f ),  XMFLOAT3( 0.0f, 0.0f, 1.0f ),  XMFLOAT2( 1.0f, 1.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f,  1.0f, 1.0f ),  XMFLOAT3( 0.0f, 0.0f, 1.0f ),  XMFLOAT2( 0.0f, 1.0f ) },
};


const std::array<WORD, 6 * 6> sIndices = {
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


struct {
    XMVECTOR eye;
    XMVECTOR at;
    XMVECTOR up;
} sViewData = {
    XMVectorSet(0.0f, 4.0f, -10.0f, 0.0f),
    XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f),
    XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
};


struct Light
{
    const XMFLOAT4 dir;
          XMFLOAT4 dirTransf;
    const XMFLOAT4 color;

    Light() = delete;
    Light& operator =(const Light &a) = delete;
};


std::array<Light, 2> sLights =
{
    Light{ XMFLOAT4(-0.577f, 0.577f, -0.577f, 1.0f), XMFLOAT4(0, 0, 0, 0), XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f) },
    Light{ XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f),        XMFLOAT4(0, 0, 0, 0), XMFLOAT4(0.8f, 0.2f, 0.2f, 1.0f) },
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
    XMMATRIX World;
    XMFLOAT4 MeshColor;
    XMFLOAT4 LightDirs[2];
    XMFLOAT4 LightColors[2];
};


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

    // Create vertex buffer
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = (UINT)(sizeof(SimpleVertex) * sVertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(initData));
    initData.pSysMem = sVertices.data();
    hr = device->CreateBuffer(&bd, &initData, &mVertexBuffer);
    if (FAILED(hr))
        return false;

    // Set vertex buffer
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    immCtx->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);

    // Create index buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * (UINT)sIndices.size();
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = sIndices.data();
    hr = device->CreateBuffer(&bd, &initData, &mIndexBuffer);
    if (FAILED(hr))
        return false;

    // Set index buffer w. topology
    immCtx->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    immCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Create constant buffers
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
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
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
    mWorldMtrx = XMMatrixIdentity();
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
    Utils::ReleaseAndMakeNull(mVertexBuffer);
    Utils::ReleaseAndMakeNull(mIndexBuffer);
    Utils::ReleaseAndMakeNull(mCbNeverChanged);
    Utils::ReleaseAndMakeNull(mCbChangedOnResize);
    Utils::ReleaseAndMakeNull(mCbChangedEachFrame);
    Utils::ReleaseAndMakeNull(mTextureRV);
    Utils::ReleaseAndMakeNull(mSamplerLinear);
}


void Scene::Animate(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    const float time = ctx.GetCurrentAnimationTime();
    const float period = 20.f; //seconds
    const float totalAnimPos = time / period;
    const float angle = totalAnimPos * XM_2PI;

    // Rotate main cube
    mWorldMtrx = XMMatrixRotationY(angle);

    // Update mesh color
    mMeshColor.x = ( sinf( totalAnimPos * 1.0f ) + 1.0f ) * 0.5f;
    mMeshColor.y = ( cosf( totalAnimPos * 3.0f ) + 1.0f ) * 0.5f;
    mMeshColor.z = ( sinf( totalAnimPos * 5.0f ) + 1.0f ) * 0.5f;

    // First light is without rotation
    sLights[0].dirTransf = sLights[0].dir;

    // Second light is rotated
    XMMATRIX rotationMtrx = XMMatrixRotationY(-2.f * angle);
    XMVECTOR lightDirVec = XMLoadFloat4(&sLights[1].dir);
    lightDirVec = XMVector3Transform(lightDirVec, rotationMtrx);
    XMStoreFloat4(&sLights[1].dirTransf, lightDirVec);
}


void Scene::Render(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    auto immCtx = ctx.GetImmediateContext();

    // Constant buffer - main cube
    CbChangedEachFrame cbEachFrame;
    XMMATRIX mainCubeScaleMtrx = XMMatrixScaling(1.8f, 1.8f, 1.8f);
    XMMATRIX mainCubeMtrx = mainCubeScaleMtrx * mWorldMtrx;
    cbEachFrame.World = XMMatrixTranspose(mainCubeMtrx);
    cbEachFrame.MeshColor = mMeshColor;
    cbEachFrame.LightDirs[0] = sLights[0].dirTransf;
    cbEachFrame.LightDirs[1] = sLights[1].dirTransf;
    cbEachFrame.LightColors[0] = sLights[0].color;
    cbEachFrame.LightColors[1] = sLights[1].color;
    immCtx->UpdateSubresource(mCbChangedEachFrame, 0, nullptr, &cbEachFrame, 0, 0);

    // Render main cube
    immCtx->VSSetShader(mVertexShader, nullptr, 0);
    immCtx->VSSetConstantBuffers(0, 1, &mCbNeverChanged);
    immCtx->VSSetConstantBuffers(1, 1, &mCbChangedOnResize);
    immCtx->VSSetConstantBuffers(2, 1, &mCbChangedEachFrame);
    immCtx->PSSetShader(mPixelShaderIllum, nullptr, 0);
    immCtx->PSSetConstantBuffers(2, 1, &mCbChangedEachFrame);
    immCtx->PSSetShaderResources(0, 1, &mTextureRV);
    immCtx->PSSetSamplers(0, 1, &mSamplerLinear);
    immCtx->DrawIndexed((UINT)sIndices.size(), 0, 0);

    // Render each light proxy geometry
    for (int i = 0; i < sLights.size(); i++)
    {
        XMMATRIX lightScaleMtrx = XMMatrixScaling(0.2f, 0.2f, 0.2f);
        XMMATRIX lightTrnslMtrx = XMMatrixTranslationFromVector(4.0f * XMLoadFloat4(&sLights[i].dirTransf));
        XMMATRIX lightMtrx = lightScaleMtrx * lightTrnslMtrx;
        cbEachFrame.World = XMMatrixTranspose(lightMtrx);
        cbEachFrame.MeshColor = sLights[i].color;
        immCtx->UpdateSubresource(mCbChangedEachFrame, 0, nullptr, &cbEachFrame, 0, 0);

        immCtx->PSSetShader(mPixelShaderSolid, nullptr, 0);
        immCtx->DrawIndexed((UINT)sIndices.size(), 0, 0);
    }
}
