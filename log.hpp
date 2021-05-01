#pragma once

#include <windows.h>

namespace Log
{
    enum ELoggingLevel
    {
        eNone,
        eError,
        eWarning,
        eInfo,
        eDebug,
    };


    extern ELoggingLevel sLoggingLevel;


    const wchar_t * LogLevelToString(ELoggingLevel level);


    template <typename... Args>
    void Write(ELoggingLevel msgLevel, const wchar_t *msg, Args... args)
    {
        if (sLoggingLevel < msgLevel)
            return;

        wchar_t tmpBuff1[100000] = {};
        swprintf_s(tmpBuff1, msg, args...);

        wchar_t tmpBuff2[100000] = {};
        swprintf_s(tmpBuff2, L"[% 7s] %s\n", LogLevelToString(msgLevel), tmpBuff1);

        OutputDebugString(tmpBuff2);
    }


    template <typename... Args>
    void Debug(const wchar_t *msg, Args... args)
    {
        Write(ELoggingLevel::eDebug, msg, args...);
    }


    template <typename... Args>
    void Info(const wchar_t *msg, Args... args)
    {
        Write(ELoggingLevel::eInfo, msg, args...);
    }


    template <typename... Args>
    void Warning(const wchar_t *msg, Args... args)
    {
        Write(ELoggingLevel::eWarning, msg, args...);
    }


    template <typename... Args>
    void Error(const wchar_t *msg, Args... args)
    {
        Write(ELoggingLevel::eError, msg, args...);
    }
}
