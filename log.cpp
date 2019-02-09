#include "log.hpp"

const Log::ELogLevel Log::sLoggingLevel = Log::eDebug;


const wchar_t * Log::LogLevelToString(ELogLevel level)
{
    switch (level)
    {
    case eDebug:
        return L"Debug";
    case eWarning:
        return L"Warning";
    case eError:
        return L"Error";
    default:
        return L"Uknown";
    }
}
