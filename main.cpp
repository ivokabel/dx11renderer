#include "renderer.hpp"

#include <windows.h>


//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    //// debug
    //OutputDebugStringW(L"Hello world!\n");
    //OutputDebugString("Hello world!\n");

    SimpleDirectX11Renderer renderer;
    
    if (!renderer.Init(hInstance, nCmdShow, 640u, 480u))
        return -1;

    return renderer.Run();
}
