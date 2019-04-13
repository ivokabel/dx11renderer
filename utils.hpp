#pragma once

#include <windows.h>
#include <stdio.h>

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
}
