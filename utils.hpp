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

    template <typename T>
    inline T ToggleBits(const T &aVal, const T &aMask)
    {
        return static_cast<T>(aVal ^ aMask);
    }

    template <typename T>
    inline T Lerp(const T &min, const T &max, const float c)
    {
        return (1.f - c) * min + c * max;
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

    float ModX(float x, float y);
}
