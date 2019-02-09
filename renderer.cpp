#include "renderer.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <d3dcompiler.h>
#include <xnamath.h>
#include <cmath>

#include <array>


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


struct ConstantBuffer
{
    XMMATRIX World;
    XMMATRIX View;
    XMMATRIX Projection;
};


SimpleDX11Renderer::SimpleDX11Renderer()
{
    Log::Debug(L"Constructing renderer");
}


SimpleDX11Renderer::~SimpleDX11Renderer()
{
    Log::Debug(L"Destructing renderer");

    DestroyWindow();
    DestroyDevice();
}


bool SimpleDX11Renderer::Init(HINSTANCE instance, int cmdShow,
                              int32_t wndWidth, int32_t wndHeight)
{
    Log::Debug(L"Initializing renderer");

    mWndWidth = wndWidth;
    mWndHeight = wndHeight;

    if (!InitWindow(instance, cmdShow))
        return false;

    if (!CreateDevice())
    {
        DestroyDevice();
        return false;
    }

    if (!CreateSceneData())
    {
        DestroyDevice();
        return false;
    }

    return true;
}


bool SimpleDX11Renderer::InitWindow(HINSTANCE instance, int cmdShow)
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = instance;
    wcex.hIcon = nullptr; // TODO: LoadIcon(instance, (LPCTSTR)IDI_XXX);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = mWndClassName;
    wcex.hIconSm = nullptr; // TODO: LoadIcon(instance, (LPCTSTR)IDI_XXX);
    if (!RegisterClassEx(&wcex))
        return false;

    // Create window
    mInstance = instance;
    RECT rc = { 0, 0, mWndWidth, mWndHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    mWnd = CreateWindow(mWndClassName, mWndName,
                        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                        rc.right - rc.left, rc.bottom - rc.top,
                        nullptr, nullptr, instance, nullptr);
    if (!mWnd)
        return false;

    ShowWindow(mWnd, cmdShow);

    return true;
}


void SimpleDX11Renderer::DestroyWindow()
{
    ::DestroyWindow(mWnd);
    mWnd = nullptr;
    mInstance = nullptr;
}


LRESULT CALLBACK SimpleDX11Renderer::WndProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc;

        hdc = BeginPaint(wnd, &ps);
        EndPaint(wnd, &ps);
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(wnd, message, wParam, lParam);
    }

    return 0;
}


int SimpleDX11Renderer::Run()
{
    Log::Debug(L"Running renderer...");

    // Message loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
            Render();
    }

    return (int)msg.wParam;
}


const wchar_t * DriverTypeToString(D3D_DRIVER_TYPE type)
{
    switch (type)
    {
    case D3D_DRIVER_TYPE_UNKNOWN:
    default:
        return L"Unknown";
    case D3D_DRIVER_TYPE_HARDWARE:
        return L"HAL";
    case D3D_DRIVER_TYPE_REFERENCE:
        return L"Reference";
    case D3D_DRIVER_TYPE_NULL:
        return L"Null";
    case D3D_DRIVER_TYPE_SOFTWARE:
        return L"Software";
    case D3D_DRIVER_TYPE_WARP:
        return L"WARP";
    }
}


const wchar_t * FeatureLevelToString(D3D_FEATURE_LEVEL level)
{
    switch (level)
    {
    case D3D_FEATURE_LEVEL_9_1:
        return L"9.1";
    case D3D_FEATURE_LEVEL_9_2:
        return L"9.2";
    case D3D_FEATURE_LEVEL_9_3:
        return L"9.3";
    case D3D_FEATURE_LEVEL_10_0:
        return L"10.0";
    case D3D_FEATURE_LEVEL_10_1:
        return L"10.1";
    case D3D_FEATURE_LEVEL_11_0:
        return L"11.0";
    default:
        return L"Uknonwn";
    }
}


bool SimpleDX11Renderer::CreateDevice()
{
    HRESULT hr = S_OK;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    std::array<D3D_DRIVER_TYPE, 3> driverTypes = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };

    std::array<D3D_FEATURE_LEVEL, 3> featureLevels = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    DXGI_SWAP_CHAIN_DESC scd;
    ZeroMemory(&scd, sizeof(scd));
    scd.BufferCount = 1;
    scd.BufferDesc.Width = mWndWidth;
    scd.BufferDesc.Height = mWndHeight;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = mWnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;

    for (auto driverType : driverTypes)
    {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, driverType, nullptr, createDeviceFlags,
                                           featureLevels.data(), (UINT)featureLevels.size(),
                                           D3D11_SDK_VERSION, &scd, &mSwapChain,
                                           &mD3dDevice, &mFeatureLevel, &mImmediateContext);
        if (SUCCEEDED(hr))
        {
            mDriverType = driverType;
            break;
        }
    }
    if (FAILED(hr))
        return false;

    Log::Debug(L"Created device: type %s, feature level %s",
               DriverTypeToString(mDriverType),
               FeatureLevelToString(mFeatureLevel));

    // Create a render target view
    ID3D11Texture2D* backBuffer = nullptr;
    hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
    if (FAILED(hr))
        return false;

    hr = mD3dDevice->CreateRenderTargetView(backBuffer, nullptr, &mRenderTargetView);
    backBuffer->Release();
    if (FAILED(hr))
        return false;

    mImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, nullptr);

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)mWndWidth;
    vp.Height = (FLOAT)mWndHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    mImmediateContext->RSSetViewports(1, &vp);

    return true;
}


