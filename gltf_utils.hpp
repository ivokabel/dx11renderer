#pragma once

#include "Libs/tinygltf-2.2.0/tiny_gltf.h" // just the interfaces (no implementation)

#include <string>

namespace GltfUtils
{
    bool LoadModel(tinygltf::Model &model, const std::wstring &filePath);

    std::wstring ModeToWString(int mode);
}