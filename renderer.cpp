#include "renderer.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <d3dcompiler.h>
#pragma warning(push)
#pragma warning(disable: 4838)
#include <xnamath.h>
#pragma warning(pop)
#include <cmath>


//#define RECORDING_MODE


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
    wcex.lpfnWndProc = WndProcStatic;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(SimpleDX11Renderer*); // We store this pointer in the extra window memory
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
                        nullptr, nullptr, instance, this);
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


LRESULT CALLBACK SimpleDX11Renderer::WndProcStatic(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    SimpleDX11Renderer *pThis;

    // We store this pointer in the extra window memory
    if (message == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        pThis = static_cast<SimpleDX11Renderer*>(lpcs->lpCreateParams);
        SetWindowLongPtr(wnd, 0, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
        pThis = reinterpret_cast<SimpleDX11Renderer*>(GetWindowLongPtr(wnd, 0));

    if (pThis)
        return pThis->WndProc(wnd, message, wParam, lParam);
    else
        return DefWindowProc(wnd, message, wParam, lParam);
}


LRESULT CALLBACK SimpleDX11Renderer::WndProc(HWND wnd,
                                             UINT message,
                                             WPARAM wParam,
                                             LPARAM lParam)
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

    case WM_KEYDOWN:
    {
        switch (wParam)
        {
        case 'B':
            mPostProcessingMode = Utils::ToggleBits(mPostProcessingMode, kBloom);
            Log::Debug(L"Bloom: %s", (mPostProcessingMode & kBloom) ? L"ON" : L"OFF");
            break;
        case 'D':
            mPostProcessingMode = Utils::ToggleBits(mPostProcessingMode, kDebug);
            Log::Debug(L"Debug shader: %s", (mPostProcessingMode & kDebug) ? L"ON" : L"OFF");
            break;
        case 'A':
            mIsAnimationActive = !mIsAnimationActive;
            Log::Debug(L"Animation: %s", mIsAnimationActive ? L"ON" : L"OFF");
            break;
        }
        break;
    }

    default:
        return DefWindowProc(wnd, message, wParam, lParam);
    }

    return 0;
}


