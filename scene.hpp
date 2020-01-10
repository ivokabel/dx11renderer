#pragma once

#include "iscene.hpp"
#include "constants.hpp"

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

#include "Libs/tinygltf-2.2.0/tiny_gltf.h" // just the interfaces (no implementation)

#include <string>


struct SceneVertex
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
    XMFLOAT2 Tex;
};


class ScenePrimitive
{
public:

    ScenePrimitive();
    ScenePrimitive(const ScenePrimitive &);
    ScenePrimitive(ScenePrimitive &&);
    ~ScenePrimitive();

    ScenePrimitive& operator = (const ScenePrimitive&);
    ScenePrimitive& operator = (ScenePrimitive&&);

    bool CreateCube(IRenderingContext & ctx);
    bool CreateOctahedron(IRenderingContext & ctx);
    bool CreateSphere(IRenderingContext & ctx,
                      const WORD vertSegmCount = 40,
                      const WORD stripCount = 80);

    bool LoadFromGLTF(IRenderingContext & ctx,
                      const tinygltf::Model &model,
                      const tinygltf::Mesh &mesh,
                      const int primitiveIdx,
                      const std::wstring &logPrefix);

    void DrawGeometry(IRenderingContext &ctx, ID3D11InputLayout *vertexLayout) const;

    void SetMaterialIdx(int idx) { mMaterialIdx = idx; };
    int GetMaterialIdx() const { return mMaterialIdx; };

    void Destroy();

private:

    bool GenerateCubeGeometry();
    bool GenerateOctahedronGeometry();
    bool GenerateSphereGeometry(const WORD vertSegmCount, const WORD stripCount);

    bool LoadDataFromGLTF(const tinygltf::Model &model,
                              const tinygltf::Mesh &mesh,
                              const int primitiveIdx,
                              const std::wstring &logPrefix);

    bool CreateDeviceBuffers(IRenderingContext &ctx);

    void DestroyGeomData();
    void DestroyDeviceBuffers();

private:

    // Geometry data
    std::vector<SceneVertex>    mVertices;
    std::vector<uint32_t>       mIndices;
    D3D11_PRIMITIVE_TOPOLOGY    mTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

    // Device geometry data
    ID3D11Buffer*               mVertexBuffer = nullptr;
    ID3D11Buffer*               mIndexBuffer = nullptr;

    // Material
    int                         mMaterialIdx = -1;
};


class SceneNode
{
public:
    SceneNode(bool useDebugAnimation = false);

    ScenePrimitive* CreateEmptyPrimitive();

    void SetIdentity();
    void AddScale(double scale);
    void AddScale(const std::vector<double> &vec);
    void AddRotationQuaternion(const std::vector<double> &vec);
    void AddTranslation(const std::vector<double> &vec);
    void AddMatrix(const std::vector<double> &vec);

    bool LoadFromGLTF(IRenderingContext & ctx,
                      const tinygltf::Model &model,
                      const tinygltf::Node &node,
                      int nodeIdx,
                      const std::wstring &logPrefix);

    void Animate(IRenderingContext &ctx);

    XMMATRIX GetWorldMtrx() const { return mWorldMtrx; }

private:
    friend class Scene;
    std::vector<ScenePrimitive> mPrimitives;
    std::vector<SceneNode>      mChildren;

private:
    bool        mIsRootNode;
    XMMATRIX    mLocalMtrx;
    XMMATRIX    mWorldMtrx;
};


class SceneTexture
{
public:
    SceneTexture(XMFLOAT4 defaultConstFactor);
    SceneTexture(const SceneTexture &src);
    SceneTexture& operator =(const SceneTexture &src);
    SceneTexture(SceneTexture &&src);
    SceneTexture& operator =(SceneTexture &&src);
    ~SceneTexture();

    bool Create(IRenderingContext &ctx,
                const wchar_t *path);
    bool LoadFromGltf(const char *factorParamName,
                      const char *textureParamName,
                      IRenderingContext &ctx,
                      const tinygltf::Model &model,
                      const tinygltf::ParameterMap &params,
                      const std::wstring &logPrefix);

private:
    XMFLOAT4 mConstFactor;
    // TODO: sampler, texCoord

public:
    ID3D11ShaderResourceView* srv;
};


enum class MaterialWorkflow
{
    eNone,
    kPbrMetalness,
    kPbrSpecularity,
};


class SceneMaterial
{
public:

    SceneMaterial();

public:

    bool Create(IRenderingContext &ctx,
                const wchar_t * diffuseTexPath = nullptr,
                const wchar_t * specularTexPath = nullptr);
    bool LoadFromGltf(IRenderingContext &ctx,
                      const tinygltf::Model &model,
                      const tinygltf::Material &material,
                      const std::wstring &logPrefix);

    MaterialWorkflow GetWorkflow() const { return mWorkflow; }

    ID3D11ShaderResourceView * const * GetBaseColorSRV()         const { return &mBaseColorTexture.srv; };
    ID3D11ShaderResourceView * const * GetMetallicRoughnessSRV() const { return &mMetallicRoughnessTexture.srv; };

    ID3D11ShaderResourceView * const * GetSpecularSRV() const { return &mSpecularTexture.srv; };

private:

    MaterialWorkflow    mWorkflow;

