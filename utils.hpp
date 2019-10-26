#pragma once

#include <windows.h>
#include <stdio.h>
#include <string>

namespace Utils
{
    template <typename T>
    void ReleaseAndMakeNull(T &value)
    {
        if (value)
        {
            value->Release();
            value = nullptr;
        }
    }

    std::wstring GetFilePathExt(const std::wstring &path);
    std::string  GetFilePathExt(const std::string  &path);

    std::string  WideStringToString(const std::wstring &wideString);
    std::wstring StringToWideString(const std::string &string);
}
