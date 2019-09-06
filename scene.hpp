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

#include <string>

class Scene : public IScene
{
public:

    enum SceneId
    {
        eExternal,
        eHardwiredSimpleDebugSphere,
        eHardwiredEarth,
        eHardwiredThreePlanets,
    };

    Scene(const SceneId sceneId);
    // TODO: Scene(const std::string &sceneFilePath);
    virtual ~Scene();

    virtual bool Init(IRenderingContext &ctx) override;
    virtual void Destroy() override;
    virtual void Animate(IRenderingContext &ctx) override;
    virtual void Render(IRenderingContext &ctx) override;
    virtual bool GetAmbientColor(float(&rgba)[4]) override;

private:

    // Loads the scene specified via constructor
    bool Load(IRenderingContext &ctx);
    bool LoadExternal(IRenderingContext &ctx, const std::wstring &filePath);
    bool LoadGLTF(IRenderingContext &ctx, const std::wstring &filePath);

private:

    const SceneId               mSceneId;

    ID3D11VertexShader*         mVertexShader = nullptr;
    ID3D11PixelShader*          mPixelShaderIllum = nullptr;
    ID3D11PixelShader*          mPixelShaderSolid = nullptr;
    ID3D11InputLayout*          mVertexLayout = nullptr;

    ID3D11Buffer*               mCbNeverChanged = nullptr;
    ID3D11Buffer*               mCbChangedOnResize = nullptr;
    ID3D11Buffer*               mCbChangedEachFrame = nullptr;
    ID3D11Buffer*               mCbChangedPerObject = nullptr;

    ID3D11SamplerState*         mSamplerLinear = nullptr;

    XMMATRIX                    mViewMtrx;
    XMMATRIX                    mProjectionMtrx;
};
