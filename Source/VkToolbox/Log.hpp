#pragma once

// ================================================================================================
// File: VkToolbox/Log.hpp
// Author: Guilherme R. Lampert
// Created on: 07/01/17
// Brief: Very simple debug log.
// ================================================================================================

// Microsoft's own flavor of format string checking.
// This sadly won't even be used by the compiler, only the static analyzer cares about it,
// so you only get the warnings when building with '/analyze'. Still better than nothing.
#ifdef _MSC_VER
    #include <sal.h>
    #define VKTB_PRINTF_LIKE _Printf_format_string_
    #define VKTB_NO_RETURN   __declspec(noreturn)
#else // !_MSC_VER
    #define VKTB_PRINTF_LIKE /* nothing */
    #define VKTB_NO_RETURN   /* nothing */
#endif // _MSC_VER

// For FILE* and var args.
#include <cstdarg>
#include <cstdio>

// ========================================================

namespace VkToolbox
{
namespace Log
{

enum class VerbosityLevel
{
    Silent, Error, Warning, Debug
};

// Default = Debug
void setVerbosityLevel(VerbosityLevel lvl);
VerbosityLevel verbosityLevel();

// Default = stdout
void setOutputStream(FILE* stream);
FILE* outputStream();

void debugF(VKTB_PRINTF_LIKE const char * fmt, ...);
void warningF(VKTB_PRINTF_LIKE const char * fmt, ...);
void errorF(VKTB_PRINTF_LIKE const char * fmt, ...);
VKTB_NO_RETURN void fatalF(VKTB_PRINTF_LIKE const char * fmt, ...);

} // namespace Log
} // namespace VkToolbox