int SimpleDX11Renderer::Run()
{
    Log::Debug(L"Running renderer...");
    Log::Debug(L"-------------------");

    const auto startTime = GetCurrentAnimationTime();
    uint32_t frameCount = 0;

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
        {
            RenderFrame();
#ifdef RECORDING_MODE
            Sleep(34);
#endif
            frameCount++;
        }
    }

    // Statistics
    const auto endTime = GetCurrentAnimationTime();
    const auto timeElapsed = endTime - startTime;
    const auto avgFps = (timeElapsed > 0.0001f) ? frameCount / timeElapsed : 0;
    const auto avgDuration = frameCount ? timeElapsed / frameCount : 0;

    Log::Debug(L"-------------------");
    Log::Debug(L"Renderer finished: "
               L"time elapsed %.1f, "
               L"frames count %d, "
               L"average fps %.1f, "
               L"average frame duration %.3f ms",
               timeElapsed, frameCount, avgFps, avgDuration * 1000.f);

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

    IDXGIAdapter* adapter = SelectAdapter();

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

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = mWndWidth;
    swapChainDesc.BufferDesc.Height = mWndHeight;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = mWnd;
    swapChainDesc.SampleDesc.Count   = GetMsaaCount();
    swapChainDesc.SampleDesc.Quality = GetMsaaQuality();
    swapChainDesc.Windowed = TRUE;

    if (adapter)
    {
        hr = D3D11CreateDeviceAndSwapChain(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, createDeviceFlags,
                                           featureLevels.data(), (UINT)featureLevels.size(),
                                           D3D11_SDK_VERSION, &swapChainDesc, &mSwapChain,
                                           &mDevice, &mFeatureLevel, &mImmediateContext);
        if (FAILED(hr))
            return false;

        mDriverType = D3D_DRIVER_TYPE_HARDWARE; // TODO: ???
    }
    else
    {
        for (auto driverType : driverTypes)
        {
            hr = D3D11CreateDeviceAndSwapChain(nullptr, driverType, nullptr, createDeviceFlags,
                                               featureLevels.data(), (UINT)featureLevels.size(),
                                               D3D11_SDK_VERSION, &swapChainDesc, &mSwapChain,
                                               &mDevice, &mFeatureLevel, &mImmediateContext);
            if (SUCCEEDED(hr))
            {
                mDriverType = driverType;
                break;
            }
        }
        if (FAILED(hr))
            return false;

        IDXGIDevice1* dxgiDevice = NULL;
        if (FAILED(mDevice->QueryInterface(&dxgiDevice)))
            return false;
        if (FAILED(dxgiDevice->GetAdapter(&adapter)) || !adapter)
            return false;
    }

    DXGI_ADAPTER_DESC dxad;
    if (FAILED(adapter->GetDesc(&dxad)))
        return false;
    Log::Debug(L"Created device: type %s, feature level %s, MSAA %d:%d, adapter \"%s\"",
               DriverTypeToString(mDriverType),
               FeatureLevelToString(mFeatureLevel),
               GetMsaaCount(),
               GetMsaaQuality(),
               dxad.Description);

    // Swap chain render target view
    ID3D11Texture2D* swapChainBuffer = nullptr;
    hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&swapChainBuffer);
    if (FAILED(hr))
        return false;
    hr = mDevice->CreateRenderTargetView(swapChainBuffer, nullptr, &mSwapChainRTV);
    swapChainBuffer->Release();
    if (FAILED(hr))
        return false;

    // Depth stencil texture
    D3D11_TEXTURE2D_DESC depthDesc;
    ZeroMemory(&depthDesc, sizeof(depthDesc));
    depthDesc.Width  = mWndWidth;
    depthDesc.Height = mWndHeight;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count   = GetMsaaCount();
    depthDesc.SampleDesc.Quality = GetMsaaQuality();
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = mDevice->CreateTexture2D(&depthDesc, nullptr, &mSwapChainDSTex);
    if (FAILED(hr))
        return false;

    // Depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC depthSVDesc;
    ZeroMemory(&depthSVDesc, sizeof(depthSVDesc));
    depthSVDesc.Format = depthDesc.Format;
    depthSVDesc.ViewDimension = mUseMSAA ? D3D11_DSV_DIMENSION_TEXTURE2DMS :
                                           D3D11_DSV_DIMENSION_TEXTURE2D;
    depthSVDesc.Texture2D.MipSlice = 0;
    hr = mDevice->CreateDepthStencilView(mSwapChainDSTex, &depthSVDesc, &mSwapChainDSV);
    if (FAILED(hr))
        return false;

    mImmediateContext->OMSetRenderTargets(1, &mSwapChainRTV, mSwapChainDSV);

    if (!CreatePostprocessingResources())
        return false;

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


bool SimpleDX11Renderer::EnumerateAdapters() 
{
    ReleaseAdapters();

    IDXGIFactory* pFactory = NULL;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory)))
    {
        Log::Error(L"Failed to create adapters enumeration factory!");
        return false;
    }

    IDXGIAdapter * pAdapter;
    for (UINT i = 0;
         pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND;
         ++i)
    {
        mAdapters.push_back(pAdapter);
    }

    pFactory->Release();
    return true;
}


IDXGIAdapter* SimpleDX11Renderer::SelectAdapter()
{
#ifdef RECORDING_MODE
    return nullptr; // default adapter
#endif

    if (!EnumerateAdapters())
        return false;

    PrintAdapters();

    SIZE_T maxDedicatedVideoMemory = 0;
    IDXGIAdapter* maxMemoryAdapter = nullptr;

    for (size_t i = 0; i < mAdapters.size(); i++)
    {
        auto adapter = mAdapters[i];

        if (!adapter)
            continue;

        DXGI_ADAPTER_DESC dxad;
        if (FAILED(adapter->GetDesc(&dxad)))
            continue;

        if (dxad.DedicatedVideoMemory > maxDedicatedVideoMemory)
        {
            maxDedicatedVideoMemory = dxad.DedicatedVideoMemory;
            maxMemoryAdapter = adapter;
        }
    }

    if (!maxMemoryAdapter)
        Log::Error(L"Failed to select an adapter!");

    return maxMemoryAdapter;
}


