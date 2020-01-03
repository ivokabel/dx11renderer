#include "gltf_utils.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include "Libs/tinygltf-2.2.0/tiny_gltf.h"

#include "log.hpp"
#include "utils.hpp"

bool GltfUtils::LoadModel(tinygltf::Model &model, const std::wstring &filePath)
{
    using namespace std;

    // Convert to plain string for tinygltf
    string filePathA = Utils::WStringToString(filePath);

    // debug: tiny glTF test
    //{
    //    std::stringstream ss;
    //    cout_redirect cr(ss.rdbuf());
    //    TinyGltfTest(filePathA.c_str());
    //    Log::Debug(L"Gltf::LoadModel: TinyGltfTest output:\n\n%s", Utils::StringToWString(ss.str()).c_str());
    //}

    tinygltf::TinyGLTF tinyGltf;
    string errA, warnA;
    wstring ext = Utils::GetFilePathExt(filePath);

    bool ret = false;
    if (ext.compare(L"glb") == 0)
    {
        Log::Debug(L"Gltf::LoadModel: Reading binary glTF from \"%s\"", filePath.c_str());
        ret = tinyGltf.LoadBinaryFromFile(&model, &errA, &warnA, filePathA);
    }
    else
    {
        Log::Debug(L"Gltf::LoadModel: Reading ASCII glTF from \"%s\"", filePath.c_str());
        ret = tinyGltf.LoadASCIIFromFile(&model, &errA, &warnA, filePathA);
    }

    if (!errA.empty())
        Log::Debug(L"Gltf::LoadModel: Error: %s", Utils::StringToWString(errA).c_str());

    if (!warnA.empty())
        Log::Debug(L"Gltf::LoadModel: Warning: %s", Utils::StringToWString(warnA).c_str());

    if (ret)
        Log::Debug(L"Gltf::LoadModel: Succesfully loaded model");
    else
        Log::Error(L"Gltf::LoadModel: Failed to parse glTF file \"%s\"", filePath.c_str());

    return ret;
}


std::wstring GltfUtils::ModeToWString(int mode)
{
    if (mode == TINYGLTF_MODE_POINTS)
        return L"POINTS";
    else if (mode == TINYGLTF_MODE_LINE)
        return L"LINE";
    else if (mode == TINYGLTF_MODE_LINE_LOOP)
        return L"LINE_LOOP";
    else if (mode == TINYGLTF_MODE_TRIANGLES)
        return L"TRIANGLES";
    else if (mode == TINYGLTF_MODE_TRIANGLE_FAN)
        return L"TRIANGLE_FAN";
    else if (mode == TINYGLTF_MODE_TRIANGLE_STRIP)
        return L"TRIANGLE_STRIP";
    else
        return L"**UNKNOWN**";
}
