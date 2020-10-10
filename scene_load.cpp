#include "scene.hpp"

#include "scene_utils.hpp"
#include "log.hpp"

bool Scene::Load(IRenderingContext &ctx)
{
    switch (mSceneId)
    {
    case eGltfSampleTriangleWithoutIndices:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/TriangleWithoutIndices/TriangleWithoutIndices.gltf"))
            return false;
        AddScaleToRoots(3.);
        break;
    }

    case eGltfSampleTriangle:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/Triangle/Triangle.gltf"))
            return false;
        AddScaleToRoots(4.5);
        AddTranslationToRoots({ 0., -1.5, 0. });
        break;
    }

    case eGltfSampleSimpleMeshes:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/SimpleMeshes/SimpleMeshes.gltf"))
            return false;
        AddScaleToRoots(2.5);
        AddTranslationToRoots({ 0., -0.5, 0. });
        break;
    }

    case eGltfSampleBox:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/Box/Box.gltf"))
            return false;
        AddScaleToRoots(4.);
        AddTranslationToRoots({ 0., 0., 0. });
        break;
    }

    case eGltfSampleBoxInterleaved:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/BoxInterleaved/BoxInterleaved.gltf"))
            return false;
        AddScaleToRoots(4.);
        AddTranslationToRoots({ 0., 0., 0. });
        break;
    }

    case eGltfSampleBoxTextured:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/BoxTextured/BoxTextured.gltf"))
            return false;
        AddScaleToRoots(4.);
        AddTranslationToRoots({ 0., 0., 0. });
        break;
    }

    case eGltfSampleMetalRoughSpheres:
    case eGltfSampleMetalRoughSpheresNoTextures:
    case eDebugMetalRoughSpheresNoTextures:
    {
        switch (mSceneId)
        {
        case eGltfSampleMetalRoughSpheres:
            if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/MetalRoughSpheres/MetalRoughSpheres.gltf"))
                return false;
            AddTranslationToRoots({ 0., 0.0, 1.6 });
            AddScaleToRoots(0.85);
            break;
        case eGltfSampleMetalRoughSpheresNoTextures:
            if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures.gltf"))
                return false;
            AddScaleToRoots(1000);
            AddTranslationToRoots({ -3.0, -3.0, 1.5 });
            break;
        case eDebugMetalRoughSpheresNoTextures:
            //if (!LoadExternal(ctx, L"../Scenes/Debugging/MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures.gltf"))
            //if (!LoadExternal(ctx, L"../Scenes/Debugging/MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures yellow.gltf"))
            if (!LoadExternal(ctx, L"../Scenes/Debugging/MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures brown.gltf"))
            //if (!LoadExternal(ctx, L"../Scenes/Debugging/MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures white.gltf"))
            //if (!LoadExternal(ctx, L"../Scenes/Debugging/MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures black.gltf"))
                return false;
            AddScaleToRoots(1000);
            AddTranslationToRoots({ -3.0, -3.0, 1.5 });
            break;
        default:
            return false;
        }

        AddRotationQuaternionToRoots({ 0.000, -1.000, 0.000, 0.000 }); // 180�y

        // Lights
//#define DIRECTIONAL
//#define POINT
//#define AMBIENT
        const size_t pointLightCount = 14;
#if defined DIRECTIONAL
        const uint8_t amb = 0;
        const float lum = 2.5f;
        const float ints = 0.f;
#elif defined POINT
        const uint8_t amb = 0;
        const float lum = 0.f;
        const float ints = 4000.f / pointLightCount;
#elif defined AMBIENT
        const uint8_t amb = 200u;
        const float lum = 0.f;
        const float ints = 0.f;
#else   // all lights
        const uint8_t amb = 80u;
        const float lum = 2.5f;
        const float ints = 4000.f / pointLightCount;
#endif
        mAmbientLight.luminance = SceneUtils::SrgbColorToFloat(amb, amb, amb);
        mDirectLights[0].dir = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);
        PointLight pointLight;
        pointLight.intensity = XMFLOAT4(ints, ints, ints, 1.0f);
        pointLight.orbitRadius = 20.f;
        pointLight.orbitInclinationMin = 0;
        pointLight.orbitInclinationMax = 0;
        mPointLights.clear();
        mPointLights.resize(pointLightCount, pointLight);

        // Camera pos
        mViewData.eye = XMVectorSet(0.0f, 0.0f, 10.f, 1.0f);
        mViewData.at  = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

        break;
    }

    case eGltfSampleNormalTangentTest:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/NormalTangentTest/glTF/NormalTangentTest.gltf"))
            return false;
        AddScaleToRoots(3.6f);
        AddRotationQuaternionToRoots({ -0.174, 0.000, 0.000,  0.985 }); // -20�x
        break;
    }

    case eGltfSampleNormalTangentMirrorTest:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/NormalTangentMirrorTest/glTF/NormalTangentMirrorTest.gltf"))
            return false;
        AddScaleToRoots(3.6f);
        AddRotationQuaternionToRoots({ -0.174, 0.000, 0.000,  0.985 }); // -20�x
        break;
    }

    case eGltfSample2CylinderEngine:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/2CylinderEngine/2CylinderEngine.gltf"))
            return false;
        AddScaleToRoots(0.012f);
        AddTranslationToRoots({ 0., 0.2, 0. });

        for (auto &light : mPointLights)
            light.orbitRadius = 6.0f;
        break;
    }

    case eGltfSampleDuck:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/Duck/Duck.gltf"))
            return false;
        AddScaleToRoots(3.8);
        AddTranslationToRoots({ -0.5, -3.3, 0. });
        break;
    }

    case eGltfSampleBoomBox:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/BoomBox/BoomBox.gltf"))
            return false;
        AddScaleToRoots(330.);
        AddTranslationToRoots({ 0., 0., 0. });
        break;
    }

    case eGltfSampleBoomBoxWithAxes:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/BoomBoxWithAxes/BoomBoxWithAxes.gltf"))
            return false;
        AddScaleToRoots(230.);
        AddTranslationToRoots({ 0., -2.2, 0. });
        break;
    }

    case eGltfSampleDamagedHelmet:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/DamagedHelmet/DamagedHelmet.gltf"))
            return false;

        AddScaleToRoots(3.7);
        AddTranslationToRoots({ 0., 0.35, 0.3 });
        //AddRotationQuaternionToRoots({ 0.000, 0.131, 0.000, 0.991 }); // 15�y
        //AddRotationQuaternionToRoots({ 0.000, 0.259, 0.000, 0.966 }); // 30�y
        //AddRotationQuaternionToRoots({ 0.000, 0.383, 0.000, 0.924 }); // 45�y
        //AddRotationQuaternionToRoots({ 0.000, 0.500, 0.000, 0.866 }); // 60�y
        //AddRotationQuaternionToRoots({ 0.000, 0.609, 0.000, 0.793 }); // 75�y
        AddRotationQuaternionToRoots({ 0.000, 0.643, 0.000, 0.766 }); // 80�y
        //AddRotationQuaternionToRoots({ 0.000, 0.676, 0.000, 0.737 }); // 85�y
        //AddRotationQuaternionToRoots({ 0.000, 0.707, 0.000, 0.707 }); // 90�y

        const uint8_t amb = 40;
        mAmbientLight.luminance = SceneUtils::SrgbColorToFloat(amb, amb, amb);
        const float lum = 2.0f;
        mDirectLights[0].dir = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);
        const float ints = 12.0f;
        mPointLights.resize(5);
        for (auto &light : mPointLights)
        {
            light.intensity = XMFLOAT4(ints, ints, ints, 1.0f);
            light.orbitRadius = 6.5f;
            light.orbitInclinationMin = 0;
            light.orbitInclinationMax = 0;
        }

        break;
    }

    case eGltfSampleFlightHelmet:
    {
        if (!LoadExternal(ctx, L"../Scenes/glTF-Sample-Models/FlightHelmet/FlightHelmet.gltf"))
            return false;

        AddScaleToRoots(11.0);
        AddTranslationToRoots({ 0., 1.2, 0. });
        AddRotationQuaternionToRoots({ 0.000, 0.259, 0.000, 0.966 }); // 30�y

        const uint8_t amb = 40;
        mAmbientLight.luminance = SceneUtils::SrgbColorToFloat(amb, amb, amb);
        const float lum = 4.0f;
        mDirectLights[0].dir = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);
        const float ints = 9.0f;
        mPointLights.resize(5);
        for (auto &light : mPointLights)
        {
            light.intensity = XMFLOAT4(ints, ints, ints, 1.0f);
            //light.orbitRadius = 6.5f;
            light.orbitInclinationMin = 0;
            light.orbitInclinationMax = 0;
        }
        break;
    }

    case eLowPolyDrifter:
    {
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/Ivan Norman - Low-poly truck (car Drifter)/scene.gltf"))
            return false;
        AddScaleToRoots(0.015);
        AddTranslationToRoots({ 0.5, -1.2, 0. });
        break;
    }

    case eWolfBaseMesh:
    {
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/TheCharliEZM - Wolf base mesh/scene.gltf"))
            return false;
        AddScaleToRoots(0.008);
        AddTranslationToRoots({ 0.7, -2.4, 0. });
        break;
    }

    case eNintendoGameBoyClassic:
    {
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/hunter333d - Nintendo Game Boy Classic/scene.gltf"))
            return false;
        AddScaleToRoots(0.50);
        AddTranslationToRoots({ 0., -1.7, 0. });
        break;
    }

    case eWeltron2001SpaceballRadio:
    {
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/ArneDC - Prinzsound SM8 - Weltron 2001 Spaceball Radio/scene.gltf"))
            return false;
        AddScaleToRoots(.016);
        AddTranslationToRoots({ 0., -3.6, 0. });
        AddRotationQuaternionToRoots({ 0.000, 0.131, 0.000, 0.991 }); // 15�y
        break;
    }

    case eSpotMiniRigged:
    {
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/Greg McKechnie - Spot Mini (Rigged)/scene.gltf"))
            return false;
        AddScaleToRoots(.00028f);
        AddTranslationToRoots({ 0., 0., 2.8 });
        break;
    }

    case eMandaloriansHelmet:
    {
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/arn204 - The Mandalorian's Helmet/scene.gltf"))
            return false;

        AddTranslationToRoots({ -35., -70., 85. });
        AddScaleToRoots(.17f);
        //AddRotationQuaternionToRoots({ 0.000, 0.996, 0.000, 0.087 }); // 170�y
        AddRotationQuaternionToRoots({ 0.000, 0.999, 0.000,  0.044 }); // 175�y
        //AddRotationQuaternionToRoots({ 0.000, 1.000, 0.000, 0.000 }); // 180�y
        //AddRotationQuaternionToRoots({ 0.000,  0.996, 0.000, -0.087 }); // 190�y

        const uint8_t amb = 30;
        mAmbientLight.luminance = SceneUtils::SrgbColorToFloat(amb, amb, amb);
        const float lum = 1.8f;
        mDirectLights[0].dir = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);
        const float ints = 7.0f;
        for (auto &light : mPointLights)
            light.intensity = XMFLOAT4(ints, ints, ints, 1.0f);

        break;
    }

    case eTheRocket:
    {
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/TuppsM - The Rocket/scene.gltf"))
            return false;
        AddScaleToRoots(.012);
        AddTranslationToRoots({ -0.1, -1., 0. });
        break;
    }

    case eRoboV1:
    {
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/_Sef_ - Robo_V1/scene.gltf"))
            return false;
        AddTranslationToRoots({ 0., -100., -240. });
        AddScaleToRoots(.10);
        break;
    }

    case eRockJacket:
    {
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/Teliri - Rock Jacket (mid-poly)/scene.gltf"))
            return false;

        AddScaleToRoots(2.2);
        AddTranslationToRoots({ 0., -3.2, 0. });
        AddRotationQuaternionToRoots({ 0.000, 0.259, 0.000, 0.966 }); // 30�y

        const uint8_t amb = 30;
        mAmbientLight.luminance = SceneUtils::SrgbColorToFloat(amb, amb, amb);

        //const float lum = 0.5f;
        //mDirectLights.resize(1);
        //mDirectLights[0].dir = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        //mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);

        const float ints = 14.0f;
        mPointLights.resize(5);
        for (auto &light : mPointLights)
        {
            light.intensity = XMFLOAT4(ints, ints, ints, 1.0f);
            light.orbitRadius = 4.63f;
            light.orbitInclinationMin = 0;
            light.orbitInclinationMax = 0;
        }

        break;
    }

    case eSalazarSkull:
    {
        if (!LoadExternal(ctx, L"../Scenes/Sketchfab/jvitorsouzadesign - Skull Salazar/scene.gltf"))
            return false;

        AddScaleToRoots(4.1);
        AddTranslationToRoots({ 0., 0.7, 0. });
        AddRotationQuaternionToRoots({ 0.000, 0.383, 0.000, 0.924 }); // 45�y

        const float amb = 0.03f;
        mAmbientLight.luminance = XMFLOAT4(amb, amb, amb, 1.0f);

        const float lum = 0.5f;
        mDirectLights.resize(1);
        mDirectLights[0].dir = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);

        const float ints = 14.0f;
        mPointLights.resize(3);
        for (auto &light : mPointLights)
        {
            light.intensity = XMFLOAT4(ints, ints, ints, 1.0f);
            light.orbitInclinationMin = 0;
            light.orbitInclinationMax = 0;
        }

        break;
    }

    case eHardwiredSimpleDebugSphere:
    {
        mMaterials.clear();
        mMaterials.resize(1, SceneMaterial());
        if (mMaterials.size() != 1)
            return false;

        auto &material0 = mMaterials[0];
        if (!material0.CreatePbrSpecularity(ctx,

                                            // Diffuse:
                                            L"../Textures/vfx_debug_textures by Chris Judkins/debug_color_02.png",
                                            //L"../Textures/vfx_debug_textures by Chris Judkins/debug_orientation_01.png",
                                            //L"../Textures/vfx_debug_textures by Chris Judkins/debug_offset_01.png",
                                            //L"../Textures/vfx_debug_textures by Chris Judkins/debug_uv_02.png",
                                            //L"../Textures/Debugging/VerticalSineWaves8.png",
                                            //nullptr,
                                            //XMFLOAT4(0.f, 0.f, 0.f, 1.f),
                                            //XMFLOAT4(0.9f, 0.9f, 0.9f, 1.f),
                                            //XMFLOAT4(0.9f, 0.45f, 0.135f, 1.f),
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f),

                                            // Specular:
                                            nullptr,
                                            XMFLOAT4(0.f, 0.f, 0.f, 1.f)
                                            //XMFLOAT4(0.1f, 0.1f, 0.1f, 1.f)
                                            //XMFLOAT4(1.f, 1.f, 1.f, 1.f)
                                            ))
            return false;

        mRootNodes.clear();
        mRootNodes.resize(1, SceneNode(true));
        if (mRootNodes.size() != 1)
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive = node0.CreateEmptyPrimitive();
        if (!primitive)
            return false;

        if (!primitive->CreateSphere(ctx, 50, 100))
            return false;
        primitive->SetMaterialIdx(0);
        node0.AddScale({ 3.4f, 3.4f, 3.4f });
        node0.AddRotationQuaternion({ 0.000, 0.707, 0.000, 0.707 }); // 90�y

