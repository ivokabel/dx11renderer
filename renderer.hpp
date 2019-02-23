#pragma once

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// Simple DirectX 11 renderer for learning purposes heavily based on DX11 SDK tutorials.
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------


#include "irenderer.hpp"
#include "iscene.hpp"

#include <windows.h>

// We are using an older version of DirectX headers which causes 
// "warning C4005: '...' : macro redefinition"
#pragma warning(push)
#pragma warning(disable: 4005)
#include <d3d11.h>
#include <d3dx11.h>
#pragma warning(pop)

#include <xnamath.h>

#include <array>
#include <memory>
#include <cstdint>

class SimpleDX11Renderer : public IRenderer
{
public:

    SimpleDX11Renderer(std::shared_ptr<IScene> scene);
    virtual ~SimpleDX11Renderer();

    SimpleDX11Renderer & operator=(const SimpleDX11Renderer &renderer) = delete;

    bool Init(HINSTANCE instance,
              int cmdShow,
              uint32_t wndWidth,
              uint32_t wndHeight);
    int Run();

    // IRenderer interface
    virtual ID3D11Device*           GetDevice() const;
    virtual ID3D11DeviceContext*    GetImmediateContext() const;
    virtual bool                    CompileShader(WCHAR* szFileName,
                                                  LPCSTR szEntryPoint,
                                                  LPCSTR szShaderModel,
                                                  ID3DBlob** ppBlobOut) const;
    virtual bool                    GetWindowSize(uint32_t &width,
                                                  uint32_t &height) const;
    virtual float                   GetCurrentAnimationTime() const; // In seconds

private:

    bool InitWindow(HINSTANCE instance, int cmdShow);
    void DestroyWindow();
    static LRESULT CALLBACK WndProc(HWND wnd,
                                    UINT message,
                                    WPARAM wParam,
                                    LPARAM lParam);

    bool CreateDevice();
    void DestroyDevice();
    bool InitScene();
    void DestroyScene();
    void Render();

private:

    const wchar_t * const       mWndClassName    = L"SimpleDirectX11RendererWndClass";
    const wchar_t * const       mWndName         = L"Simple DirectX 11 Renderer";

    uint32_t                    mWndWidth = 0u;
    uint32_t                    mWndHeight = 0u;

    HINSTANCE                   mInstance = nullptr;
    HWND                        mWnd = nullptr;

    D3D_DRIVER_TYPE             mDriverType = D3D_DRIVER_TYPE_NULL;
    D3D_FEATURE_LEVEL           mFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    ID3D11Device*               mDevice = nullptr;
    ID3D11DeviceContext*        mImmediateContext = nullptr;
    IDXGISwapChain*             mSwapChain = nullptr;
    ID3D11RenderTargetView*     mRenderTargetView = nullptr;
    ID3D11Texture2D*            mDepthStencil = nullptr;
    ID3D11DepthStencilView*     mDepthStencilView = nullptr;

    std::shared_ptr<IScene>     mScene;
};
