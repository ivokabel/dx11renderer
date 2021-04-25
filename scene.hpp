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

#include "Libs/tinygltf-2.5.0/tiny_gltf.h" // just the interfaces (no implementation)

#include <string>


struct SceneVertex
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
    XMFLOAT4 Tangent; // w represents handedness of the tangent basis and is either 1 or -1
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

    bool CreateQuad(IRenderingContext & ctx);
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

    // Uses mikktspace tangent space calculator by Morten S. Mikkelsen.
    // Requires position, normal, and texture coordinates to be already loaded.
    bool CalculateTangentsIfNeeded(const std::wstring &logPrefix = std::wstring());

    size_t GetVerticesPerFace() const;
    size_t GetFacesCount() const;
    const size_t GetVertexIndex(const int face, const int vertex) const;
    const SceneVertex& GetVertex(const int face, const int vertex) const;
          SceneVertex& GetVertex(const int face, const int vertex);
    void GetPosition(float outpos[], const int face, const int vertex) const;
    void GetNormal(float outnormal[], const int face, const int vertex) const;
    void GetTextCoord(float outuv[], const int face, const int vertex) const;
    void SetTangent(const float tangent[], const float sign, const int face, const int vertex);

    bool IsTangentPresent() const { return mIsTangentPresent; }

    void DrawGeometry(IRenderingContext &ctx, ID3D11InputLayout *vertexLayout) const;

    void SetMaterialIdx(int idx) { mMaterialIdx = idx; };
    int GetMaterialIdx() const { return mMaterialIdx; };

    void Destroy();

private:

    bool GenerateQuadGeometry();
    bool GenerateCubeGeometry();
    bool GenerateOctahedronGeometry();
    bool GenerateSphereGeometry(const WORD vertSegmCount, const WORD stripCount);

    bool LoadDataFromGLTF(const tinygltf::Model &model,
                              const tinygltf::Mesh &mesh,
                              const int primitiveIdx,
                              const std::wstring &logPrefix);

    void FillFaceStripsCacheIfNeeded() const;
    bool CreateDeviceBuffers(IRenderingContext &ctx);

    void DestroyGeomData();
    void DestroyDeviceBuffers();

private:

    // Geometry data
    std::vector<SceneVertex>    mVertices;
    std::vector<uint32_t>       mIndices;
    D3D11_PRIMITIVE_TOPOLOGY    mTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    bool                        mIsTangentPresent = false;

    // Cached geometry data
    struct FaceStrip
    {
        size_t startIdx;
        size_t faceCount;
    };
    mutable bool                    mAreFaceStripsCached = false;
    mutable std::vector<FaceStrip>  mFaceStrips;
    mutable size_t                  mFaceStripsTotalCount = 0;

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
    enum ValueType
    {
        eLinear,
        eSrgb,
    };

    SceneTexture(const std::wstring &name, ValueType valueType, XMFLOAT4 neutralValue);

    SceneTexture(const SceneTexture &src);
    SceneTexture& operator =(const SceneTexture &src);
    SceneTexture(SceneTexture &&src);
    SceneTexture& operator =(SceneTexture &&src);
    ~SceneTexture();

    bool Create(IRenderingContext &ctx, const wchar_t *path);
    bool CreateNeutral(IRenderingContext &ctx);
    bool LoadTextureFromGltf(const int textureIndex,
                             IRenderingContext &ctx,
                             const tinygltf::Model &model,
                             const std::wstring &logPrefix);

    std::wstring    GetName()   const { return mName; }
    bool            IsLoaded()  const { return mIsLoaded; }

private:
    std::wstring    mName;
    ValueType       mValueType;
    XMFLOAT4        mNeutralValue;
    bool            mIsLoaded;
    // TODO: sampler, texCoord

public:
    ID3D11ShaderResourceView* srv;
};


class SceneNormalTexture : public SceneTexture
{
public:
    SceneNormalTexture(const std::wstring &name) :
        SceneTexture(name, SceneTexture::eLinear, XMFLOAT4(0.5f, 0.5f, 1.f, 1.f)) {}
    ~SceneNormalTexture() {}

