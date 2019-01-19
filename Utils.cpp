#include "utils.hpp"
#include <windows.h>

void Utils::Log(const wchar_t *msg /*TODO: Message level: error, warning, debug, ...*/)
{
    std::wstring tmpStr = msg;
    tmpStr += L"\n";

    OutputDebugString(tmpStr.c_str());
}

