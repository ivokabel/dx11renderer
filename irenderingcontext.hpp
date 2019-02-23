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

    virtual bool                    CompileShader(WCHAR* szFileName,
                                                  LPCSTR szEntryPoint,
                                                  LPCSTR szShaderModel,
                                                  ID3DBlob** ppBlobOut) const = 0;

    virtual bool                    GetWindowSize(uint32_t &width,
                                                  uint32_t &height) const = 0;

    virtual float                   GetCurrentAnimationTime() const = 0; // In seconds

    virtual bool IsValid() const
    {
        return GetDevice() && GetImmediateContext();
    };
};
