#include "renderer.hpp"
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
    Utils::Log(Utils::eDebug,
                L"Entering WinMain: cmd \"%s\", current dir \"%s\"",
                cmdLine, buffer);

    SimpleDX11Renderer renderer;

    if (!renderer.Init(instance, cmdShow, 640u, 480u))
        return -1;

    return renderer.Run();
}
