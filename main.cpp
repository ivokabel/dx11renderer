// Visual studio 2017 (15.8.0 preview 3.0) introduced and ABI-breaking bugfix for make_shared and 
// requires us to decide wether we want the the old non-conforming behaviour or the correct one
// until the next ABI breaking release of the libraries.
// See: https://developercommunity.visualstudio.com/content/problem/274945/stdmake-shared-is-not-honouring-alignment-of-a.html
#define _ENABLE_EXTENDED_ALIGNED_STORAGE

#include "renderer.hpp"
#include "scene.hpp"
#include "log.hpp"
#include "utils.hpp"
#include <windows.h>
#include <array>


//--------------------------------------------------------------------------------------
int RunRenderer(HINSTANCE instance,
                int cmdShow,
                Scene::SceneId sceneId,
                double timeout = 0)
{
    auto scene = std::make_shared<Scene>(sceneId);

    SimpleDX11Renderer renderer(scene);

    if (!renderer.Init(instance, cmdShow, 1200u, 900u))
        return -1;

    if (timeout > 0.)
        renderer.SetTimeout(timeout); // For more deterministic measurements

    return renderer.Run();
}


//--------------------------------------------------------------------------------------
int RunSingleScene(HINSTANCE instance, int cmdShow)
{
    Scene::SceneId sceneId =
        //Scene::eHardwiredSimpleDebugSphere
        //Scene::eHardwiredMaterialConstFactors
        //Scene::eHardwiredPbrMetalnesDebugSphere
        //Scene::eHardwiredEarth
        //Scene::eHardwiredThreePlanets

        //Scene::eDebugGradientBox
        //Scene::eDebugMetalRoughSpheresNoTextures

        //Scene::eGltfSampleTriangleWithoutIndices // Non-indexed geometry not yet supported!
        //Scene::eGltfSampleTriangle
        //Scene::eGltfSampleSimpleMeshes
        //Scene::eGltfSampleBox
        //Scene::eGltfSampleBoxInterleaved
        //Scene::eGltfSampleBoxTextured
        //Scene::eGltfSampleMetalRoughSpheres
        //Scene::eGltfSampleMetalRoughSpheresNoTextures
        //Scene::eGltfSampleNormalTangentTest
        Scene::eGltfSampleNormalTangentMirrorTest
        //Scene::eGltfSample2CylinderEngine
        //Scene::eGltfSampleDuck
        //Scene::eGltfSampleBoomBox
        //Scene::eGltfSampleBoomBoxWithAxes
        //Scene::eGltfSampleDamagedHelmet
        //Scene::eGltfSampleFlightHelmet

        //Scene::eWolfBaseMesh
        //Scene::eNintendoGameBoyClassic
        //Scene::eLowPolyDrifter
        //Scene::eSpotMiniRigged
        //Scene::eMandaloriansHelmet
        //Scene::eWeltron2001SpaceballRadio
        //Scene::eTheRocket
        //Scene::eRoboV1
        //Scene::eRockJacket
        //Scene::eSalazarSkull
        ;

    return RunRenderer(instance, cmdShow, sceneId);
}


//--------------------------------------------------------------------------------------
int RunAllScenes(HINSTANCE instance, int cmdShow, double timeout = 0)
{
    for (int sceneId = Scene::SceneId::eFirst;
         sceneId <= Scene::SceneId::eLast;
         sceneId++)
    {
        Log::Debug(L"");
        Log::Debug(L"-------------------------------");
        Log::Debug(L"RunAllScenes: scene %d", sceneId);
        Log::Debug(L"-------------------------------");
        Log::Debug(L"");

        auto ret = RunRenderer(instance, cmdShow, (Scene::SceneId)sceneId, timeout);
        if (ret != 0)
            return ret;
    }

    return 0;
}


//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE prevInstance,
                    LPWSTR cmdLine,
                    int cmdShow)
{
    UNREFERENCED_PARAMETER(instance);
    UNREFERENCED_PARAMETER(prevInstance);
    UNREFERENCED_PARAMETER(cmdLine);
    UNREFERENCED_PARAMETER(cmdShow);

    wchar_t buffer[1024] = {};
    GetCurrentDirectory(1024, buffer);
    Log::Debug(L"WinMain: %s config, cmd \"%s\", current dir \"%s\"",
               Utils::ConfigName(), cmdLine, buffer);

    return RunSingleScene(instance, cmdShow);
    //return RunAllScenes(instance, cmdShow, 2.);
}
