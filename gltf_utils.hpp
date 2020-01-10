#pragma once

#include "Libs/tinygltf-2.2.0/tiny_gltf.h" // just the interfaces (no implementation)

// We are using an older version of DirectX headers which causes 
// "warning C4005: '...' : macro redefinition"
#pragma warning(push)
#pragma warning(disable: 4005)
#include <d3d11.h>
#include <d3dx11.h>
#pragma warning(pop)

#include <string>

namespace GltfUtils
{
    bool LoadModel(tinygltf::Model &model, const std::wstring &filePath);

    //bool LoadFloat4Param(XMFLOAT4 &materialParam,
    //                     const char *paramName,
    //                     const tinygltf::ParameterMap &params,
    //                     const std::wstring &logPrefix);

    bool LoadFloatParam(float &materialParam,
                        const char *paramName,
                        const tinygltf::ParameterMap &params,
                        const std::wstring &logPrefix);

    D3D11_PRIMITIVE_TOPOLOGY ModeToTopology(int mode);

    std::wstring ModeToWstring(int mode);

    std::wstring StringIntMapToWstring(const std::map<std::string, int> &m);

    std::wstring TypeToWstring(int ty);

    std::wstring ComponentTypeToWstring(int ty);

    //size_t SizeOfComponentType(int ty);

    //std::wstring TargetToWstring(int target);

    std::wstring FloatArrayToWstring(const std::vector<double> &arr);

    std::wstring StringDoubleMapToWstring(const std::map<std::string, double> &mp);

    std::wstring ParameterValueToWstring(const tinygltf::Parameter &param);
}