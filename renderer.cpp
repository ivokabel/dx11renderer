#include "renderer.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <d3dcompiler.h>
#include <xnamath.h>

#include <array>


struct SimpleVertex
{
    XMFLOAT3 Pos;
};


SimpleDX11Renderer::SimpleDX11Renderer()
{
    Log::Log(Log::eDebug, L"Constructing renderer");
}


SimpleDX11Renderer::~SimpleDX11Renderer()
{
    Log::Log(Log::eDebug, L"Destructing renderer");

    DestroyWindow();
    DestroyDevice();
}


//bool SimpleDX11Renderer::IsValid() const
//{
//    return mWnd
//        && (mWndWidth > 0u)
//        && (mWndHeight > 0u);
//}


bool SimpleDX11Renderer::Init(HINSTANCE instance, int cmdShow,
                              int32_t wndWidth, int32_t wndHeight)
{
    Log::Log(Log::eDebug, L"Initializing renderer");

    mWndWidth = wndWidth;
    mWndHeight = wndHeight;

    if (!InitWindow(instance, cmdShow))
        return false;

    if (!CreateDevice())
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
    Log::Log(Log::eDebug, L"Running renderer...");

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

    RECT rc;
    GetClientRect(mWnd, &rc);
    UINT width  = rc.right  - rc.left;
    UINT height = rc.bottom - rc.top;

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
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
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

    Log::Log(Log::eDebug,
               L"Created device: type %s, feature level %s",
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
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    mImmediateContext->RSSetViewports(1, &vp);

    // Compile vertex shader
    ID3DBlob* pVSBlob = NULL;
    if (!CompileShader(L"../shaders.fx", "VS", "vs_4_0", &pVSBlob))
    {
        Log::Log(Log::eError, L"The FX file failed to compile.");
        //Log::Write(Log::eError, L"The FX file failed to compile.");
        //Log::Error(L"The FX file failed to compile.");
        return false;
    }

    // Create vertex shader
    hr = mD3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),
                                        pVSBlob->GetBufferSize(),
                                        NULL, &mVertexShader);
    if (FAILED(hr))
    {
        pVSBlob->Release();
        return false;
    }

    // Input layout
    const std::array<D3D11_INPUT_ELEMENT_DESC, 1> layout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = mD3dDevice->CreateInputLayout(layout.data(), (UINT)layout.size(),
                                       pVSBlob->GetBufferPointer(),
                                       pVSBlob->GetBufferSize(),
                                       &mVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr))
        return false;
    mImmediateContext->IASetInputLayout(mVertexLayout);

    // Compile pixel shader
    ID3DBlob* pPSBlob = NULL;
    if (!CompileShader(L"../shaders.fx", "PS", "ps_4_0", &pPSBlob))
    {
        Log::Log(Log::eError, L"The FX file failed to compile.");
        return false;
    }

    // Create pixel shader
    hr = mD3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),
                                       pPSBlob->GetBufferSize(),
                                       NULL, &mPixelShader);
    pPSBlob->Release();
    if (FAILED(hr))
        return false;

    // Create vertex buffer
    const std::array<SimpleVertex, 3> vertices =
    {
        XMFLOAT3( 0.0f,  0.5f, 0.5f),
        XMFLOAT3( 0.5f, -0.5f, 0.5f),
        XMFLOAT3(-0.5f, -0.5f, 0.5f),
    };
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = (UINT)(sizeof(SimpleVertex) * vertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = vertices.data();
    hr = mD3dDevice->CreateBuffer(&bd, &InitData, &mVertexBuffer);
    if (FAILED(hr))
        return false;

    // Set vertex buffer
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    mImmediateContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);

    // Set primitive topology
    mImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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
    Utils::ReleaseAndMakeNullptr(mVertexLayout);
    Utils::ReleaseAndMakeNullptr(mVertexShader);
    Utils::ReleaseAndMakeNullptr(mPixelShader);
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
            Log::Log(Log::eError,
                       L"CompileShader: D3DX11CompileFromFile failed: \"%S\"",
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
    float clearColor[4] = { 0.1f, 0.225f, 0.5f, 1.0f }; // red,green,blue,alpha
    mImmediateContext->ClearRenderTargetView(mRenderTargetView, clearColor);

    // Render one triangle
    mImmediateContext->VSSetShader(mVertexShader, NULL, 0);
    mImmediateContext->PSSetShader(mPixelShader, NULL, 0);
    mImmediateContext->Draw(3, 0);

    mSwapChain->Present(0, 0);
}