    bool CreateNeutral(IRenderingContext &ctx);
    bool LoadTextureFromGltf(const tinygltf::NormalTextureInfo &normalTextureInfo,
                             const tinygltf::Model &model,
                             IRenderingContext &ctx,
                             const std::wstring &logPrefix);

    void    SetScale(float scale)   { mScale = scale; }
    float   GetScale() const        { return mScale; }

private:
    float mScale = 1.f;
};


class SceneOcclusionTexture : public SceneTexture
{
public:
    SceneOcclusionTexture(const std::wstring &name) :
        SceneTexture(name, SceneTexture::eLinear, XMFLOAT4(1.f, 0.f, 0.f, 1.f)) {}
    ~SceneOcclusionTexture() {}

    bool CreateNeutral(IRenderingContext &ctx);
    bool LoadTextureFromGltf(const tinygltf::OcclusionTextureInfo &occlusionTextureInfo,
                             const tinygltf::Model &model,
                             IRenderingContext &ctx,
                             const std::wstring &logPrefix);

    void    SetStrength(float strength) { mStrength = strength; }
    float   GetStrength() const         { return mStrength; }

private:
    float mStrength = 1.f;
};


enum class MaterialWorkflow
{
    kNone,
    kPbrMetalness,
    kPbrSpecularity,
};


class SceneMaterial
{
public:

    SceneMaterial();

public:

    bool CreatePbrSpecularity(IRenderingContext &ctx,
                              const wchar_t * diffuseTexPath,
                              XMFLOAT4 diffuseFactor,
                              const wchar_t * specularTexPath,
                              XMFLOAT4 specularFactor);

    bool CreatePbrMetalness(IRenderingContext &ctx,
                            const wchar_t * baseColorTexPath,
                            XMFLOAT4 baseColorFactor,
                            const wchar_t * metallicRoughnessTexPath,
                            float metallicFactor,
                            float roughnessFactor);

    bool LoadFromGltf(IRenderingContext &ctx,
                      const tinygltf::Model &model,
                      const tinygltf::Material &material,
                      const std::wstring &logPrefix);

    MaterialWorkflow GetWorkflow() const { return mWorkflow; }

    const SceneTexture &            GetBaseColorTexture()           const { return mBaseColorTexture; };
    XMFLOAT4                        GetBaseColorFactor()            const { return mBaseColorFactor; }
    const SceneTexture &            GetMetallicRoughnessTexture()   const { return mMetallicRoughnessTexture; };
    XMFLOAT4                        GetMetallicRoughnessFactor()    const { return mMetallicRoughnessFactor; }

    const SceneTexture &            GetSpecularTexture()            const { return mSpecularTexture; };
    XMFLOAT4                        GetSpecularFactor()             const { return mSpecularFactor; }

    const SceneNormalTexture &      GetNormalTexture()              const { return mNormalTexture; };
    const SceneOcclusionTexture &   GetOcclusionTexture()           const { return mOcclusionTexture; };
    const SceneTexture &            GetEmissionTexture()            const { return mEmissionTexture; };
    XMFLOAT4                        GetEmissionFactor()             const { return mEmissionFactor; }

    void Animate(IRenderingContext &ctx);

private:

    MaterialWorkflow    mWorkflow;

    // Metal/roughness workflow
    SceneTexture        mBaseColorTexture;
    XMFLOAT4            mBaseColorFactor;
    SceneTexture        mMetallicRoughnessTexture;
    XMFLOAT4            mMetallicRoughnessFactor;

    // Specularity workflow
    SceneTexture        mSpecularTexture;
    XMFLOAT4            mSpecularFactor;
    //TODO...

    // Both workflows
    SceneNormalTexture      mNormalTexture;
    SceneOcclusionTexture   mOcclusionTexture;
    SceneTexture            mEmissionTexture;
    XMFLOAT4                mEmissionFactor;
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
    XMFLOAT4 luminance; // lm * sr-1 * m-2 ... Really???

    DirectLight() :
        dir{},
        dirTransf{},
        luminance{}
    {}
};