void SimpleDX11Renderer::ReleaseAdapters()
{
    for (auto adapter : mAdapters)
        if (adapter)
            adapter->Release();
    mAdapters.clear();
}

void SimpleDX11Renderer::PrintAdapters(const std::wstring logPrefix)
{
    Log::Debug(L"Found %d adapters", mAdapters.size());
    const std::wstring &subItemsLogPrefix = logPrefix + L"   ";

    for (size_t i = 0; i < mAdapters.size(); i++)
    {
        auto adapter = mAdapters[i];

        if (!adapter)
        {
            Log::Warning(L"%s" L"Adapter %d is null!", subItemsLogPrefix.c_str(), i);
            continue;
        }

        DXGI_ADAPTER_DESC dxad;
        if (FAILED(adapter->GetDesc(&dxad)))
        {
            Log::Warning(L"%s" L"Adapter %d failed to get description!", subItemsLogPrefix.c_str(), i);
            continue;
        }

        Log::Debug(L"%s%d: %-32s, vmem %4dM, sysmem %4dM, shared sysmem %4dM",
                   subItemsLogPrefix.c_str(),
                   i,
                   dxad.Description,
                   dxad.DedicatedVideoMemory >> 20,
                   dxad.DedicatedSystemMemory >> 20,
                   dxad.SharedSystemMemory >> 20);
    }
}

bool SimpleDX11Renderer::CreatePostprocessingResources()
{
    HRESULT hr = S_OK;

    // Full screen quad vertex shader & input layout
    ID3DBlob* pVsBlob = nullptr;
    if (!CreateVertexShader(L"../post_shaders.fx", "QuadVS", "vs_4_0", pVsBlob, mScreenQuadVS))
        return false;
    const D3D11_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = mDevice->CreateInputLayout(quadlayout, 2,
                                    pVsBlob->GetBufferPointer(),
                                    pVsBlob->GetBufferSize(),
                                    &mScreenQuadLayout);
    pVsBlob->Release();
    if (FAILED(hr))
        return false;

    // Full screen quad vertex buffer
    ScreenVertex svQuad[4];
    svQuad[0].Pos = XMFLOAT4(-1.0f, 1.0f, 0.5f, 1.0f);
    svQuad[0].Tex = XMFLOAT2(0.0f, 0.0f);
    svQuad[1].Pos = XMFLOAT4(1.0f, 1.0f, 0.5f, 1.0f);
    svQuad[1].Tex = XMFLOAT2(1.0f, 0.0f);
    svQuad[2].Pos = XMFLOAT4(-1.0f, -1.0f, 0.5f, 1.0f);
    svQuad[2].Tex = XMFLOAT2(0.0f, 1.0f);
    svQuad[3].Pos = XMFLOAT4(1.0f, -1.0f, 0.5f, 1.0f);
    svQuad[3].Tex = XMFLOAT2(1.0f, 1.0f);
    D3D11_BUFFER_DESC vbDesc =
    {
        4 * sizeof(ScreenVertex),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0,
        0
    };
    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(initData));
    initData.pSysMem = svQuad;
    hr = mDevice->CreateBuffer(&vbDesc, &initData, &mScreenQuadVB);
    if (FAILED(hr))
        return false;

    auto msaaBufferFlags = PassBuffer::eRtv;
    auto postBufferFlags = (PassBuffer::ECreateFlags)(PassBuffer::eRtv |
                                                      PassBuffer::eSrv |
                                                      PassBuffer::eSingleSample);
    if (mUseMSAA)
        mRenderBuffMS.Create(*this, msaaBufferFlags, 1);
    mRenderBuff.Create(*this, postBufferFlags, 1);
    mBloomHorzBuff.Create(*this, postBufferFlags, mBloomDownscaleFactor);
    mBloomBuff.Create(*this, postBufferFlags, mBloomDownscaleFactor);
    mDebugBuff.Create(*this, postBufferFlags, 1);

    // Samplers
    D3D11_SAMPLER_DESC descSampler;
    ZeroMemory(&descSampler, sizeof(descSampler));
    descSampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    descSampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    descSampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    descSampler.MinLOD = 0;
    descSampler.MaxLOD = D3D11_FLOAT32_MAX;
    hr = mDevice->CreateSamplerState(&descSampler, &mSamplerStateLinear);
    if (FAILED(hr))
        return false;
    descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hr = mDevice->CreateSamplerState(&descSampler, &mSamplerStatePoint);
    if (FAILED(hr))
        return false;

    // Bloom constant buffer
    D3D11_BUFFER_DESC descBloomCB;
    descBloomCB.Usage = D3D11_USAGE_DYNAMIC;
    descBloomCB.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    descBloomCB.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    descBloomCB.MiscFlags = 0;
    descBloomCB.ByteWidth = sizeof(BloomCB);
    hr = mDevice->CreateBuffer(&descBloomCB, NULL, &mBloomCB);
    if (FAILED(hr))
        return false;

    // Shaders
    if (!CreatePixelShader(L"../post_shaders.fx", "BloomPS", "ps_4_0", mBloomPS))
        return false;
    if (!CreatePixelShader(L"../post_shaders.fx", "FinalPassPS", "ps_4_0", mFinalPassPS))
        return false;
    if (!CreatePixelShader(L"../post_shaders.fx", "DebugPS", "ps_4_0", mDebugPS))
        return false;

    return true;
}


