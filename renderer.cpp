#include "renderer.hpp"


SimpleDirectX11Renderer::SimpleDirectX11Renderer()
{
}


SimpleDirectX11Renderer::~SimpleDirectX11Renderer()
{
    DestroyWindow();
    DestroyDxDevice();
}


bool SimpleDirectX11Renderer::IsValid() const
{
    return mWnd
        && (mWndWidth > 0u)
        && (mWndHeight > 0u);
}


bool SimpleDirectX11Renderer::Init(HINSTANCE hInstance, int nCmdShow,
                                   int32_t wndWidth, int32_t wndHeight)
{
    mWndWidth = wndWidth;
    mWndHeight = wndHeight;

    if (!InitWindow(hInstance, nCmdShow))
        return false;

    if (!CreateDxDevice())
    {
        DestroyDxDevice();
        return false;
    }

    return true;
}


bool SimpleDirectX11Renderer::InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = nullptr; // TODO: LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = mWndClassName;
    wcex.hIconSm = nullptr; // TODO: LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
    if (!RegisterClassEx(&wcex))
        return false;

    // Create window
    mInstance = hInstance;
    RECT rc = { 0, 0, mWndWidth, mWndHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    mWnd = CreateWindow(mWndClassName, mWndName,
                        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                        rc.right - rc.left, rc.bottom - rc.top,
                        nullptr, nullptr, hInstance, nullptr);
    if (!mWnd)
        return false;

    ShowWindow(mWnd, nCmdShow);

    return true;
}


void SimpleDirectX11Renderer::DestroyWindow()
{
    ::DestroyWindow(mWnd);
    mWnd = nullptr;
    mInstance = nullptr;
}


LRESULT CALLBACK SimpleDirectX11Renderer::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}


int SimpleDirectX11Renderer::Run()
{
    // Main message loop
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
            Render();
    }

    return (int)msg.wParam;
}


bool SimpleDirectX11Renderer::CreateDxDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(mWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = mWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
    {
        mDriverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain(NULL, mDriverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
            D3D11_SDK_VERSION, &sd, &mSwapChain, &mD3dDevice, &mFeatureLevel, &mImmediateContext);
        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr))
        return false;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = NULL;
    hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr))
        return false;

    hr = mD3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &mRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
        return false;

    mImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, NULL);

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    mImmediateContext->RSSetViewports(1, &vp);

    return true;
}


void SimpleDirectX11Renderer::Render()
{
    // Just clear the backbuffer
    float ClearColor[4] = { 0.1f, 0.225f, 0.5f, 1.0f }; //red,green,blue,alpha
    mImmediateContext->ClearRenderTargetView(mRenderTargetView, ClearColor);
    mSwapChain->Present(0, 0);
}


void SimpleDirectX11Renderer::DestroyDxDevice()
{
    if (mImmediateContext) mImmediateContext->ClearState();

    if (mRenderTargetView) mRenderTargetView->Release();
    if (mSwapChain) mSwapChain->Release();
    if (mImmediateContext) mImmediateContext->Release();
    if (mD3dDevice) mD3dDevice->Release();
}
