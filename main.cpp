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
                bool startWithAnimationActive,
                double timeout = 0)
{
    auto scene = std::make_shared<Scene>(sceneId);

    SimpleDX11Renderer renderer(scene, startWithAnimationActive);

    if (!renderer.Init(instance, cmdShow, 1200u, 900u))
        return -1;

    if (timeout > 0.)
        renderer.SetTimeout(timeout); // For more deterministic measurements

    return renderer.Run();
}


//--------------------------------------------------------------------------------------
int RunSingleScene(Scene::SceneId sceneId,
                   HINSTANCE instance,
                   int cmdShow,
                   bool startWithAnimationActive,
                   double timeout = 0)
{
    return RunRenderer(instance, cmdShow, sceneId, startWithAnimationActive, timeout);
}


//--------------------------------------------------------------------------------------
int RunManyScenes(Scene::SceneId firstScene,
                  Scene::SceneId lastScene,
                  HINSTANCE instance,
                  int cmdShow,
                  bool startWithAnimationActive,
                  double timeout = 0)
{
    for (int sceneId = firstScene; sceneId <= lastScene; sceneId++)
    {
        Log::Debug(L"");
        Log::Debug(L"-------------------------------");
        Log::Debug(L"RunManyScenes: scene %d", sceneId);
        Log::Debug(L"-------------------------------");
        Log::Debug(L"");

        auto ret = RunRenderer(instance, cmdShow, (Scene::SceneId)sceneId, startWithAnimationActive, timeout);
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

    RunSingleScene(Scene::eGltfSampleDamagedHelmet, instance, cmdShow, false);
    RunSingleScene(Scene::eGltfSampleFlightHelmet, instance, cmdShow, false);
    RunSingleScene(Scene::eSpotMiniRigged, instance, cmdShow, false);
    RunSingleScene(Scene::eTheRocket, instance, cmdShow, false);
    RunSingleScene(Scene::eSalazarSkull, instance, cmdShow, false);
    return 0;

    //return RunManyScenes(Scene::eFirstSampleGltf, Scene::eLastSampleGltf, instance, cmdShow, false, 3.);
    //return RunManyScenes(Scene::eLowPolyDrifter, Scene::eHardhead, instance, cmdShow, false, 5.);
    //return RunManyScenes(Scene::eFirst, Scene::eLast, instance, cmdShow, true, 2.);
}
