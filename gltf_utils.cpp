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
    string filePathA = Utils::WstringToString(filePath);

    // debug: tiny glTF test
    //{
    //    std::stringstream ss;
    //    cout_redirect cr(ss.rdbuf());
    //    TinyGltfTest(filePathA.c_str());
    //    Log::Debug(L"Gltf::LoadModel: TinyGltfTest output:\n\n%s", Utils::StringToWstring(ss.str()).c_str());
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
        Log::Debug(L"Gltf::LoadModel: Error: %s", Utils::StringToWstring(errA).c_str());

    if (!warnA.empty())
        Log::Debug(L"Gltf::LoadModel: Warning: %s", Utils::StringToWstring(warnA).c_str());

    if (ret)
        Log::Debug(L"Gltf::LoadModel: Succesfully loaded model");
    else
        Log::Error(L"Gltf::LoadModel: Failed to parse glTF file \"%s\"", filePath.c_str());

    return ret;
}


D3D11_PRIMITIVE_TOPOLOGY GltfUtils::ModeToTopology(int mode)
{
    switch (mode)
    {
    case TINYGLTF_MODE_POINTS:
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    case TINYGLTF_MODE_LINE:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case TINYGLTF_MODE_LINE_STRIP:
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case TINYGLTF_MODE_TRIANGLES:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case TINYGLTF_MODE_TRIANGLE_STRIP:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        //case TINYGLTF_MODE_LINE_LOOP:
        //case TINYGLTF_MODE_TRIANGLE_FAN:
    default:
        return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}


std::wstring GltfUtils::ModeToWstring(int mode)
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


std::wstring GltfUtils::StringIntMapToWstring(const std::map<std::string, int> &m)
{
    std::stringstream ss;
    bool first = true;
    for (auto item : m)
    {
        if (!first)
            ss << ", ";
        else
            first = false;
        ss << item.first << ": " << item.second;
    }
    return Utils::StringToWstring(ss.str());
}


std::wstring GltfUtils::TypeToWstring(int ty)
{
    if (ty == TINYGLTF_TYPE_SCALAR)
        return L"SCALAR";
    else if (ty == TINYGLTF_TYPE_VECTOR)
        return L"VECTOR";
    else if (ty == TINYGLTF_TYPE_VEC2)
        return L"VEC2";
    else if (ty == TINYGLTF_TYPE_VEC3)
        return L"VEC3";
    else if (ty == TINYGLTF_TYPE_VEC4)
        return L"VEC4";
    else if (ty == TINYGLTF_TYPE_MATRIX)
        return L"MATRIX";
    else if (ty == TINYGLTF_TYPE_MAT2)
        return L"MAT2";
    else if (ty == TINYGLTF_TYPE_MAT3)
        return L"MAT3";
    else if (ty == TINYGLTF_TYPE_MAT4)
        return L"MAT4";
    return L"**UNKNOWN**";
}


std::wstring GltfUtils::ComponentTypeToWstring(int ty)
{
    if (ty == TINYGLTF_COMPONENT_TYPE_BYTE)
        return L"BYTE";
    else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
        return L"UNSIGNED_BYTE";
    else if (ty == TINYGLTF_COMPONENT_TYPE_SHORT)
        return L"SHORT";
    else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
        return L"UNSIGNED_SHORT";
    else if (ty == TINYGLTF_COMPONENT_TYPE_INT)
        return L"INT";
    else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        return L"UNSIGNED_INT";
    else if (ty == TINYGLTF_COMPONENT_TYPE_FLOAT)
        return L"FLOAT";
    else if (ty == TINYGLTF_COMPONENT_TYPE_DOUBLE)
        return L"DOUBLE";

    return L"**UNKNOWN**";
}


//size_t GltfUtils::SizeOfComponentType(int ty)
//{
//    switch (ty)
//    {
//    case TINYGLTF_COMPONENT_TYPE_BYTE:              return sizeof(int8_t);
//    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:     return sizeof(uint8_t);
//    case TINYGLTF_COMPONENT_TYPE_SHORT:             return sizeof(int16_t);
//    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:    return sizeof(uint16_t);
//    case TINYGLTF_COMPONENT_TYPE_INT:               return sizeof(int32_t);
//    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:      return sizeof(uint32_t);
//    default:                                        return 0;
//    }
//}


//std::wstring GltfUtils::TargetToWstring(int target) {
//    if (target == 34962)
//        return L"GL_ARRAY_BUFFER";
//    else if (target == 34963)
//        return L"GL_ELEMENT_ARRAY_BUFFER";
//    else
//        return L"**UNKNOWN**";
//}


std::wstring GltfUtils::FloatArrayToWstring(const std::vector<double> &arr)
{
    if (arr.size() == 0)
        return L"";

    std::stringstream ss;
    ss << "[ ";
    for (size_t i = 0; i < arr.size(); i++)
        ss << arr[i] << ((i != arr.size() - 1) ? ", " : "");
    ss << " ]";

    return Utils::StringToWstring(ss.str());
}


std::wstring GltfUtils::StringDoubleMapToWstring(const std::map<std::string, double> &mp)
{
    if (mp.size() == 0)
        return L"";

    std::stringstream ss;
    ss << "[";
    bool first = true;
    for (auto &item : mp)
    {
        ss << (first ? " " : ", ");
        ss << item.first << ": " << item.second;
        first = false;
    }
    ss << " ]";

    return Utils::StringToWstring(ss.str());
}


std::wstring GltfUtils::ParameterValueToWstring(const tinygltf::Parameter &param)
{
    if (!param.number_array.empty())
        return FloatArrayToWstring(param.number_array);
    else if (!param.json_double_value.empty())
        return StringDoubleMapToWstring(param.json_double_value);
    else if (param.has_number_value)
    {
        std::wstringstream ss;
        ss << param.number_value;
        return ss.str();
    }
    else
        return Utils::StringToWstring("\"" + param.string_value + "\"");
}

