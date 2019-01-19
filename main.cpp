#include "renderer.hpp"
#include "utils.hpp"
#include <windows.h>


//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow)
{
    // debug
    {
        Utils::Log(Utils::eDebug,   L"Debug print: \"%s\", %d", L"string", 123);
        Utils::Log(Utils::eWarning, L"Debug print: \"%s\", %d", L"string", 123);
        Utils::Log(Utils::eError,   L"Debug print: \"%s\", %d", L"string", 123);
    }

    UNREFERENCED_PARAMETER(instance);
    UNREFERENCED_PARAMETER(prevInstance);
    UNREFERENCED_PARAMETER(cmdLine);
    UNREFERENCED_PARAMETER(cmdShow);

    SimpleDX11Renderer renderer;
    
    if (!renderer.Init(instance, cmdShow, 640u, 480u))
        return -1;

    return renderer.Run();
}
