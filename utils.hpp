#pragma once

#include <windows.h>
#include <stdio.h>

namespace Utils
{
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
