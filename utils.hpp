#pragma once

#include <windows.h>
#include <stdio.h>

namespace Utils
{
    enum ELogLevel
    {
        eDebug,
        eWarning,
        eError,
    };


    extern const ELogLevel sLoggingLevel;


    const wchar_t * LogLevelToString(ELogLevel level);


    template <typename... Args>
    void Log(ELogLevel level, const wchar_t *msg, Args... args)
    {
        if (level < sLoggingLevel)
            return;

        wchar_t tmpBuff1[1024] = { 0 };
        swprintf_s(tmpBuff1, msg, args...);

        wchar_t tmpBuff2[1024] = { 0 };
        swprintf_s(tmpBuff2, L"[% 7s] %s\n", LogLevelToString(level), tmpBuff1);

        OutputDebugString(tmpBuff2);
    }


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
