#pragma once

// We are using an older version of DirectX headers which causes 
// "warning C4005: '...' : macro redefinition"
#pragma warning(push)
#pragma warning(disable: 4005)
#include <d3d11.h>
#include <d3dx11.h>
#pragma warning(pop)

// Interface for a renderer
// Used by the scene
class IRenderer
{
public:
    virtual ID3D11Device*           GetDevice() = 0;
    virtual ID3D11DeviceContext*    GetImmediateContext() = 0;

    virtual ~IRenderer() {};

    virtual bool IsValid()
    {
        return GetDevice() && GetImmediateContext();
    };
};
