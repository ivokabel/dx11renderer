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


SimpleDX11Renderer::SimpleDX11Renderer()
{
    Log::Debug(L"Constructing renderer");
}


SimpleDX11Renderer::~SimpleDX11Renderer()
{
    Log::Debug(L"Destructing renderer");

    DestroyWindow();
    DestroyDevice();
    DestroyScene();
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

    if (!InitScene())
    {
        DestroyDevice();
        DestroyScene();
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


ID3D11Device* SimpleDX11Renderer::GetDevice()
{
    return mDevice;
}


ID3D11DeviceContext* SimpleDX11Renderer::GetImmediateContext()
{
    return mImmediateContext;
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
    Log::Debug(L"Creating device");

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
                                           &mDevice, &mFeatureLevel, &mImmediateContext);
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

    hr = mDevice->CreateRenderTargetView(backBuffer, nullptr, &mRenderTargetView);
    backBuffer->Release();
    if (FAILED(hr))
        return false;

    // Depth stencil texture
    D3D11_TEXTURE2D_DESC descDepth;
    ZeroMemory(&descDepth, sizeof(descDepth));
    descDepth.Width  = mWndWidth;
    descDepth.Height = mWndHeight;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    hr = mDevice->CreateTexture2D(&descDepth, nullptr, &mDepthStencil);
    if (FAILED(hr))
        return false;

    // Depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory(&descDSV, sizeof(descDSV));
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = mDevice->CreateDepthStencilView(mDepthStencil, &descDSV, &mDepthStencilView);
    if (FAILED(hr))
        return false;

    mImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width  = (FLOAT)mWndWidth;
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
    Log::Debug(L"Destroying device");

    if (mImmediateContext)
        mImmediateContext->ClearState();

    Utils::ReleaseAndMakeNullptr(mDepthStencil);
    Utils::ReleaseAndMakeNullptr(mDepthStencilView);
    Utils::ReleaseAndMakeNullptr(mRenderTargetView);
    Utils::ReleaseAndMakeNullptr(mSwapChain);
    Utils::ReleaseAndMakeNullptr(mImmediateContext);
    Utils::ReleaseAndMakeNullptr(mDevice);
}


bool SimpleDX11Renderer::InitScene()
{
    Log::Debug(L"Initializing scene");

    // Init scene
    bool result = [](IRenderer &renderer, Scene &mScene) -> bool
    {
        if (!renderer.IsValid())
            return false;

        auto device             = renderer.GetDevice();
        auto immediateContext   = renderer.GetImmediateContext();

        HRESULT hr = S_OK;

        // Vertex shader

        ID3DBlob* pVSBlob = nullptr;
        if (!renderer.CompileShader(L"../shaders.fx", "VS", "vs_4_0", &pVSBlob))
        {
            Log::Error(L"The FX file failed to compile.");
            return false;
        }

        hr = device->CreateVertexShader(pVSBlob->GetBufferPointer(),
                                         pVSBlob->GetBufferSize(),
                                         nullptr, &mScene.mVertexShader);
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
                                        &mScene.mVertexLayout);
        pVSBlob->Release();
        if (FAILED(hr))
            return false;
        immediateContext->IASetInputLayout(mScene.mVertexLayout);

        // Pixel shader

        ID3DBlob* pPSBlob = nullptr;
        if (!renderer.CompileShader(L"../shaders.fx", "PS", "ps_4_0", &pPSBlob))
        {
            Log::Error(L"The FX file failed to compile.");
            return false;
        }

        hr = device->CreatePixelShader(pPSBlob->GetBufferPointer(),
                                        pPSBlob->GetBufferSize(),
                                        nullptr, &mScene.mPixelShader);
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
        hr = device->CreateBuffer(&bd, &initData, &mScene.mVertexBuffer);
        if (FAILED(hr))
            return false;

        // Set vertex buffer
        UINT stride = sizeof(SimpleVertex);
        UINT offset = 0;
        immediateContext->IASetVertexBuffers(0, 1, &mScene.mVertexBuffer, &stride, &offset);

        // Create index buffer
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(WORD) * (UINT)sIndices.size();
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.CPUAccessFlags = 0;
        initData.pSysMem = sIndices.data();
        hr = device->CreateBuffer(&bd, &initData, &mScene.mIndexBuffer);
        if (FAILED(hr))
            return false;

        // Set index buffer
        immediateContext->IASetIndexBuffer(mScene.mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

        // Primitive topology
        immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Create the constant buffer
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(ConstantBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = 0;
        hr = device->CreateBuffer(&bd, nullptr, &mScene.mConstantBuffer);
        if (FAILED(hr))
            return false;

        // Object->World matrices
        mScene.mWorldMatrix1 = XMMatrixIdentity();
        mScene.mWorldMatrix2 = XMMatrixIdentity();

        return true;
    }
    (*this, mScene);
    if (!result)
        return false;

    // System-wide transformations
    mScene.mViewMatrix = XMMatrixLookAtLH(sViewData.eye, sViewData.at, sViewData.up);
    mScene.mProjectionMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2,
                                                 (FLOAT)mWndWidth / mWndHeight,
                                                 0.01f, 100.0f);

    return true;
}


void SimpleDX11Renderer::DestroyScene()
{
    Log::Debug(L"Destroying scene data");

    // TODO...
    [](Scene &mScene)
    {
        Utils::ReleaseAndMakeNullptr(mScene.mVertexBuffer);
        Utils::ReleaseAndMakeNullptr(mScene.mIndexBuffer);
        Utils::ReleaseAndMakeNullptr(mScene.mConstantBuffer);
        Utils::ReleaseAndMakeNullptr(mScene.mVertexLayout);
        Utils::ReleaseAndMakeNullptr(mScene.mVertexShader);
        Utils::ReleaseAndMakeNullptr(mScene.mPixelShader);
    }
    (mScene);
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
    // Clear backbuffer
    float clearColor[4] = { 0.08f, 0.18f, 0.29f, 1.0f }; // RGBA
    mImmediateContext->ClearRenderTargetView(mRenderTargetView, clearColor);

    // Clear depth buffer
    mImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Update and render scene
    [](IRenderer &renderer, Scene &mScene)
    {
        if (!renderer.IsValid())
            return;

        auto immediateContext = renderer.GetImmediateContext();

        const float time = renderer.GetCurrentAnimationTime();
        const float period = 20.f; //seconds
        const float totalAnimPos = time / period;
        const float angle = totalAnimPos * XM_2PI;

        // Animate first cube
        mScene.mWorldMatrix1 = XMMatrixRotationY(angle);

        // Animate second cube
        XMMATRIX spinMat = XMMatrixRotationZ(-angle * 20.f);
        XMMATRIX orbitMat = XMMatrixRotationY(-angle * 2.f);
        XMMATRIX translateMat = XMMatrixTranslation(-4.0f, 0.0f, 0.0f);
        XMMATRIX scaleMat = XMMatrixScaling(0.05f, 0.05f, 0.4f);
        mScene.mWorldMatrix2 = scaleMat * spinMat * translateMat * orbitMat;

        // Update constant buffer - first cube
        ConstantBuffer cb1;
        cb1.World = XMMatrixTranspose(mScene.mWorldMatrix1);
        cb1.View = XMMatrixTranspose(mScene.mViewMatrix);
        cb1.Projection = XMMatrixTranspose(mScene.mProjectionMatrix);
        immediateContext->UpdateSubresource(mScene.mConstantBuffer, 0, nullptr, &cb1, 0, 0);

        // Render first cube
        immediateContext->VSSetShader(mScene.mVertexShader, nullptr, 0);
        immediateContext->VSSetConstantBuffers(0, 1, &mScene.mConstantBuffer);
        immediateContext->PSSetShader(mScene.mPixelShader, nullptr, 0);
        immediateContext->DrawIndexed((UINT)sIndices.size(), 0, 0);

        // Update variables - second cube
        ConstantBuffer cb2;
        cb2.World = XMMatrixTranspose(mScene.mWorldMatrix2);
        cb2.View = XMMatrixTranspose(mScene.mViewMatrix);
        cb2.Projection = XMMatrixTranspose(mScene.mProjectionMatrix);
        immediateContext->UpdateSubresource(mScene.mConstantBuffer, 0, nullptr, &cb2, 0, 0);

        // Render second cube
        // (using the same shaders and constant buffer)
        immediateContext->DrawIndexed((UINT)sIndices.size(), 0, 0);
    }
    (*this, mScene);

    mSwapChain->Present(0, 0);
}


float SimpleDX11Renderer::GetCurrentAnimationTime() const
{
    static float time = 0.0f; // seconds

    if (mDriverType == D3D_DRIVER_TYPE_REFERENCE)
    {
        time += (float)XM_PI * 0.0125f;
    }
    else
    {
        static DWORD timeStart = 0;
        const DWORD timeCur = GetTickCount();
        if (timeStart == 0)
            timeStart = timeCur;
        time = (timeCur - timeStart) / 1000.0f;
    }

    return time;
}

