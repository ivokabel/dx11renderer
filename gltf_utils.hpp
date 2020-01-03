#pragma once

#include "Libs/tinygltf-2.2.0/tiny_gltf.h" // just the interfaces (no implementation)

#include <string>

namespace GltfUtils
{
    std::wstring GltfModeToWString(int mode);
}