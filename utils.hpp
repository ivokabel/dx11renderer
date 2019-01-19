#pragma once

#include <string>

namespace Utils
{
    void Log(const wchar_t *msg /*TODO: Message level: error, warning, debug, ...*/);

    template <typename T>
    void ReleaseAndMakeNullptr(T &value)
    {
        if (value)
        {
            value->Release();
            value = nullptr;
        }
    }
}
