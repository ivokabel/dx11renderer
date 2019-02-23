#include "scene.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <array>

Scene::~Scene() {}


struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};


const std::array<SimpleVertex, 8> sVertices = {
    SimpleVertex{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
    SimpleVertex{ XMFLOAT3(1.0f, 1.0f, -1.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
    SimpleVertex{ XMFLOAT3(1.0f, 1.0f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    SimpleVertex{ XMFLOAT3(-1.0f, 1.0f, 1.0f),  XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
    SimpleVertex{ XMFLOAT3(-1.0f, -1.0f, -1.0f),XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
    SimpleVertex{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
    SimpleVertex{ XMFLOAT3(1.0f, -1.0f, 1.0f),  XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
    SimpleVertex{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
};


const std::array<WORD, 36> sIndices = {
    3, 1, 0,
    2, 1, 3,

    0, 5, 4,
    1, 5, 0,

    3, 4, 7,
    0, 4, 3,

    1, 6, 5,
    2, 6, 1,

    2, 7, 6,
    3, 7, 2,

    6, 4, 5,
    7, 4, 6,
};


struct {
    XMVECTOR eye;
    XMVECTOR at;
    XMVECTOR up;
} sViewData = {
    XMVectorSet(0.0f, 2.0f, -5.f, 0.0f),
    XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f),
    XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
};


struct ConstantBuffer
{
    XMMATRIX World;
    XMMATRIX View;
    XMMATRIX Projection;
};


bool Scene::Init(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return false;

    uint32_t wndWidth, wndHeight;
    if (!ctx.GetWindowSize(wndWidth, wndHeight))
        return false;

    auto device             = ctx.GetDevice();
    auto immediateContext   = ctx.GetImmediateContext();

    HRESULT hr = S_OK;

    // Vertex shader

    ID3DBlob* pVSBlob = nullptr;
    if (!ctx.CompileShader(L"../shaders.fx", "VS", "vs_4_0", &pVSBlob))
    {
        Log::Error(L"The FX file failed to compile.");
        return false;
    }

    hr = device->CreateVertexShader(pVSBlob->GetBufferPointer(),
                                        pVSBlob->GetBufferSize(),
                                        nullptr, &mVertexShader);
    if (FAILED(hr))
    {
        pVSBlob->Release();
        return false;
    }

    // Input layout

    typedef D3D11_INPUT_ELEMENT_DESC InputElmDesc;
    const std::array<InputElmDesc, 2> layout =
    {
        InputElmDesc{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        InputElmDesc{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = device->CreateInputLayout(layout.data(), (UINT)layout.size(),
                                    pVSBlob->GetBufferPointer(),
                                    pVSBlob->GetBufferSize(),
                                    &mVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr))
        return false;
    immediateContext->IASetInputLayout(mVertexLayout);

    // Pixel shader

    ID3DBlob* pPSBlob = nullptr;
    if (!ctx.CompileShader(L"../shaders.fx", "PS", "ps_4_0", &pPSBlob))
    {
        Log::Error(L"The FX file failed to compile.");
        return false;
    }

    hr = device->CreatePixelShader(pPSBlob->GetBufferPointer(),
                                   pPSBlob->GetBufferSize(),
                                   nullptr, &mPixelShader);
    pPSBlob->Release();
    if (FAILED(hr))
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
    immediateContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);

    // Create index buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * (UINT)sIndices.size();
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = sIndices.data();
    hr = device->CreateBuffer(&bd, &initData, &mIndexBuffer);
    if (FAILED(hr))
        return false;

    // Set index buffer
    immediateContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Primitive topology
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Create the constant buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = device->CreateBuffer(&bd, nullptr, &mConstantBuffer);
    if (FAILED(hr))
        return false;

    // Object->World matrices
    mWorldMatrix1 = XMMatrixIdentity();
    mWorldMatrix2 = XMMatrixIdentity();

    // System-wide transformations
    mViewMatrix = XMMatrixLookAtLH(sViewData.eye, sViewData.at, sViewData.up);
    mProjectionMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2,
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
    Utils::ReleaseAndMakeNullptr(mPixelShader);
}


void Scene::Animate(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    const float time = ctx.GetCurrentAnimationTime();
    const float period = 20.f; //seconds
    const float totalAnimPos = time / period;
    const float angle = totalAnimPos * XM_2PI;

    // First cube
    mWorldMatrix1 = XMMatrixRotationY(angle);

    // Second cube
    XMMATRIX spinMat = XMMatrixRotationZ(-angle * 20.f);
    XMMATRIX orbitMat = XMMatrixRotationY(-angle * 2.f);
    XMMATRIX translateMat = XMMatrixTranslation(-4.0f, 0.0f, 0.0f);
    XMMATRIX scaleMat = XMMatrixScaling(0.05f, 0.05f, 0.4f);
    mWorldMatrix2 = scaleMat * spinMat * translateMat * orbitMat;
}


void Scene::Render(IRenderingContext &ctx)
{
    if (!ctx.IsValid())
        return;

    auto immediateContext = ctx.GetImmediateContext();

    // Update constant buffer - first cube
    ConstantBuffer cb1;
    cb1.World = XMMatrixTranspose(mWorldMatrix1);
    cb1.View = XMMatrixTranspose(mViewMatrix);
    cb1.Projection = XMMatrixTranspose(mProjectionMatrix);
    immediateContext->UpdateSubresource(mConstantBuffer, 0, nullptr, &cb1, 0, 0);

    // Render first cube
    immediateContext->VSSetShader(mVertexShader, nullptr, 0);
    immediateContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
    immediateContext->PSSetShader(mPixelShader, nullptr, 0);
    immediateContext->DrawIndexed((UINT)sIndices.size(), 0, 0);

    // Update variables - second cube
    ConstantBuffer cb2;
    cb2.World = XMMatrixTranspose(mWorldMatrix2);
    cb2.View = XMMatrixTranspose(mViewMatrix);
    cb2.Projection = XMMatrixTranspose(mProjectionMatrix);
    immediateContext->UpdateSubresource(mConstantBuffer, 0, nullptr, &cb2, 0, 0);

    // Render second cube
    // (using the same shaders and constant buffer)
    immediateContext->DrawIndexed((UINT)sIndices.size(), 0, 0);
}