//#define USE_PURE_AMBIENT_LIGHT
//#define USE_PURE_DIRECTIONAL_LIGHT
//#define USE_PURE_POINT_LIGHT
        mDirectLights.resize(1);
        mPointLights.resize(3);
#ifdef USE_PURE_AMBIENT_LIGHT
        const float amb = 0.9f;
        mAmbientLight.luminance     = XMFLOAT4(amb, amb, amb, 1.0f);

        const float lum = 0.f;
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        const float ints = 0.f;
        for (auto &light : mPointLights)
            light.intensity = XMFLOAT4(ints, ints, ints, 1.0f);
#elif defined USE_PURE_DIRECTIONAL_LIGHT
        const float amb = 0.f;
        mAmbientLight.luminance     = XMFLOAT4(amb, amb, amb, 1.0f);

        const float lum = 3.141f;
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        const float ints = 0.f;
        for (auto &light : mPointLights)
            light.intensity = XMFLOAT4(ints, ints, ints, 1.0f);
#elif defined USE_PURE_POINT_LIGHT
        const float amb = 0.f;
        mAmbientLight.luminance     = XMFLOAT4(amb, amb, amb, 1.0f);

        const float lum = 0.f;
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        const float ints = 3.141f;
        for (auto &light : mPointLights)
            light.intensity = XMFLOAT4(ints, ints, ints, 1.0f);
