#pragma once

// ------------------------------------------------------------------------------------------------
// Simple DirectX 11 renderer for learning purposes heavily based on DX11 SDK tutorials.
// ------------------------------------------------------------------------------------------------

#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <cstdint>

class SimpleDX11Renderer
{
public:

    SimpleDX11Renderer();
    ~SimpleDX11Renderer();

    bool Init(HINSTANCE instance, int cmdShow,
              int32_t wndWidth, int32_t wndHeight);
    int Run();

private:

    bool IsValid() const;

    bool InitWindow(HINSTANCE instance, int cmdShow);
    void DestroyWindow();
    static LRESULT CALLBACK WndProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool CreateDxDevice();
    void Render();
    void DestroyDxDevice();

private:

    const wchar_t * const       mWndClassName   = L"SimpleDirectX11RendererWndClass";
    const wchar_t * const       mWndName        = L"Simple DirectX 11 Renderer";

    int32_t                     mWndWidth = 0u;
    int32_t                     mWndHeight = 0u;
    HINSTANCE                   mInstance = nullptr;
    HWND                        mWnd = nullptr;
    D3D_DRIVER_TYPE             mDriverType = D3D_DRIVER_TYPE_NULL;
    D3D_FEATURE_LEVEL           mFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    ID3D11Device*               mD3dDevice = nullptr;
    ID3D11DeviceContext*        mImmediateContext = nullptr;
    IDXGISwapChain*             mSwapChain = nullptr;
    ID3D11RenderTargetView*     mRenderTargetView = nullptr;
};
