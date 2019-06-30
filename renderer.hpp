#pragma once

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// Simple DirectX 11 renderer for learning purposes heavily based on DX11 SDK tutorials.
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------


#include "irenderingcontext.hpp"
#include "iscene.hpp"

#include <windows.h>

// We are using an older version of DirectX headers which causes 
// "warning C4005: '...' : macro redefinition"
#pragma warning(push)
#pragma warning(disable: 4005)
#include <d3d11.h>
#include <d3dx11.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 4838)
#include <xnamath.h>
#pragma warning(pop)

#include <array>
#include <memory>
#include <cstdint>

class SimpleDX11Renderer : public IRenderingContext
{
public:

    SimpleDX11Renderer(std::shared_ptr<IScene> scene);
    virtual ~SimpleDX11Renderer();

    SimpleDX11Renderer & operator=(const SimpleDX11Renderer &ctx) = delete;

    bool Init(HINSTANCE instance,
              int cmdShow,
              uint32_t wndWidth,
              uint32_t wndHeight);
    int Run();

    // IRenderingContext interface
    virtual ID3D11Device*           GetDevice() const override;
    virtual ID3D11DeviceContext*    GetImmediateContext() const override;
    virtual bool                    CreateVertexShader(WCHAR* szFileName,
                                                       LPCSTR szEntryPoint,
                                                       LPCSTR szShaderModel,
                                                       ID3DBlob *&pVsBlob,
                                                       ID3D11VertexShader *&pVertexShader) const override;
    virtual bool                    CreatePixelShader(WCHAR* szFileName,
                                                      LPCSTR szEntryPoint,
                                                      LPCSTR szShaderModel,
                                                      ID3D11PixelShader *&pPixelShader) const override;
    virtual bool                    GetWindowSize(uint32_t &width,
                                                  uint32_t &height) const override;
    virtual float                   GetCurrentAnimationTime() const override; // In seconds

private:

    bool InitWindow(HINSTANCE instance, int cmdShow);
    void DestroyWindow();
    static LRESULT CALLBACK WndProcStatic(HWND wnd,
                                          UINT message,
                                          WPARAM wParam,
                                          LPARAM lParam);
    LRESULT WndProc(HWND wnd,
                    UINT message,
                    WPARAM wParam,
                    LPARAM lParam);

    bool CreateDevice();
    bool CreatePostprocessingResources();
    void DestroyDevice();
    bool InitScene();
    void DestroyScene();
    void Render();

    bool                            CompileShader(WCHAR* szFileName,
                                                  LPCSTR szEntryPoint,
                                                  LPCSTR szShaderModel,
                                                  ID3DBlob** ppBlobOut) const;

    void                            DrawFullScreenQuad(ID3D11PixelShader* PS,
                                                       UINT width,
                                                       UINT height);

    //void                            ExecuteRenderPass(ID3D11RenderTargetView* (&rtvs)[1],
    //                                                  ID3D11DepthStencilView* dsv,
    //                                                  ID3D11ShaderResourceView* (&srvs)[1],
    //                                                  ID3D11SamplerState* samplers[2],
    //                                                  ID3D11PixelShader* ps,
    //                                                  UINT width,
    //                                                  UINT height);

    static float                    GaussianDistribution(float x, float y, float rho);

    static void                     GetBloomCoeffs(DWORD textureSize,
                                                   float weights[15],
                                                   float offsets[15]);

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

    // Swap chain
    IDXGISwapChain*             mSwapChain = nullptr;
    ID3D11RenderTargetView*     mSwapChainRTV = nullptr;
    ID3D11Texture2D*            mSwapChainDSTex = nullptr;
    ID3D11DepthStencilView*     mSwapChainDSV = nullptr;

    class PassBuffer
    {
    public:

        PassBuffer() {}
        ~PassBuffer() { Destroy(); }

        bool Create(IRenderingContext &ctx, uint32_t scaleDownFactor);
        void Destroy();

        ID3D11RenderTargetView*     GetRTV() { return rtv; }
        ID3D11ShaderResourceView*   GetSRV() { return srv; }

    private:

        ID3D11Texture2D*            tex = nullptr;
        ID3D11RenderTargetView*     rtv = nullptr;
        ID3D11ShaderResourceView*   srv = nullptr;
    };

    // Postprocessing resources
    struct ScreenVertex
    {
        XMFLOAT4 Pos;
        XMFLOAT2 Tex;
    };
    ID3D11Buffer*               mScreenQuadVB = nullptr;
    ID3D11InputLayout*          mScreenQuadLayout = nullptr;
    ID3D11VertexShader*         mScreenQuadVS = nullptr;
    PassBuffer                  mRenderBuff;
    PassBuffer                  mBloomHorzBuff;
    uint32_t                    mBloomDownscaleFactor = 4;
    struct BloomCB
    {
        XMFLOAT4 offsets[15];
        XMFLOAT4 weights[15];
    };
    ID3D11Buffer*               mBloomCB = NULL;
    ID3D11PixelShader*          mBloomPS = nullptr;
    ID3D11PixelShader*          mPass2PS = nullptr;
    ID3D11SamplerState*         mSamplerStatePoint = nullptr;
    ID3D11SamplerState*         mSamplerStateLinear = nullptr;

    std::shared_ptr<IScene>     mScene;

    bool                        mIsPostProcessingActive = true;
};
