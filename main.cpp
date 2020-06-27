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
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow)
{
    UNREFERENCED_PARAMETER(instance);
    UNREFERENCED_PARAMETER(prevInstance);
    UNREFERENCED_PARAMETER(cmdLine);
    UNREFERENCED_PARAMETER(cmdShow);

    wchar_t buffer[1024] = {};
    GetCurrentDirectory(1024, buffer);
    Log::Debug(L"Entering WinMain: %s config, cmd \"%s\", current dir \"%s\"",
               Utils::ConfigName(), cmdLine, buffer);

    auto scene = std::make_shared<Scene>(
        //Scene::eHardwiredSimpleDebugSphere
        //Scene::eHardwiredMaterialConstFactors
        //Scene::eHardwiredPbrMetalnesDebugSphere
        //Scene::eHardwiredEarth
        //Scene::eHardwiredThreePlanets

        //Scene::eDebugGradientBox
        Scene::eDebugMetalRoughSpheresNoTextures

        //Scene::eGltfSampleTriangleWithoutIndices // Non-indexed geometry not yet supported!
        //Scene::eGltfSampleTriangle
        //Scene::eGltfSampleSimpleMeshes
        //Scene::eGltfSampleBox
        //Scene::eGltfSampleBoxInterleaved
        //Scene::eGltfSampleBoxTextured
        //Scene::eGltfSampleMetalRoughSpheres
        //Scene::eGltfSampleMetalRoughSpheresNoTextures
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
        );
    SimpleDX11Renderer renderer(scene);

    if (!renderer.Init(instance, cmdShow, 1000u, 750u))
        return -1;

    renderer.SetTimeout(15.);

    return renderer.Run();
}
