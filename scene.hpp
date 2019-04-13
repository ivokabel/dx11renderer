#pragma once

#include "iscene.hpp"

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

class Scene : public IScene
{
public:

    virtual ~Scene();

    virtual bool Init(IRenderingContext &ctx);
    virtual void Destroy();
    virtual void Animate(IRenderingContext &ctx);
    virtual void Render(IRenderingContext &ctx);

private:

    ID3D11VertexShader*         mVertexShader = nullptr;
    ID3D11PixelShader*          mPixelShaderIllum = nullptr;
    ID3D11PixelShader*          mPixelShaderSolid = nullptr;
    ID3D11InputLayout*          mVertexLayout = nullptr;
    ID3D11Buffer*               mVertexBuffer = nullptr;
    ID3D11Buffer*               mIndexBuffer = nullptr;

    ID3D11Buffer*               mCbNeverChanges = nullptr;
    ID3D11Buffer*               mCbChangesOnResize = nullptr;
    ID3D11Buffer*               mCbChangesEachFrame = nullptr;

    ID3D11ShaderResourceView*   mTextureRV = nullptr;
    ID3D11SamplerState*         mSamplerLinear = nullptr;

    XMMATRIX                    mWorldMtrx;
    XMMATRIX                    mViewMtrx;
    XMMATRIX                    mProjectionMtrx;

    XMFLOAT4                    mMeshColor = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
};
