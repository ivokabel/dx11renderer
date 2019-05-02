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

    ID3D11Buffer*               mCbNeverChanged = nullptr;
    ID3D11Buffer*               mCbChangedOnResize = nullptr;
    ID3D11Buffer*               mCbChangedEachFrame = nullptr;

    ID3D11ShaderResourceView*   mTextureRV = nullptr;
    ID3D11SamplerState*         mSamplerLinear = nullptr;

    XMMATRIX                    mWorldMtrx;
    XMMATRIX                    mViewMtrx;
    XMMATRIX                    mProjectionMtrx;

    XMFLOAT4                    mMeshColor;
};
