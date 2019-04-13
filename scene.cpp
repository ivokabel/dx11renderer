#include "scene.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <array>

Scene::~Scene() {}


typedef D3D11_INPUT_ELEMENT_DESC InputElmDesc;
const std::array<InputElmDesc, 2> sVertexLayout =
{
    InputElmDesc{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    InputElmDesc{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};


struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
};


const std::array<SimpleVertex, 6 * 4> sVertices =
{
    SimpleVertex{ XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT3( 0.0f, 1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT3( 0.0f, 1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT3( 0.0f, 1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT3( 0.0f, 1.0f, 0.0f ) },

    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ) },

    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ) },

    SimpleVertex{ XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ) },

    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ) },

    SimpleVertex{ XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
    SimpleVertex{ XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
    SimpleVertex{ XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
};


const std::array<WORD, 36> sIndices = {
    3, 1, 0,
    2, 1, 3,

    6, 4, 5,
    7, 4, 6,

    11, 9, 8,
    10, 9, 11,

    14, 12, 13,
    15, 12, 14,

    19, 17, 16,
    18, 17, 19,

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
    Light{ XMFLOAT4(-0.577f, 0.577f, -0.577f, 1.0f), XMFLOAT4(0, 0, 0, 0), XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f) },
    Light{ XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f),        XMFLOAT4(0, 0, 0, 0), XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f) },
};


struct ConstantBuffer
{
    XMMATRIX World;
    XMMATRIX View;
    XMMATRIX Projection;

    XMFLOAT4 LightDirs[2];
    XMFLOAT4 LightColors[2];
    XMFLOAT4 OutputColor;
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
    if (!ctx.CreatePixelShader(L"../shaders.fx", "PSIllum", "ps_4_0", mPixelShaderIllum))
        return false;

    // Pixel shader - surface with solid color
    if (!ctx.CreatePixelShader(L"../shaders.fx", "PSSolid", "ps_4_0", mPixelShaderSolid))
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

    // Create the constant buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = device->CreateBuffer(&bd, nullptr, &mConstantBuffer);
    if (FAILED(hr))
        return false;

    // Matrices
    mWorldMatrix = XMMatrixIdentity();
    mViewMatrix = XMMatrixLookAtLH(sViewData.eye, sViewData.at, sViewData.up);
    mProjectionMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV4,
                                                 (FLOAT)wndWidth / wndHeight,
                                                 0.01f, 100.0f);

    return true;
}


void Scene::Destroy()
{
    Utils::ReleaseAndMakeNullptr(mVertexBuffer);
    Utils::ReleaseAndMakeNullptr(mIndexBuffer);
    Utils::ReleaseAndMakeNullptr(mConstantBuffer);
    Utils::ReleaseAndMakeNullptr(mVertexLayout);
    Utils::ReleaseAndMakeNullptr(mVertexShader);
    Utils::ReleaseAndMakeNullptr(mPixelShaderIllum);
    Utils::ReleaseAndMakeNullptr(mPixelShaderSolid);
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
    mWorldMatrix = XMMatrixRotationY(angle);

    // First light without rotation
    sLights[0].dirTransf = sLights[0].dir;

    // Second light is rotated
    XMMATRIX rotationMat = XMMatrixRotationY(-2.f * angle);
    XMVECTOR lightDirVec = XMLoadFloat4(&sLights[1].dir);
    lightDirVec = XMVector3Transform(lightDirVec, rotationMat);
    XMStoreFloat4(&sLights[1].dirTransf, lightDirVec);
}


void Scene::Render(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    auto immCtx = ctx.GetImmediateContext();

    // Constant buffer - main cube
    ConstantBuffer cb;
    cb.World = XMMatrixTranspose(mWorldMatrix);
    cb.View = XMMatrixTranspose(mViewMatrix);
    cb.Projection = XMMatrixTranspose(mProjectionMatrix);
    cb.LightDirs[0] = sLights[0].dirTransf;
    cb.LightDirs[1] = sLights[1].dirTransf;
    cb.LightColors[0] = sLights[0].color;
    cb.LightColors[1] = sLights[1].color;
    cb.OutputColor = XMFLOAT4(0, 0, 0, 0);
    immCtx->UpdateSubresource(mConstantBuffer, 0, NULL, &cb, 0, 0);

    // Render main cube
    immCtx->VSSetShader(mVertexShader, nullptr, 0);
    immCtx->VSSetConstantBuffers(0, 1, &mConstantBuffer);
    immCtx->PSSetShader(mPixelShaderIllum, nullptr, 0);
    immCtx->PSSetConstantBuffers(0, 1, &mConstantBuffer);
    immCtx->DrawIndexed((UINT)sIndices.size(), 0, 0);

    // Render each light proxy geometry
    for (int i = 0; i < sLights.size(); i++)
    {
        XMMATRIX lightScaleMat = XMMatrixScaling(0.2f, 0.2f, 0.2f);
        XMMATRIX lightTrnslMat = XMMatrixTranslationFromVector(5.0f * XMLoadFloat4(&sLights[i].dirTransf));
        XMMATRIX lightMat = lightScaleMat * lightTrnslMat;

        cb.World = XMMatrixTranspose(lightMat);
        cb.OutputColor = sLights[i].color;
        immCtx->UpdateSubresource(mConstantBuffer, 0, NULL, &cb, 0, 0);

        immCtx->PSSetShader(mPixelShaderSolid, NULL, 0);
        immCtx->DrawIndexed((UINT)sIndices.size(), 0, 0);
    }


}
