#pragma once

namespace Log
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

        wchar_t tmpBuff1[1024] = {};
        swprintf_s(tmpBuff1, msg, args...);

        wchar_t tmpBuff2[1024] = {};
        swprintf_s(tmpBuff2, L"[% 7s] %s\n", LogLevelToString(level), tmpBuff1);

        OutputDebugString(tmpBuff2);
    }


}
