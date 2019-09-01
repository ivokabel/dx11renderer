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

// debug: wstring->string conversion
#include <locale>
#include <codecvt>

// debug
#include "Libs/tinygltf-2.2.0/loader_example.h"

// debug: redirecting cout to string
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iostream>
struct cout_redirect {
    cout_redirect(std::streambuf * new_buffer)
        : old(std::cout.rdbuf(new_buffer))
    { }

    ~cout_redirect() {
        std::cout.rdbuf(old);
    }

private:
    std::streambuf * old;
};

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

    // debug: tiny glTF test
    {
        // convert to plain string
        using namespace std;
        wstring_convert<codecvt_utf8<wchar_t>, wchar_t> converter;
        string strCmdLine = converter.to_bytes(cmdLine);

        std::stringstream strstrm;
        cout_redirect cr(strstrm.rdbuf());
        TinyGltfTest(strCmdLine.c_str());
        Log::Debug(L"TinyGltfTest output:\n%s", converter.from_bytes(strstrm.str()).c_str());
        return 0;
    }

    auto scene = std::make_shared<Scene>(
        //Scene::eSimpleDebugSphere
        Scene::eEarth
        //Scene::eThreePlanets
        );
    SimpleDX11Renderer renderer(scene);

    if (!renderer.Init(instance, cmdShow, 1000u, 750u))
        return -1;

    return renderer.Run();
}
