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
        case 'P':
            mIsPostProcessingActive = !mIsPostProcessingActive;
            Log::Debug(L"WM_KEYDOWN: Post-processing %s", mIsPostProcessingActive ? L"ON" : L"OFF");
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
    scd.SampleDesc.Count = 1; //4; //
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
    ID3D11Texture2D* swapChainBuffer = nullptr;
    hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&swapChainBuffer);
    if (FAILED(hr))
        return false;

    hr = mDevice->CreateRenderTargetView(swapChainBuffer, nullptr, &mSwapChainRTV);
    swapChainBuffer->Release();
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
    descDepth.SampleDesc.Count = 1; //4; //
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = mDevice->CreateTexture2D(&descDepth, nullptr, &mSwapChainDSTex);
    if (FAILED(hr))
        return false;

    // Depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory(&descDSV, sizeof(descDSV));
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS; //D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = mDevice->CreateDepthStencilView(mSwapChainDSTex, &descDSV, &mSwapChainDSV);
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

    mRenderBuff.Create(*this, 1);
    mBloomHorzBuff.Create(*this, mBloomDownscaleFactor);
    mBloomBuff.Create(*this, mBloomDownscaleFactor);

    // Samplers
    D3D11_SAMPLER_DESC descSampler;
    ZeroMemory(&descSampler, sizeof(descSampler));
    descSampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    descSampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    descSampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
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
    mBloomHorzBuff.Destroy();
    mBloomBuff.Destroy();
    Utils::ReleaseAndMakeNull(mBloomCB);
    Utils::ReleaseAndMakeNull(mBloomPS);
    Utils::ReleaseAndMakeNull(mFinalPassPS);
    Utils::ReleaseAndMakeNull(mSamplerStatePoint);
    Utils::ReleaseAndMakeNull(mSamplerStateLinear);

    // Swap chain
    Utils::ReleaseAndMakeNull(mSwapChainDSV);
    Utils::ReleaseAndMakeNull(mSwapChainDSTex);
    Utils::ReleaseAndMakeNull(mSwapChainRTV);

    Utils::ReleaseAndMakeNull(mSwapChain);
    Utils::ReleaseAndMakeNull(mImmediateContext);
    Utils::ReleaseAndMakeNull(mDevice);
}


