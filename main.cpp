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
    Log::Debug(L"Entering WinMain: cmd \"%s\", current dir \"%s\"",
               cmdLine, buffer);

    auto scene = std::make_shared<Scene>();
    SimpleDX11Renderer renderer(scene);

    if (!renderer.Init(instance, cmdShow, 800u, 600u))
        return -1;

    return renderer.Run();
}
