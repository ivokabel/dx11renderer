#pragma once

// We are using an older version of DirectX headers which causes 
// "warning C4005: '...' : macro redefinition"
#pragma warning(push)
#pragma warning(disable: 4005)
#include <d3d11.h>
#include <d3dx11.h>
#pragma warning(pop)

#include <cstdint>

// Used by a scene to access necessary renderer internals
class IRenderingContext
{
public:

    virtual ~IRenderingContext() {};

    virtual ID3D11Device*           GetDevice() const = 0;

    virtual ID3D11DeviceContext*    GetImmediateContext() const = 0;

    virtual bool                    CreateVertexShader(WCHAR* szFileName,
                                                       LPCSTR szEntryPoint,
                                                       LPCSTR szShaderModel,
                                                       ID3DBlob *&pVsBlob,
                                                       ID3D11VertexShader *&pVertexShader) const = 0;

    virtual bool                    CreatePixelShader(WCHAR* szFileName,
                                                      LPCSTR szEntryPoint,
                                                      LPCSTR szShaderModel,
                                                      ID3D11PixelShader *&pPixelShader) const = 0;

    virtual bool                    GetWindowSize(uint32_t &width,
                                                  uint32_t &height) const = 0;

    virtual bool                    UsesMSAA() const = 0;
    virtual uint32_t                GetMsaaCount() const = 0;
    virtual uint32_t                GetMsaaQuality() const = 0;

    virtual float                   GetFrameAnimationTime() const = 0; // In seconds

    virtual bool IsValid() const
    {
        return GetDevice() && GetImmediateContext();
    };
};