bool SimpleDX11Renderer::PassBuffer::Create(IRenderingContext &ctx,
                                            uint32_t scaleDownFactor)
{
    if (!ctx.IsValid())
        return false;

    uint32_t width, height;
    if (!ctx.GetWindowSize(width, height))
        return false;

    width  /= scaleDownFactor;
    height /= scaleDownFactor;

    auto device = ctx.GetDevice();

    HRESULT hr = S_OK;

    // Texture
    D3D11_TEXTURE2D_DESC descTex;
    ZeroMemory(&descTex, sizeof(D3D11_TEXTURE2D_DESC));
    descTex.ArraySize = 1;
    descTex.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    descTex.Usage = D3D11_USAGE_DEFAULT;
    descTex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    descTex.Width  = width;
    descTex.Height = height;
    descTex.MipLevels = 1;
    descTex.SampleDesc.Count = 1;
    hr = device->CreateTexture2D(&descTex, nullptr, &tex);
    if (FAILED(hr))
        return false;

    // Render target view
    D3D11_RENDER_TARGET_VIEW_DESC descRTV;
    descRTV.Format = descTex.Format;
    descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    descRTV.Texture2D.MipSlice = 0;
    hr = device->CreateRenderTargetView(tex, &descRTV, &rtv);
    if (FAILED(hr))
        return false;

    // Shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    descSRV.Format = descTex.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MipLevels = 1;
    descSRV.Texture2D.MostDetailedMip = 0;
    hr = device->CreateShaderResourceView(tex, &descSRV, &srv);
    if (FAILED(hr))
        return false;

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


void SimpleDX11Renderer::Render()
{
    ID3D11RenderTargetView* swapChainRTV = nullptr;
    ID3D11DepthStencilView* swapChainDSV = nullptr;
    mImmediateContext->OMGetRenderTargets(1, &swapChainRTV, &swapChainDSV);

    float ambientColor[4] = {};
    if (mScene)
        mScene->GetAmbientColor(ambientColor);
    mImmediateContext->ClearRenderTargetView(swapChainRTV, ambientColor);
    mImmediateContext->ClearDepthStencilView(swapChainDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Pass 0: Replace render target view with our buffer
    if (mIsPostProcessingActive)
    {
        ID3D11RenderTargetView* aRTViews[1] = { mRenderBuff.GetRTV() };
        mImmediateContext->OMSetRenderTargets(1, aRTViews, swapChainDSV);
        mImmediateContext->ClearRenderTargetView(mRenderBuff.GetRTV(), ambientColor);
    }

    // Render scene
    if (mScene)
    {
        mScene->Animate(*this);
        mScene->Render(*this);
    }

    // Bloom - part 1: Scale image down & blur horizontally
    if (mIsPostProcessingActive)
    {
        SetBloomCB(true);

        ID3D11RenderTargetView* aRTViews[1] = { mBloomHorzBuff.GetRTV() };
        mImmediateContext->OMSetRenderTargets(1, aRTViews, nullptr);

        ID3D11ShaderResourceView* aSRViews[1] = { mRenderBuff.GetSRV() };
        mImmediateContext->PSSetShaderResources(0, 1, aSRViews);

        ID3D11SamplerState* aSamplers[] = { mSamplerStatePoint, mSamplerStateLinear };
        mImmediateContext->PSSetSamplers(0, 2, aSamplers);

        DrawFullScreenQuad(mBloomPS,
                           mWndWidth / mBloomDownscaleFactor,
                           mWndHeight / mBloomDownscaleFactor);

        ID3D11ShaderResourceView* aSRViewsNull[1] = { nullptr };
        mImmediateContext->PSSetShaderResources(0, 1, aSRViewsNull);
    }

    // Bloom - part 2: blur (downscaled image) vertically
    if (mIsPostProcessingActive)
    {
        SetBloomCB(false);

        ID3D11RenderTargetView* aRTViews[1] = { mBloomBuff.GetRTV() };
        mImmediateContext->OMSetRenderTargets(1, aRTViews, nullptr);

        ID3D11ShaderResourceView* aSRViews[1] = { mBloomHorzBuff.GetSRV() };
        mImmediateContext->PSSetShaderResources(0, 1, aSRViews);

        ID3D11SamplerState* aSamplers[] = { mSamplerStatePoint, mSamplerStateLinear };
        mImmediateContext->PSSetSamplers(0, 2, aSamplers);

        DrawFullScreenQuad(mBloomPS,
                           mWndWidth / mBloomDownscaleFactor,
                           mWndHeight / mBloomDownscaleFactor);

        ID3D11ShaderResourceView* aSRViewsNull[1] = { nullptr };
        mImmediateContext->PSSetShaderResources(0, 1, aSRViewsNull);
    }

    // TODO: Final pass: Compose original and (upscaled) blurred image
    if (mIsPostProcessingActive)
    {
        // Restore the swap chain render target for the last pass
        ID3D11RenderTargetView* aRTViews[1] = { swapChainRTV };
        mImmediateContext->OMSetRenderTargets(1, aRTViews, swapChainDSV);

        ID3D11ShaderResourceView* aSRViews[1] = { mBloomBuff.GetSRV() };
        mImmediateContext->PSSetShaderResources(0, 1, aSRViews);

        ID3D11SamplerState* aSamplers[] = { mSamplerStatePoint, mSamplerStateLinear };
        mImmediateContext->PSSetSamplers(0, 2, aSamplers);

        DrawFullScreenQuad(mFinalPassPS,
                           //mWndWidth/mBloomDownscaleFactor, mWndHeight/mBloomDownscaleFactor
                           mWndWidth, mWndHeight
        );

        ID3D11ShaderResourceView* aSRViewsNull[1] = { nullptr };
        mImmediateContext->PSSetShaderResources(0, 1, aSRViewsNull);
    }

    Utils::ReleaseAndMakeNull(swapChainRTV);
    Utils::ReleaseAndMakeNull(swapChainDSV);

    mSwapChain->Present(0, 0);
}


//void SimpleDX11Renderer::ExecuteRenderPass(ID3D11RenderTargetView* (&rtvs)[1],
//                                           ID3D11DepthStencilView* dsv,
//                                           ID3D11ShaderResourceView* (&srvs)[1],
//                                           ID3D11SamplerState* samplers[2],
//                                           ID3D11PixelShader* ps,
//                                           UINT width,
//                                           UINT height)
//{
//    mImmediateContext->OMSetRenderTargets(1, rtvs, dsv);
//    mImmediateContext->PSSetShaderResources(0, 1, srvs);
//    mImmediateContext->PSSetSamplers(0, 2, samplers);
//
//    DrawFullScreenQuad(ps, width, height);
//
//    ID3D11ShaderResourceView* NullSRVs[1] = {};
//    mImmediateContext->PSSetShaderResources(0, 1, NullSRVs);
//}


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
    const float deviation = 2.0f; // 3.0f;

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

    return time;
}
