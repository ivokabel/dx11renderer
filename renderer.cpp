#include "renderer.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <d3dcompiler.h>
#pragma warning(push)
#pragma warning(disable: 4838)
#include <xnamath.h>
#pragma warning(pop)
#include <cmath>
#include <array>


SimpleDX11Renderer::SimpleDX11Renderer(std::shared_ptr<IScene> scene) :
    mScene(scene)
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
                              uint32_t wndWidth, uint32_t wndHeight)
{
    Log::Debug(L"Initializing renderer");

    mWndWidth  = wndWidth;
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
    RECT rc = { 0, 0, (LONG)mWndWidth, (LONG)mWndHeight };
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


ID3D11Device* SimpleDX11Renderer::GetDevice() const
{
    return mDevice;
}


ID3D11DeviceContext* SimpleDX11Renderer::GetImmediateContext() const
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
    Log::Debug(L"Creating device...");

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
    scd.SampleDesc.Count = 4; //1;
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
    descDepth.SampleDesc.Count = 4; //1;
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
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS; //D3D11_DSV_DIMENSION_TEXTURE2D;
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

    Utils::ReleaseAndMakeNull(mDepthStencil);
    Utils::ReleaseAndMakeNull(mDepthStencilView);
    Utils::ReleaseAndMakeNull(mRenderTargetView);
    Utils::ReleaseAndMakeNull(mSwapChain);
    Utils::ReleaseAndMakeNull(mImmediateContext);
    Utils::ReleaseAndMakeNull(mDevice);
}


bool SimpleDX11Renderer::InitScene()
{
    Log::Debug(L"Initializing scene");

    if (mScene)
        return mScene->Init(*this);
    else
        return false;
}


void SimpleDX11Renderer::DestroyScene()
{
    Log::Debug(L"Destroying scene data");

    if (mScene)
        mScene->Destroy();
}


bool SimpleDX11Renderer::CompileShader(WCHAR* szFileName,
                                       LPCSTR szEntryPoint,
                                       LPCSTR szShaderModel,
                                       ID3DBlob** ppBlobOut) const
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


bool SimpleDX11Renderer::CreateVertexShader(WCHAR* szFileName,
                                            LPCSTR szEntryPoint,
                                            LPCSTR szShaderModel,
                                            ID3DBlob *&pVSBlob,
                                            ID3D11VertexShader *&pVertexShader) const
{
    HRESULT hr = S_OK;

    if (!CompileShader(szFileName, szEntryPoint, szShaderModel, &pVSBlob))
    {
        Log::Error(L"The FX file failed to compile.");
        return false;
    }

    hr = mDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),
                                     pVSBlob->GetBufferSize(),
                                     nullptr,
                                     &pVertexShader);
    if (FAILED(hr))
    {
        Log::Error(L"mDevice->CreateVertexShader failed.");
        pVSBlob->Release();
        return false;
    }

    return true;
}


bool SimpleDX11Renderer::CreatePixelShader(WCHAR* szFileName,
                                           LPCSTR szEntryPoint,
                                           LPCSTR szShaderModel,
                                           ID3D11PixelShader *&pPixelShader) const
{
    HRESULT hr = S_OK;
    ID3DBlob *pPSBlob;

    if (!CompileShader(szFileName, szEntryPoint, szShaderModel, &pPSBlob))
    {
        Log::Error(L"The FX file failed to compile.");
        return false;
    }

    hr = mDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),
                                    pPSBlob->GetBufferSize(),
                                    nullptr,
                                    &pPixelShader);
    pPSBlob->Release();
    if (FAILED(hr))
    {
        Log::Error(L"mDevice->CreatePixelShader failed.");
        return false;
    }

    return true;
}


void SimpleDX11Renderer::Render()
{
    float ambientColor[4] = {};
    if (mScene)
        mScene->GetAmbientColor(ambientColor);
    mImmediateContext->ClearRenderTargetView(mRenderTargetView, ambientColor);

    mImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Update and render scene
    if (mScene)
    {
        mScene->Animate(*this);
        mScene->Render(*this);
    }

    mSwapChain->Present(0, 0);
}


bool SimpleDX11Renderer::GetWindowSize(uint32_t &width,
                                       uint32_t &height) const
{
    if (!IsValid())
        return false;

    width   = mWndWidth;
    height  = mWndHeight;
    return true;
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