    // PBR metal/roughness workflow
    SceneTexture        mBaseColorTexture;
    SceneTexture        mMetallicRoughnessTexture;

    // PBR specularity workflow
    SceneTexture        mSpecularTexture;
    //TODO...
};



struct AmbientLight
{
    XMFLOAT4 luminance; // omnidirectional luminance: lm * sr-1 * m-2

    AmbientLight() :
        luminance{}
    {}
};


// Directional light
struct DirectLight
{
    XMFLOAT4 dir;
    XMFLOAT4 dirTransf;
    XMFLOAT4 luminance; // lm * sr-1 * m-2

    DirectLight() :
        dir{},
        dirTransf{},
        luminance{}
    {}
};


struct PointLight
{
    XMFLOAT4 pos;
    XMFLOAT4 posTransf;
    XMFLOAT4 intensity; // luminuous intensity [cd = lm * sr-1] = luminuous flux / 4Pi

    PointLight() :
        pos{ 0.f, 0.f, 0.f, 0.f },
        posTransf{ 0.f, 0.f, 0.f, 0.f },
        intensity{ 0.f, 0.f, 0.f, 0.f }
    {}
};


class Scene : public IScene
{
public:

    enum SceneId
    {
        eHardwiredSimpleDebugSphere,
        eHardwiredEarth,
        eHardwiredThreePlanets,

        eExternalDebugTriangleWithoutIndices, // Non-indexed geometry not yet supported!
        eExternalDebugTriangle,
        eExternalDebugSimpleMeshes,
        eExternalDebugBox,
        eExternalDebugBoxInterleaved,
        eExternalDebugBoxTextured,
        eExternalDebug2CylinderEngine,
        eExternalDebugDuck,
        eExternalDebugBoomBox,
        eExternalDebugBoomBoxWithAxes,
        eExternalDebugDamagedHelmet,
        eExternalDebugFlightHelmet,

        eExternalTeslaCybertruck,
        eExternalLowPolyDrifter,
        eExternalWolfBaseMesh,
        eExternalNintendoGameBoyClassic,
        eExternalWeltron2001SpaceballRadio,
        eExternalSpotMiniRigged,
        eExternalMandaloriansHelmet,
        eExternalTheRocket,
        eExternalRoboV1,
    };

    Scene(const SceneId sceneId);
    // TODO: Scene(const std::string &sceneFilePath);
    virtual ~Scene();

    virtual bool Init(IRenderingContext &ctx) override;
    virtual void Destroy() override;
    virtual void AnimateFrame(IRenderingContext &ctx) override;
    virtual void RenderFrame(IRenderingContext &ctx) override;
    virtual bool GetAmbientColor(float(&rgba)[4]) override;

private:

    // Loads the scene specified via constructor
    bool Load(IRenderingContext &ctx);
    bool LoadExternal(IRenderingContext &ctx, const std::wstring &filePath);

    // glTF loader
    bool LoadGLTF(IRenderingContext &ctx,
                  const std::wstring &filePath);
    bool LoadMaterialsFromGltf(IRenderingContext &ctx,
                               const tinygltf::Model &model,
                               const std::wstring &logPrefix);
    bool LoadSceneFromGltf(IRenderingContext &ctx,
                           const tinygltf::Model &model,
                           const std::wstring &logPrefix);
    bool LoadSceneNodeFromGLTF(IRenderingContext &ctx,
                               SceneNode &sceneNode,
                               const tinygltf::Model &model,
                               int nodeIdx,
                               const std::wstring &logPrefix);

    // Transformations
    void AddScaleToRoots(double scale);
    void AddScaleToRoots(const std::vector<double> &vec);
    void AddRotationQuaternionToRoots(const std::vector<double> &vec);
    void AddTranslationToRoots(const std::vector<double> &vec);
    void AddMatrixToRoots(const std::vector<double> &vec);

    void RenderNode(IRenderingContext &ctx,
                    const SceneNode &node,
                    const XMMATRIX &parentWorldMtrx);

private:

    const SceneId               mSceneId;

    // Geometry
    std::vector<SceneNode>      mRootNodes;
    ScenePrimitive              mPointLightProxy;

    // Materials
    std::vector<SceneMaterial>  mMaterials;
    SceneMaterial               mDefaultMaterial;

    // Lights
    AmbientLight                                    mAmbientLight;
    std::array<DirectLight, DIRECT_LIGHTS_COUNT>    mDirectLights;
    std::array<PointLight, POINT_LIGHTS_COUNT>      mPointLights;

    // Camera
    XMMATRIX                    mViewMtrx;
    XMMATRIX                    mProjectionMtrx;

    // Shaders

    ID3D11VertexShader*         mVertexShader = nullptr;
    ID3D11PixelShader*          mPsPbrMetalness = nullptr;
    ID3D11PixelShader*          mPsPbrSpecularity = nullptr;
    ID3D11PixelShader*          mPsConstEmmisive = nullptr;
    ID3D11InputLayout*          mVertexLayout = nullptr;

    ID3D11Buffer*               mCbNeverChanged = nullptr;
    ID3D11Buffer*               mCbChangedOnResize = nullptr;
    ID3D11Buffer*               mCbChangedEachFrame = nullptr;
    ID3D11Buffer*               mCbChangedPerSceneNode = nullptr;

    ID3D11SamplerState*         mSamplerLinear = nullptr;
};