#else
        mAmbientLight.luminance     = SceneUtils::SrgbColorToFloat(30, 30, 30);

        const float lum = 2.6f;
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        // coloured point lights
        mPointLights[0].intensity   = XMFLOAT4(4.0f, 1.8f, 1.2f, 1.0f); // red
        mPointLights[1].intensity   = XMFLOAT4(1.0f, 2.5f, 1.1f, 1.0f); // green
        mPointLights[2].intensity   = XMFLOAT4(1.2f, 1.8f, 4.0f, 1.0f); // blue
#endif

        break;
    }

    case eHardwiredPbrMetalnesDebugSphere:
    {
        mMaterials.clear();
        mMaterials.resize(1, SceneMaterial());
        if (mMaterials.size() != 1)
            return false;

        auto &material0 = mMaterials[0];
        const XMFLOAT4 baseColorConstFactor(0.8f, 0.8f, 0.8f, 1.f);
        const float    metallicConstFactor  = 1.0f;
        const float    roughnessConstFactor =
                            //0.000f;
                            //0.001f;
                            //0.010f;
                            0.100f;
                            //0.200f;
                            //0.400f;
                            //0.800f;
                            //1.000f;
        if (!material0.CreatePbrMetalness(ctx,
                                          nullptr,
                                          baseColorConstFactor,
                                          nullptr,
                                          metallicConstFactor,
                                          roughnessConstFactor))
            return false;

        mRootNodes.clear();
        mRootNodes.resize(1, SceneNode(true));
        if (mRootNodes.size() != 1)
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive = node0.CreateEmptyPrimitive();
        if (!primitive)
            return false;

        if (!primitive->CreateSphere(ctx, 20, 40))//40, 80))
            return false;
        primitive->SetMaterialIdx(0);
        node0.AddScale(3.9f);
        node0.AddTranslation({ 0.f, -0.2f, 0.f });

        const float amb = 0.6f;
        mAmbientLight.luminance     = XMFLOAT4(amb, amb, amb, 1.0f);

        const float lum = 3.f;
        mDirectLights.resize(1);
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        // coloured point lights
        const float ints = 3.f;
        mPointLights.resize(3);
        //mPointLights[0].intensity   = XMFLOAT4(4.0f * ints, 0.0f * ints, 0.0f * ints, 1.0f); // red
        //mPointLights[1].intensity   = XMFLOAT4(0.0f * ints, 2.5f * ints, 0.0f * ints, 1.0f); // green
        //mPointLights[2].intensity   = XMFLOAT4(0.0f * ints, 0.0f * ints, 4.0f * ints, 1.0f); // blue
        mPointLights[0].intensity   = XMFLOAT4(0.5f * ints, 0.5f * ints, 0.5f * ints, 1.0f);
        mPointLights[1].intensity   = XMFLOAT4(0.5f * ints, 0.5f * ints, 0.5f * ints, 1.0f);
        mPointLights[2].intensity   = XMFLOAT4(0.5f * ints, 0.5f * ints, 0.5f * ints, 1.0f);
        const float pointScale = 10.f;
        for (int i = 0; i < 3; ++i)
        {
            mPointLights[i].intensity.x *= pointScale;
            mPointLights[i].intensity.y *= pointScale;
            mPointLights[i].intensity.z *= pointScale;
        }

        break;
    }

    case eHardwiredEarth:
    {
        mMaterials.clear();
        mMaterials.resize(1, SceneMaterial());
        if (mMaterials.size() != 1)
            return false;

        auto &material0 = mMaterials[0];
        if (!material0.CreatePbrSpecularity(ctx,
                                            L"../Textures/www.solarsystemscope.com/2k_earth_daymap.jpg",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f),
                                            L"../Textures/www.solarsystemscope.com/2k_earth_specular_map.tif",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f)))
            return false;

        mRootNodes.clear();
        mRootNodes.resize(1, SceneNode(true));
        if (mRootNodes.size() != 1)
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive = node0.CreateEmptyPrimitive();
        if (!primitive)
            return false;

        if (!primitive->CreateSphere(ctx, 40, 80))
            return false;
        primitive->SetMaterialIdx(0);
        node0.AddScale({ 3.4f, 3.4f, 3.4f });

        mAmbientLight.luminance     = XMFLOAT4(0.f, 0.f, 0.f, 1.0f);

        const float lum = 3.5f;
        mDirectLights.resize(1);
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);

        const float ints = 3.5f;
        mPointLights.resize(3);
        mPointLights[0].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);
        mPointLights[1].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);
        mPointLights[2].intensity   = XMFLOAT4(ints, ints, ints, 1.0f);

        break;
    }

    case eHardwiredThreePlanets:
    {
        mMaterials.clear();
        mMaterials.resize(3, SceneMaterial());
        if (mMaterials.size() != 3)
            return false;

        mRootNodes.clear();
        mRootNodes.resize(3, SceneNode(true));
        if (mRootNodes.size() != 3)
            return false;

        auto &material0 = mMaterials[0];
        if (!material0.CreatePbrSpecularity(ctx,
                                            L"../Textures/www.solarsystemscope.com/2k_earth_daymap.jpg",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f),
                                            L"../Textures/www.solarsystemscope.com/2k_earth_specular_map.tif",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f)))
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive0 = node0.CreateEmptyPrimitive();
        if (!primitive0)
            return false;
        if (!primitive0->CreateSphere(ctx, 40, 80))
            return false;
        primitive0->SetMaterialIdx(0);
        node0.AddScale({ 2.2f, 2.2f, 2.2f });
        node0.AddTranslation({ 0.f, 0.f, -1.5f });

        auto &material1 = mMaterials[1];
        if (!material1.CreatePbrSpecularity(ctx, L"../Textures/www.solarsystemscope.com/2k_mars.jpg",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f),
                                            nullptr,
                                            XMFLOAT4(0.f, 0.f, 0.f, 1.f)))
            return false;

        auto &node1 = mRootNodes[1];
        auto primitive1 = node1.CreateEmptyPrimitive();
        if (!primitive1)
            return false;
        if (!primitive1->CreateSphere(ctx, 20, 40))
            return false;
        primitive1->SetMaterialIdx(1);
        node1.AddScale({ 1.2f, 1.2f, 1.2f });
        node1.AddTranslation({ -2.5f, 0.f, 2.0f });

        auto &material2 = mMaterials[2];
        if (!material2.CreatePbrSpecularity(ctx, L"../Textures/www.solarsystemscope.com/2k_jupiter.jpg",
                                            XMFLOAT4(1.f, 1.f, 1.f, 1.f),
                                            nullptr,
                                            XMFLOAT4(0.f, 0.f, 0.f, 1.f)))
            return false;

        auto &node2 = mRootNodes[2];
        auto primitive2 = node2.CreateEmptyPrimitive();
        if (!primitive2)
            return false;
        if (!primitive2->CreateSphere(ctx, 20, 40))
            return false;
        primitive2->SetMaterialIdx(2);
        node2.AddScale({ 1.2f, 1.2f, 1.2f });
        node2.AddTranslation({ 2.5f, 0.f, 2.0f });

        const float amb = 0.f;
        mAmbientLight.luminance     = XMFLOAT4(amb, amb, amb, 1.0f);
        const float lum = 3.0f;
        mDirectLights.resize(1);
        mDirectLights[0].dir       = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);
        const float ints = 4.0f;
        mPointLights.resize(3);
        for (auto &light : mPointLights)
        {
            light.intensity = XMFLOAT4(ints, ints, ints, 1.0f);
            light.orbitRadius = 5.3f;
            light.orbitInclinationMin = -XM_PI / 8;
            light.orbitInclinationMax =  XM_PI / 8;
        }

        break;
    }

    case eHardwiredMaterialConstFactors:
    {
        mMaterials.clear();
        mMaterials.resize(3, SceneMaterial());
        if (mMaterials.size() != 3)
            return false;

        mRootNodes.clear();
        mRootNodes.resize(3, SceneNode(true));
        if (mRootNodes.size() != 3)
            return false;

        const wchar_t *baseColorTex   = L"../Textures/Debugging/VerticalSineWaves8.png";
        const wchar_t *baseColorNoTex = nullptr;
        const XMFLOAT4 baseColorConstFactor   = XMFLOAT4(1.0f, 0.7f, 0.1f, 1.0f);
        const XMFLOAT4 baseColorNoConstFactor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        const wchar_t *metallicRoughnessTex = nullptr;
        const float metallicConstFactor = 0.f;
        const float roughnessConstFactor = 0.4f;

        // Sphere 0

        auto &material0 = mMaterials[0];
        if (!material0.CreatePbrMetalness(ctx,
                                          baseColorTex,
                                          baseColorConstFactor,
                                          metallicRoughnessTex,
                                          metallicConstFactor,
                                          roughnessConstFactor))
            return false;

        auto &node0 = mRootNodes[0];
        auto primitive0 = node0.CreateEmptyPrimitive();
        if (!primitive0)
            return false;
        if (!primitive0->CreateSphere(ctx, 40, 80))
            return false;
        primitive0->SetMaterialIdx(0);
        node0.AddScale({ 1.7f, 1.7f, 1.7f });
        node0.AddTranslation({ 0.f, 0.f, 0.f });

        // Sphere 1

        auto &material1 = mMaterials[1];
        if (!material1.CreatePbrMetalness(ctx,
                                          baseColorTex,
                                          baseColorNoConstFactor,
                                          metallicRoughnessTex,
                                          metallicConstFactor,
                                          roughnessConstFactor))
            return false;

        auto &node1 = mRootNodes[1];
        auto primitive1 = node1.CreateEmptyPrimitive();
        if (!primitive1)
            return false;
        if (!primitive1->CreateSphere(ctx, 40, 80))
            return false;
        primitive1->SetMaterialIdx(1);
        node1.AddScale({ 1.7f, 1.7f, 1.7f });
        node1.AddTranslation({ -3.6f, 0.f, 0.f });

        // Sphere 2

        auto &material2 = mMaterials[2];
        if (!material2.CreatePbrMetalness(ctx,
                                          baseColorNoTex,
                                          baseColorConstFactor,
                                          metallicRoughnessTex,
                                          metallicConstFactor,
                                          roughnessConstFactor))
            return false;

        auto &node2 = mRootNodes[2];
        auto primitive2 = node2.CreateEmptyPrimitive();
        if (!primitive2)
            return false;
        if (!primitive2->CreateSphere(ctx, 40, 80))
            return false;
        primitive2->SetMaterialIdx(2);
        node2.AddScale({ 1.7f, 1.7f, 1.7f });
        node2.AddTranslation({ 3.6f, 0.f, 0.f });

        const float amb = 0.f;
        mAmbientLight.luminance     = XMFLOAT4(amb, amb, amb, 1.0f);
        const float lum = 3.0f;
        mDirectLights.resize(1);
        mDirectLights[0].dir        = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance  = XMFLOAT4(lum, lum, lum, 1.0f);
        const float ints = 4.0f;
        PointLight pointLight;
        pointLight.intensity = XMFLOAT4(ints, ints, ints, 1.0f);
        pointLight.orbitRadius = 6.5f;
        mPointLights.resize(3, pointLight);

        break;
    }

    case  eDebugGradientBox:
    {
        if (!LoadExternal(ctx, L"../Scenes/Debugging/GradientBox/GradientBox.gltf"))
            return false;
        //AddScaleToRoots({ 4., 1., 1. });
        AddScaleToRoots(6.);
        AddTranslationToRoots({ 0., .2, 0. });
        //AddRotationQuaternionToRoots({ 0.980, 0., 0., 0.197 }); //22.7�
        AddRotationQuaternionToRoots({ 0.980, 0., 0., 0.198 }); //22.8�

        const float amb = 0.f;//1.f;//
        mAmbientLight.luminance = XMFLOAT4(amb, amb, amb, 1.0f);
        const float lum = 8.f;//0.f;//
        mDirectLights.resize(1);
        mDirectLights[0].dir = XMFLOAT4(0.f, 1.f, 0.f, 1.0f);
        mDirectLights[0].luminance = XMFLOAT4(lum, lum, lum, 1.0f);
        const float ints = 0.f;//3000.f;//
        mPointLights.resize(3);
        for (auto &light : mPointLights)
            light.intensity = XMFLOAT4(ints, ints, ints, 1.0f);

        break;
    }

    default:
        return false;
    }

    return PostLoadSanityTest();
}