struct PointLight
{
    // Parameters
    XMFLOAT4 intensity = XMFLOAT4{ 0.f, 0.f, 0.f, 0.f }; // luminuous intensity [cd = lm * sr-1] = luminuous flux / 4Pi
    float orbitRadius = 5.5f;
    float orbitInclinationMin = -XM_PIDIV4;
    float orbitInclinationMax =  XM_PIDIV4;

    // Internals
    XMFLOAT4 posTransf = XMFLOAT4{ 0.f, 0.f, 0.f, 0.f }; // Final position after animation
};


class Scene : public IScene
{
public:

    enum SceneId
    {
        eFirst, // Keep first!

        eHardwiredSimpleDebugSphere = eFirst,
        eHardwiredMaterialFactors,
        eHardwiredPbrMetalnesDebugSphere,
        eHardwiredEarth,
        eHardwiredThreePlanets,

        eDebugGradientBox,
        eDebugMetalRoughSpheresNoTextures,
        eHardwiredLightsOverQuad,

        eFirstSampleGltf, // Keep here!
        //eGltfSampleTriangleWithoutIndices, // Non-indexed geometry not yet supported!
        eGltfSampleTriangle = eFirstSampleGltf,
        eGltfSampleSimpleMeshes,
        eGltfSampleBox,
        eGltfSampleBoxInterleaved,
        eGltfSampleBoxTextured,
        eGltfSampleMetalRoughSpheres,
        eGltfSampleMetalRoughSpheresNoTextures,
        eGltfSampleNormalTangentTest,
        eGltfSampleNormalTangentMirrorTest,
        eGltfSample2CylinderEngine,
        eGltfSampleDuck,
        eGltfSampleBoomBox,
        eGltfSampleBoomBoxWithAxes,
        eGltfSampleDamagedHelmet,
        eGltfSampleFlightHelmet,
        eLastSampleGltf = eGltfSampleFlightHelmet, // Keep here!

        eLowPolyDrifter,
        eWolfBaseMesh,
        eNintendoGameBoyClassic,
        eWeltron2001SpaceballRadio,
        eSpotMiniRigged,
        eMandaloriansHelmet,
        eTheRocket,
        eRoboV1,
        eRockJacket,
        eSalazarSkull,
        eHardhead,

        eLast = eHardhead, // Keep last!
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

    bool PostLoadSanityTest();
    bool NodeTangentSanityTest(const SceneNode &node);

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

    // Materials
    const SceneMaterial& GetMaterial(const ScenePrimitive &primitive) const;

    // Lights
    void SetupDefaultLights();
    bool SetupPointLights(size_t count,
                          float intensity = 6.5f,
                          float orbitRadius = 5.5f,
                          float orbitInclMin = -XM_PIDIV4,
                          float orbitInclMax = XM_PIDIV4);
    bool SetupPointLights(const std::initializer_list<XMFLOAT4> &intensities,
                          float orbitRadius = 5.5f,
                          float orbitInclMin = -XM_PIDIV4,
                          float orbitInclMax = XM_PIDIV4);

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
    AmbientLight                mAmbientLight;
    std::vector<DirectLight>    mDirectLights;
    std::vector<PointLight>     mPointLights;

    // Camera
    struct {
        XMVECTOR eye;
        XMVECTOR at;
        XMVECTOR up;
    }                           mViewData;
    XMMATRIX                    mViewMtrx;
    XMMATRIX                    mProjectionMtrx;

    // Shaders

    ID3D11VertexShader*         mVertexShader = nullptr;
    ID3D11PixelShader*          mPsPbrMetalness = nullptr;
    ID3D11PixelShader*          mPsPbrSpecularity = nullptr;
    ID3D11PixelShader*          mPsConstEmmisive = nullptr;
    ID3D11InputLayout*          mVertexLayout = nullptr;

    ID3D11Buffer*               mCbScene = nullptr;
    ID3D11Buffer*               mCbFrame = nullptr;
    ID3D11Buffer*               mCbSceneNode = nullptr;
    ID3D11Buffer*               mCbScenePrimitive = nullptr;

    ID3D11SamplerState*         mSamplerLinear = nullptr;
};
