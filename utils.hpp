#pragma once

#include <windows.h>
#include <stdio.h>
#include <string>

namespace Utils
{
    template<class T, class U = T>
    T Exchange(T& obj, U&& new_value)
    {
        T old_value = std::move(obj);
        obj = std::forward<U>(new_value);
        return old_value;
    }

    template <typename T>
    void ReleaseAndMakeNull(T &value)
    {
        if (value)
        {
            value->Release();
            value = nullptr;
        }
    }

    template <typename T>
    void SafeAddRef(T &value)
    {
        if (value)
            value->AddRef();
    }

    std::wstring GetFilePathExt(const std::wstring &path);
    std::string  GetFilePathExt(const std::string  &path);

    std::string  WstringToString(const std::wstring &wideString);
    std::wstring StringToWstring(const std::string &string);

    inline const wchar_t* ConfigName()
    {
#ifdef DEBUG
        return L"debug";
#else
        return L"release";
#endif // DEBUG
    }
}
