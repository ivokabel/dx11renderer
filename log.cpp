#include "log.hpp"

Log::ELoggingLevel Log::sLoggingLevel = Log::eDebug;


const wchar_t * Log::LogLevelToString(ELoggingLevel level)
{
    switch (level)
    {
    case eDebug:
        return L"Debug";
    case eInfo:
        return L"Info";
    case eWarning:
        return L"Warning";
    case eError:
        return L"Error";
    default:
        return L"Uknown";
    }
}
