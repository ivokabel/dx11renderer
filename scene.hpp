#pragma once

#include "iscene.hpp"

// We are using an older version of DirectX headers which causes 
// "warning C4005: '...' : macro redefinition"
#pragma warning(push)
#pragma warning(disable: 4005)
#include <d3d11.h>
#include <d3dx11.h>
#pragma warning(pop)

#include <xnamath.h>

class Scene : public IScene
{
public:
    virtual ~Scene();

    virtual bool Init(IRenderer &renderer);
    virtual void Destroy();
    virtual void Animate(IRenderer &renderer);
    virtual void Render(IRenderer &renderer);

private:
    ID3D11VertexShader*         mVertexShader = nullptr;
    ID3D11PixelShader*          mPixelShader = nullptr;
    ID3D11InputLayout*          mVertexLayout = nullptr;
    ID3D11Buffer*               mVertexBuffer = nullptr;
    ID3D11Buffer*               mIndexBuffer = nullptr;
    ID3D11Buffer*               mConstantBuffer = nullptr;

    XMMATRIX                    mWorldMatrix1;
    XMMATRIX                    mWorldMatrix2;
    XMMATRIX                    mViewMatrix;
    XMMATRIX                    mProjectionMatrix;
};
