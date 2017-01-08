
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Log.cpp
// Author: Guilherme R. Lampert
// Created on: 07/01/17
// Brief: Very simple debug log.
// ================================================================================================

#include "Log.hpp"
#include "Utils.hpp"

#if (WIN32 || WIN64)
    #define NOMINMAX 1
    #define WIN32_LEAN_AND_MEAN 1
    #include <Windows.h>
#endif // WIN32 || WIN64

namespace VkToolbox
{
namespace Log
{

// ========================================================

static constexpr int  PrintfBufferSize = 2048;
static constexpr int  PrefixMaxLength  = 128;
static VerbosityLevel s_verbosityLevel = VerbosityLevel::Debug;
static FILE *         s_outputStream   = stdout;

// ========================================================

static void printLine(const char * prefix, const char * message)
{
    char buffer[PrintfBufferSize + PrefixMaxLength];
    const std::size_t n = std::snprintf(buffer, arrayLength(buffer), "%s: %s\n", prefix, message);
    buffer[n < arrayLength(buffer) ? n : arrayLength(buffer) - 1] = '\0';

    // Console or log file:
    std::fputs(buffer, s_outputStream);

    // Visual Studio output window:
    #ifdef _MSC_VER
    OutputDebugStringA(buffer);
    #endif // _MSC_VER
}

// ========================================================

void setVerbosityLevel(const VerbosityLevel lvl)
{
    s_verbosityLevel = lvl;
}

VerbosityLevel getVerbosityLevel()
{
    return s_verbosityLevel;
}

// ========================================================

void setOutputStream(FILE* stream)
{
    assert(stream != nullptr);
    s_outputStream = stream;
}

FILE* getOutputStream()
{
    return s_outputStream;
}

// ========================================================

void debugF(VKTB_PRINTF_LIKE const char * fmt, ...)
{
    if (s_verbosityLevel < VerbosityLevel::Debug)
    {
        return;
    }

    va_list vaArgs;
    char buffer[PrintfBufferSize];

    va_start(vaArgs, fmt);
    std::vsnprintf(buffer, arrayLength(buffer), fmt, vaArgs);
    va_end(vaArgs);

    printLine("[debug]", buffer);
}

void warningF(VKTB_PRINTF_LIKE const char * fmt, ...)
{
    if (s_verbosityLevel < VerbosityLevel::Warning)
    {
        return;
    }

    va_list vaArgs;
    char buffer[PrintfBufferSize];

    va_start(vaArgs, fmt);
    std::vsnprintf(buffer, arrayLength(buffer), fmt, vaArgs);
    va_end(vaArgs);

    printLine("[warning]", buffer);
}

void errorF(VKTB_PRINTF_LIKE const char * fmt, ...)
{
    if (s_verbosityLevel < VerbosityLevel::Error)
    {
        return;
    }

    va_list vaArgs;
    char buffer[PrintfBufferSize];

    va_start(vaArgs, fmt);
    std::vsnprintf(buffer, arrayLength(buffer), fmt, vaArgs);
    va_end(vaArgs);

    printLine("[error]", buffer);
}

} // namespace Log
} // namespace VkToolbox
