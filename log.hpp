#pragma once

namespace Log
{
    enum ELoggingLevel
    {
        eDebug,
        eWarning,
        eError,
    };


    extern const ELoggingLevel sLoggingLevel;


    const wchar_t * LogLevelToString(ELoggingLevel level);


    template <typename... Args>
    void Write(ELoggingLevel level, const wchar_t *msg, Args... args)
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
