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
#include <vector>
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
    void SetTimeout(double timeout);
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
    virtual bool                    UsesMSAA() const;
    virtual uint32_t                GetMsaaCount() const;
    virtual uint32_t                GetMsaaQuality() const;

    virtual float                   GetCurrentAnimationTime(); // In seconds
    virtual void                    StartFrame(); // Saves time of the current frame
    virtual float                   GetFrameAnimationTime() const override; // In seconds


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
    bool EnumerateAdapters();
    IDXGIAdapter* SelectAdapter();
    void ReleaseAdapters();
    void PrintAdapters(const std::wstring logPrefix = L"");
    bool CreatePostprocessingResources();
    void DestroyDevice();
    bool InitScene();
    void DestroyScene();
    void RenderFrame();

    bool                            CompileShader(WCHAR* szFileName,
                                                  LPCSTR szEntryPoint,
                                                  LPCSTR szShaderModel,
                                                  ID3DBlob** ppBlobOut) const;

    void                            DrawFullScreenQuad(ID3D11PixelShader* PS,
                                                       UINT width,
                                                       UINT height);

    void                            ExecuteRenderPass(std::initializer_list<ID3D11ShaderResourceView*> srvs,
                                                      std::initializer_list<ID3D11SamplerState*> samplers,
                                                      ID3D11PixelShader* ps,
                                                      ID3D11RenderTargetView* rtv,
                                                      ID3D11DepthStencilView* dsv,
                                                      UINT width,
                                                      UINT height);

    static float                    GaussianDistribution(float x, float y, float rho);

    static void                     GetBloomCoeffs(DWORD textureSize,
                                                   float weights[15],
                                                   float offsets[15]);

    void                            SetBloomCB(bool horizontal);


private:

    const wchar_t * const       mWndClassName    = L"SimpleDirectX11RendererWndClass";
    const wchar_t * const       mWndName         = L"Simple DirectX 11 Renderer";

    uint32_t                    mWndWidth = 0u;
    uint32_t                    mWndHeight = 0u;

    HINSTANCE                   mInstance = nullptr;
    HWND                        mWnd = nullptr;

    double                      mTimeout = 0.;
    const UINT_PTR              mTimeoutTimerID = 1;

    D3D_DRIVER_TYPE             mDriverType = D3D_DRIVER_TYPE_NULL;
    D3D_FEATURE_LEVEL           mFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    std::vector <IDXGIAdapter*> mAdapters;
    ID3D11Device*               mDevice = nullptr;
    ID3D11DeviceContext*        mImmediateContext = nullptr;

    // Swap chain
    IDXGISwapChain*             mSwapChain = nullptr;
    ID3D11RenderTargetView*     mSwapChainRTV = nullptr;
    ID3D11Texture2D*            mSwapChainDSTex = nullptr;
    ID3D11DepthStencilView*     mSwapChainDSV = nullptr;

    float                       mFrameAnimationTime = 0;

    class PassBuffer
    {
    public:

        PassBuffer() {}
        ~PassBuffer() { Destroy(); }

        enum ECreateFlags
        {
            eRtv            = 0x01,
            eSrv            = 0x02,
            eSingleSample   = 0x04
        };

        bool Create(IRenderingContext &ctx,
                    ECreateFlags flags,
                    uint32_t scaleDownFactor);
        void Destroy();

        ID3D11Texture2D*            GetTex() { return tex; }
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
    PassBuffer                  mRenderBuffMS;
    PassBuffer                  mBloomHorzBuff;
    PassBuffer                  mBloomBuff;
    PassBuffer                  mDebugBuff;
    uint32_t                    mBloomDownscaleFactor = 4;
    struct BloomCB
    {
        XMFLOAT4 offsets[15];
        XMFLOAT4 weights[15];
    };
    ID3D11Buffer*               mBloomCB = NULL;
    ID3D11PixelShader*          mBloomPS = nullptr;
    ID3D11PixelShader*          mFinalPassPS = nullptr;
    ID3D11PixelShader*          mDebugPS = nullptr;
    ID3D11SamplerState*         mSamplerStatePoint = nullptr;
    ID3D11SamplerState*         mSamplerStateLinear = nullptr;

    std::shared_ptr<IScene>     mScene;

private: // Options

    enum PostProcessingModes
    {
        kNone   = 0x00,
        kBloom  = 0x01,
        kDebug  = 0x02,
    };

    const bool                  mUseMSAA = true;
    PostProcessingModes         mPostProcessingMode = kNone;// PostProcessingModes(kBloom | kDebug);
    DWORD                       mAnimationStartTime = 0;
    bool                        mIsAnimationActive = false;// true;//
};