void SimpleDX11Renderer::DestroyDevice()
{
    if (mImmediateContext)
        mImmediateContext->ClearState();

    Utils::ReleaseAndMakeNullptr(mRenderTargetView);
    Utils::ReleaseAndMakeNullptr(mSwapChain);
    Utils::ReleaseAndMakeNullptr(mImmediateContext);
    Utils::ReleaseAndMakeNullptr(mD3dDevice);

    Utils::ReleaseAndMakeNullptr(mVertexBuffer);
    Utils::ReleaseAndMakeNullptr(mIndexBuffer);
    Utils::ReleaseAndMakeNullptr(mConstantBuffer);
    Utils::ReleaseAndMakeNullptr(mVertexLayout);
    Utils::ReleaseAndMakeNullptr(mVertexShader);
    Utils::ReleaseAndMakeNullptr(mPixelShader);
}


bool SimpleDX11Renderer::CreateSceneData()
{
    HRESULT hr = S_OK;

    // Vertex shader

    ID3DBlob* pVSBlob = nullptr;
    if (!CompileShader(L"../shaders.fx", "VS", "vs_4_0", &pVSBlob))
    {
        Log::Error(L"The FX file failed to compile.");
        return false;
    }

    hr = mD3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),
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
    hr = mD3dDevice->CreateInputLayout(layout.data(), (UINT)layout.size(),
                                       pVSBlob->GetBufferPointer(),
                                       pVSBlob->GetBufferSize(),
                                       &mVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr))
        return false;
    mImmediateContext->IASetInputLayout(mVertexLayout);

    // Pixel shader

    ID3DBlob* pPSBlob = nullptr;
    if (!CompileShader(L"../shaders.fx", "PS", "ps_4_0", &pPSBlob))
    {
        Log::Error(L"The FX file failed to compile.");
        return false;
    }

    hr = mD3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),
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
    hr = mD3dDevice->CreateBuffer(&bd, &initData, &mVertexBuffer);
    if (FAILED(hr))
        return false;

    // Set vertex buffer
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    mImmediateContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);

    // Create index buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * (UINT)sIndices.size();
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = sIndices.data();
    hr = mD3dDevice->CreateBuffer(&bd, &initData, &mIndexBuffer);
    if (FAILED(hr))
        return false;

    // Set index buffer
    mImmediateContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Primitive topology
    mImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Create the constant buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = mD3dDevice->CreateBuffer(&bd, nullptr, &mConstantBuffer);
    if (FAILED(hr))
        return false;

    // World matrix
    mWorldMatrix = XMMatrixIdentity();

    // View matrix
    XMVECTOR vecEye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
    XMVECTOR vecAt  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR vecUp  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    mViewMatrix = XMMatrixLookAtLH(vecEye, vecAt, vecUp);

    // Projection matrix
    mProjectionMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2, (FLOAT)mWndWidth / mWndHeight, 0.01f, 100.0f);

    return true;
}


bool SimpleDX11Renderer::CompileShader(WCHAR* szFileName,
                                       LPCSTR szEntryPoint,
                                       LPCSTR szShaderModel,
                                       ID3DBlob** ppBlobOut)
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
                               dwShaderFlags, 0, nullptr, ppBlobOut, &pErrorBlob, nullptr);
    if (FAILED(hr))
    {
        if (pErrorBlob)
            Log::Error(L"CompileShader: D3DX11CompileFromFile failed: \"%S\"",
                       (char*)pErrorBlob->GetBufferPointer());
        if (pErrorBlob)
            pErrorBlob->Release();
        return false;
    }

    if (pErrorBlob)
        pErrorBlob->Release();

    return true;
}


void SimpleDX11Renderer::Render()
{
    // Update time
    static float time = 0.0f;
    if (mDriverType == D3D_DRIVER_TYPE_REFERENCE)
    {
        time += (float)XM_PI * 0.0125f;
    }
    else
    {
        static DWORD dwTimeStart = 0;
        DWORD dwTimeCur = GetTickCount();
        if (dwTimeStart == 0)
            dwTimeStart = dwTimeCur;
        time = (dwTimeCur - dwTimeStart) / 1000.0f;
    }

    // Animation
    mWorldMatrix = XMMatrixRotationY(time);
    //auto Rotation = XMMatrixRotationY(time);
    //const float scaleBase = fmod(time, 1.f);
    //const float scalePulse = 0.5f * (cos(scaleBase * XM_2PI) + 1.f); // 0-1
    //const float scaleFactor = 1.5f * scalePulse + 0.1f;
    //auto Scaling = XMMatrixScaling(scaleFactor, scaleFactor, scaleFactor);
    //mWorldMatrix = XMMatrixMultiply(Scaling, Rotation);
    //mWorldMatrix = Scaling;

    // Clear backbuffer
    float clearColor[4] = { 0.10f, 0.34f, 0.51f, 1.0f }; // RGBA
    mImmediateContext->ClearRenderTargetView(mRenderTargetView, clearColor);

    // Update constant buffer
    ConstantBuffer cb;
    cb.World       = XMMatrixTranspose(mWorldMatrix);
    cb.View        = XMMatrixTranspose(mViewMatrix);
    cb.Projection  = XMMatrixTranspose(mProjectionMatrix);
    mImmediateContext->UpdateSubresource(mConstantBuffer, 0, nullptr, &cb, 0, 0);

    // Render cube
    mImmediateContext->VSSetShader(mVertexShader, nullptr, 0);
    mImmediateContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
    mImmediateContext->PSSetShader(mPixelShader, nullptr, 0);
    mImmediateContext->DrawIndexed((UINT)sIndices.size(), 0, 0);

    mSwapChain->Present(0, 0);
}