void SimpleDX11Renderer::DestroyDevice()
{
    Log::Debug(L"Destroying device");

    if (mImmediateContext)
        mImmediateContext->ClearState();

    // Full screen quad resources
    Utils::ReleaseAndMakeNull(mScreenQuadVB);
    Utils::ReleaseAndMakeNull(mScreenQuadLayout);
    Utils::ReleaseAndMakeNull(mScreenQuadVS);

    // Postprocessing resources
    mRenderBuff.Destroy();
    mRenderBuffMS.Destroy();
    mBloomHorzBuff.Destroy();
    mBloomBuff.Destroy();
    mDebugBuff.Destroy();
    Utils::ReleaseAndMakeNull(mBloomCB);
    Utils::ReleaseAndMakeNull(mBloomPS);
    Utils::ReleaseAndMakeNull(mFinalPassPS);
    Utils::ReleaseAndMakeNull(mDebugPS);
    Utils::ReleaseAndMakeNull(mSamplerStatePoint);
    Utils::ReleaseAndMakeNull(mSamplerStateLinear);

    // Swap chain
    Utils::ReleaseAndMakeNull(mSwapChainDSV);
    Utils::ReleaseAndMakeNull(mSwapChainDSTex);
    Utils::ReleaseAndMakeNull(mSwapChainRTV);

    Utils::ReleaseAndMakeNull(mSwapChain);
    Utils::ReleaseAndMakeNull(mImmediateContext);
    Utils::ReleaseAndMakeNull(mDevice);

    ReleaseAdapters();
}


bool SimpleDX11Renderer::PassBuffer::Create(IRenderingContext &ctx,
                                            ECreateFlags flags,
                                            uint32_t scaleDownFactor)
{
    if (!ctx.IsValid())
        return false;

    uint32_t width, height;
    if (!ctx.GetWindowSize(width, height))
        return false;
    width  /= scaleDownFactor;
    height /= scaleDownFactor;

    HRESULT hr = S_OK;
    auto device = ctx.GetDevice();
    bool createRtv    = flags & eRtv;
    bool createSrv    = flags & eSrv;
    bool singleSample = flags & eSingleSample;
    bool generateMips = createRtv && createSrv;

    // Texture
    D3D11_TEXTURE2D_DESC textDesc;
    ZeroMemory(&textDesc, sizeof(D3D11_TEXTURE2D_DESC));
    textDesc.ArraySize = 1;
    textDesc.BindFlags =
        (createRtv ? D3D11_BIND_RENDER_TARGET   : 0) |
        (createSrv ? D3D11_BIND_SHADER_RESOURCE : 0);
    textDesc.MiscFlags = generateMips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
    textDesc.Usage = D3D11_USAGE_DEFAULT;
    textDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    textDesc.Width  = width;
    textDesc.Height = height;
    textDesc.MipLevels = generateMips ? 0 : 1;
    textDesc.SampleDesc.Count   = singleSample ? 1 : ctx.GetMsaaCount();
    textDesc.SampleDesc.Quality = singleSample ? 0 : ctx.GetMsaaQuality();
    hr = device->CreateTexture2D(&textDesc, nullptr, &tex);
    if (FAILED(hr))
        return false;

    bool isMultiSample = !singleSample && ctx.UsesMSAA();

    // Render target view
    if (createRtv)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format = textDesc.Format;
        rtvDesc.ViewDimension = isMultiSample ? D3D11_RTV_DIMENSION_TEXTURE2DMS :
                                                D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        hr = device->CreateRenderTargetView(tex, &rtvDesc, &rtv);
        if (FAILED(hr))
            return false;
    }

    // Shader resource view
    if (createSrv)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = textDesc.Format;
        srvDesc.ViewDimension = isMultiSample ? D3D11_SRV_DIMENSION_TEXTURE2DMS :
                                                D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = generateMips ? -1 : 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        hr = device->CreateShaderResourceView(tex, &srvDesc, &srv);
        if (FAILED(hr))
            return false;
    }

    return true;
}


