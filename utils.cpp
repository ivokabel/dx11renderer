#include "utils.hpp"

// wstring->string conversion
#include <locale>
#include <codecvt>

#include <cmath>


std::wstring Utils::GetFilePathExt(const std::wstring &path)
{
    const auto lastDot = path.find_last_of(L".");
    if (lastDot != std::wstring::npos)
        return path.substr(lastDot + 1);
    else
        return L"";
}


std::string Utils::GetFilePathExt(const std::string &path)
{
    const auto lastDot = path.find_last_of(".");
    if (lastDot != std::string::npos)
        return path.substr(lastDot + 1);
    else
        return "";
}


std::string Utils::WstringToString(const std::wstring &wideString)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

    return converter.to_bytes(wideString);
}


std::wstring Utils::StringToWstring(const std::string &string)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

    return converter.from_bytes(string);
}


float Utils::ModX(float x, float y)
{
    float result = fmod(x, y);
    if (result < 0.0f)
        result += y;

    return result;
}
