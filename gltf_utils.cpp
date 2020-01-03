#include "gltf_utils.hpp"

std::wstring GltfUtils::GltfModeToWString(int mode)
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
