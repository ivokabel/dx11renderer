#pragma once

#include "Libs/tinygltf-2.5.0/tiny_gltf.h" // just the interfaces (no implementation)

// We are using an older version of DirectX headers which causes 
// "warning C4005: '...' : macro redefinition"
#pragma warning(push)
#pragma warning(disable: 4005)
#include <d3d11.h>
#include <d3dx11.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 4838)
#include <xnamath.h>
#pragma warning(pop)

#include <string>

namespace GltfUtils
{
    bool LoadModel(tinygltf::Model &model, const std::wstring &filePath);

    D3D11_PRIMITIVE_TOPOLOGY ModeToTopology(int mode);

    std::wstring ModeToWstring(int mode);

    std::wstring StringIntMapToWstring(const std::map<std::string, int> &m);

    std::wstring TypeToWstring(int ty);

    std::wstring ComponentTypeToWstring(int ty);

    //size_t SizeOfComponentType(int ty);

    //std::wstring TargetToWstring(int target);

    std::wstring ColorToWstring(const XMFLOAT4 &color);

    std::wstring FloatArrayToWstring(const std::vector<double> &arr);

    std::wstring StringDoubleMapToWstring(const std::map<std::string, double> &mp);

    std::wstring ParameterValueToWstring(const tinygltf::Parameter &param);

    bool FloatArrayToColor(XMFLOAT4 &color, const std::vector<double> &vector);

    template <int component>
    void FloatToColorComponent(XMFLOAT4 &color, double value)
    {
        static_assert(component < 4, "Incorrect color component index!");

        switch (component)
        {
        case 0: color.x = (float)value; break;
        case 1: color.y = (float)value; break;
        case 2: color.z = (float)value; break;
        case 3: color.w = (float)value; break;
        }
    }
}