bool Scene::PostLoadSanityTest()
{
    if (mPointLights.size() > POINT_LIGHTS_MAX_COUNT)
    {
        Log::Error(L"Point lights count (%d) exceeded maximum limit (%d)!",
                   mPointLights.size(), POINT_LIGHTS_MAX_COUNT);
        return false;
    }

    if (mDirectLights.size() > DIRECT_LIGHTS_MAX_COUNT)
    {
        Log::Error(L"Directional lights count (%d) exceeded maximum limit (%d)!",
                   mDirectLights.size(), DIRECT_LIGHTS_MAX_COUNT);
        return false;
    }

    // Geometry using normal map must have tangent specified (for now)
    for (auto &node : mRootNodes)
        if (!NodeTangentSanityTest(node))
            return false;

    return true;
}


bool Scene::NodeTangentSanityTest(const SceneNode &node)
{
    // Test node
    for (auto &primitive : node.mPrimitives)
    {
        const SceneMaterial &material = [&]()
        {
            const int matIdx = primitive.GetMaterialIdx();
            if (matIdx >= 0 && matIdx < mMaterials.size())
                return mMaterials[matIdx];
            else
                return mDefaultMaterial;
        }();

        if (material.GetNormalTexture().IsLoaded() && !primitive.IsTangentPresent())
        {
            Log::Error(L"A scene primitive without tangent data uses a material with normal map!");
            return false;
        }
    }

    // Children
    for (auto &child : node.mChildren)
        if (!NodeTangentSanityTest(child))
            return false;

    return true;
}
