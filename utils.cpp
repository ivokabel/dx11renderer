#include "utils.hpp"

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
