#include "utils.hpp"


const Utils::ELogLevel Utils::sLoggingLevel = Utils::eDebug;


const wchar_t * Utils::LogLevelToString(ELogLevel level)
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