void SimpleDX11Renderer::PassBuffer::Destroy()
{
    Utils::ReleaseAndMakeNull(rtv);
    Utils::ReleaseAndMakeNull(srv);
    Utils::ReleaseAndMakeNull(tex);
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
            Log::Error(L"CompileShader: D3DX11CompileFromFile failed: \n%S",
                       (char*)pErrorBlob->GetBufferPointer());
        if (pErrorBlob)
            pErrorBlob->Release();
        return false;
    }

    if (pErrorBlob)
        Log::Debug(L"CompileShader: D3DX11CompileFromFile: \n%S",
                   (char*)pErrorBlob->GetBufferPointer());

    if (pErrorBlob)
        pErrorBlob->Release();

    return true;
}


bool SimpleDX11Renderer::CreateVertexShader(WCHAR* szFileName,
                                            LPCSTR szEntryPoint,
                                            LPCSTR szShaderModel,
                                            ID3DBlob *&pVsBlob,
                                            ID3D11VertexShader *&pVertexShader) const
{
    HRESULT hr = S_OK;

    if (!CompileShader(szFileName, szEntryPoint, szShaderModel, &pVsBlob))
    {
        Log::Error(L"The FX file failed to compile.");
        return false;
    }

    hr = mDevice->CreateVertexShader(pVsBlob->GetBufferPointer(),
                                     pVsBlob->GetBufferSize(),
                                     nullptr,
                                     &pVertexShader);
    if (FAILED(hr))
    {
        Log::Error(L"mDevice->CreateVertexShader failed.");
        pVsBlob->Release();
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


void SimpleDX11Renderer::RenderFrame()
{
    StartFrame();

    ID3D11RenderTargetView* swapChainRTV = nullptr;
    ID3D11DepthStencilView* swapChainDSV = nullptr;
    mImmediateContext->OMGetRenderTargets(1, &swapChainRTV, &swapChainDSV);

    float ambientColor[4] = {};
    if (mScene)
        mScene->GetAmbientColor(ambientColor);
    mImmediateContext->ClearRenderTargetView(swapChainRTV, ambientColor);
    mImmediateContext->ClearDepthStencilView(swapChainDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Pass 0: Replace render target view with our buffer
    if (mPostProcessingMode != kNone)
    {
        if (mUseMSAA)
        {
            // We need to render into multi-sampled buffer, which will be converted into 
            // single-sampled before post processing passes
            ID3D11RenderTargetView* aRTViews[1] = { mRenderBuffMS.GetRTV() };
            mImmediateContext->OMSetRenderTargets(1, aRTViews, swapChainDSV);
            mImmediateContext->ClearRenderTargetView(mRenderBuffMS.GetRTV(), ambientColor);
        } 
        else
        {
            ID3D11RenderTargetView* aRTViews[1] = { mRenderBuff.GetRTV() };
            mImmediateContext->OMSetRenderTargets(1, aRTViews, swapChainDSV);
            mImmediateContext->ClearRenderTargetView(mRenderBuff.GetRTV(), ambientColor);
        }
    }

    // Render scene
    if (mScene)
    {
        mScene->AnimateFrame(*this);
        mScene->RenderFrame(*this);
    }

    // Resolve multisampled buffer into single sampled before post processing
    if ((mPostProcessingMode != kNone) && mUseMSAA)
    {
        auto renderTex = mRenderBuff.GetTex();
        auto renderTexMS = mRenderBuffMS.GetTex();
        if (renderTex && renderTexMS)
        {
            D3D11_TEXTURE2D_DESC desc;
            renderTex->GetDesc(&desc);
            mImmediateContext->ResolveSubresource(renderTex,   D3D11CalcSubresource(0, 0, 1),
                                                  renderTexMS, D3D11CalcSubresource(0, 0, 1),
                                                  desc.Format);
            ID3D11RenderTargetView* aRTViews[1] = { nullptr };
            mImmediateContext->OMSetRenderTargets(1, aRTViews, nullptr);
        }
    }

    if (mPostProcessingMode & kBloom)
    {
        // Bloom - part 1: Scale image down & blur horizontally

        SetBloomCB(true);

        mImmediateContext->GenerateMips(mRenderBuff.GetSRV()); // for nicer downscaling

        ExecuteRenderPass({ mRenderBuff.GetSRV() },
                          { mSamplerStatePoint, mSamplerStateLinear },
                          mBloomPS,
                          mBloomHorzBuff.GetRTV(), nullptr,
                          mWndWidth / mBloomDownscaleFactor,
                          mWndHeight / mBloomDownscaleFactor);

        // Bloom - part 2: blur (downscaled image) vertically

        SetBloomCB(false);

        ExecuteRenderPass({ mBloomHorzBuff.GetSRV() },
                          { mSamplerStatePoint, mSamplerStateLinear },
                          mBloomPS,
                          mBloomBuff.GetRTV(), nullptr,
                          mWndWidth / mBloomDownscaleFactor,
                          mWndHeight / mBloomDownscaleFactor);
    }

    // Final bloom pass: Compose original and (upscaled) blurred image
    if (mPostProcessingMode & kBloom)
        ExecuteRenderPass({ mRenderBuff.GetSRV(), mBloomBuff.GetSRV() },
                          { mSamplerStatePoint, mSamplerStateLinear },
                          mFinalPassPS,
                          (mPostProcessingMode & kDebug) ? mDebugBuff.GetRTV() : swapChainRTV,
                          (mPostProcessingMode & kDebug) ? nullptr             : swapChainDSV,
                          mWndWidth, mWndHeight);

    if (mPostProcessingMode & kDebug)
        ExecuteRenderPass({ (mPostProcessingMode & kBloom) ? mDebugBuff.GetSRV() : mRenderBuff.GetSRV() },
                          { mSamplerStatePoint, mSamplerStateLinear },
                          mDebugPS,
                          swapChainRTV, swapChainDSV, // restores swap chain buffers
                          mWndWidth, mWndHeight);

    Utils::ReleaseAndMakeNull(swapChainRTV);
    Utils::ReleaseAndMakeNull(swapChainDSV);

    mSwapChain->Present(0, 0);
}


void SimpleDX11Renderer::ExecuteRenderPass(std::initializer_list<ID3D11ShaderResourceView*> srvs,
                                           std::initializer_list<ID3D11SamplerState*> samplers,
                                           ID3D11PixelShader* ps,
                                           ID3D11RenderTargetView* rtv,
                                           ID3D11DepthStencilView* dsv,
                                           UINT width,
                                           UINT height)
{
    mImmediateContext->OMSetRenderTargets(1, &rtv, dsv);

    const std::vector<ID3D11ShaderResourceView*> srvsVec = srvs;
    mImmediateContext->PSSetShaderResources(0, (UINT)srvsVec.size(), srvsVec.data());

    const std::vector<ID3D11SamplerState*> samplersVec = samplers;
    mImmediateContext->PSSetSamplers(0, (UINT)samplersVec.size(), samplersVec.data());

    DrawFullScreenQuad(ps, width, height);

    const std::vector<ID3D11ShaderResourceView*> nullSrvs(srvsVec.size(), nullptr);
    mImmediateContext->PSSetShaderResources(0, (UINT)nullSrvs.size(), nullSrvs.data());
}


void SimpleDX11Renderer::DrawFullScreenQuad(ID3D11PixelShader* PS,
                                            UINT width,
                                            UINT height)
{
    // Backup viewport
    D3D11_VIEWPORT vpOrig[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
    UINT nViewPorts = 1;
    mImmediateContext->RSGetViewports(&nViewPorts, vpOrig);

    // Setup the viewport to match the backbuffer
    D3D11_VIEWPORT vp;
    vp.Width  = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    mImmediateContext->RSSetViewports(1, &vp);

    UINT strides = sizeof(ScreenVertex);
    UINT offsets = 0;
    ID3D11Buffer* pBuffers[1] = { mScreenQuadVB };

    mImmediateContext->IASetInputLayout(mScreenQuadLayout);
    mImmediateContext->IASetVertexBuffers(0, 1, pBuffers, &strides, &offsets);
    mImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    mImmediateContext->VSSetShader(mScreenQuadVS, NULL, 0);
    mImmediateContext->PSSetShader(PS, NULL, 0);
    mImmediateContext->Draw(4, 0);

    // Restore viewport
    mImmediateContext->RSSetViewports(nViewPorts, vpOrig);
}


float SimpleDX11Renderer::GaussianDistribution(float x, float y, float rho)
{
    float g = 1.0f / sqrtf(2.0f * XM_PI * rho * rho);
    return g * expf(-(x * x + y * y) / (2 * rho * rho));
}


void SimpleDX11Renderer::GetBloomCoeffs(DWORD textureSize,
                                        float weights[15],
                                        float offsets[15])
{
    const float deviation = 2.0f;

    int i = 0;
    float tu = 1.0f / (float)textureSize;

    // The center texel
    float weight = GaussianDistribution(0, 0, deviation);
    weights[7] = weight;
    offsets[7] = 0.0f;

    // One side
    for (i = 1; i < 8; i++)
    {
        weight = GaussianDistribution((float)i, 0, deviation);
        weights[7 - i] = weight;
        offsets[7 - i] = -i * tu;
    }

    // Other side - copy
    for (i = 8; i < 15; i++)
    {
        weights[i] = weights[14 - i];
        offsets[i] = -offsets[14 - i];
    }

    // Debug: identity kernel
    //ZeroMemory(weights, sizeof(float) * 15);
    //weights[7] = 1.f;
}


void SimpleDX11Renderer::SetBloomCB(bool horizontal)
{
    D3D11_MAPPED_SUBRESOURCE mappedRes;
    mImmediateContext->Map(mBloomCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
    BloomCB* cbBloom = (BloomCB*)mappedRes.pData;
    XMFLOAT4* offsets = cbBloom->offsets;
    XMFLOAT4* weights = cbBloom->weights;

    float offsetsF[15];
    float weightsF[15];
    auto wndSize = horizontal ? mWndWidth : mWndHeight;
    GetBloomCoeffs(wndSize / mBloomDownscaleFactor, weightsF, offsetsF);
    for (uint32_t i = 0; i < 15; i++)
    {
        if (horizontal)
            offsets[i] = XMFLOAT4(offsetsF[i], 0.0f, 0.0f, 0.0f);
        else
            offsets[i] = XMFLOAT4(0.0f, offsetsF[i], 0.0f, 0.0f);
        weights[i] = XMFLOAT4(weightsF[i], weightsF[i], weightsF[i], 0.0f);
    }
    mImmediateContext->Unmap(mBloomCB, 0);

    mImmediateContext->PSSetConstantBuffers(0, 1, &mBloomCB);
};


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

    return mIsAnimationActive ? time : 0.f;
}


void SimpleDX11Renderer::StartFrame()
{
    mFrameAnimationTime = GetCurrentAnimationTime();
}


float SimpleDX11Renderer::GetFrameAnimationTime() const
{
    return mFrameAnimationTime;
}


bool SimpleDX11Renderer::UsesMSAA() const
{
    return mUseMSAA;
}

uint32_t SimpleDX11Renderer::GetMsaaCount() const
{
    if (mUseMSAA)
        return 4;
    else
        return 1;
}

uint32_t SimpleDX11Renderer::GetMsaaQuality() const
{
    if (mUseMSAA)
        return 0;
    else
        return 0;
}

