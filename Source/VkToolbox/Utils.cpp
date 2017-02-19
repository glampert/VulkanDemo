
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Utils.cpp
// Author: Guilherme R. Lampert
// Created on: 18/02/17
// Brief: Miscellaneous helper code.
// ================================================================================================

#include "Utils.hpp"
#include <cstdio>

namespace VkToolbox
{

std::unique_ptr<char[]> loadTextFile(const char * const inFilename, std::size_t * outFileSize)
{
    assert(inFilename  != nullptr);
    assert(outFileSize != nullptr);

    if (inFilename[0] == '\0')
    {
        (*outFileSize) = 0;
        return nullptr;
    }

    // We won't be performing any text operations in the file, just read all of it
    // with a single fread(), so it can be opened as if it was binary.
    #ifdef _MSC_VER
    FILE * fileIn = nullptr;
    fopen_s(&fileIn, inFilename, "rb");
    #else // !_MSC_VER
    FILE * fileIn = std::fopen(inFilename, "rb");
    #endif // _MSC_VER

    if (fileIn == nullptr)
    {
        (*outFileSize) = 0;
        return nullptr;
    }

    std::fseek(fileIn, 0, SEEK_END);
    const auto fileLength = std::ftell(fileIn);
    std::fseek(fileIn, 0, SEEK_SET);

    if (fileLength <= 0 || std::ferror(fileIn))
    {
        std::fclose(fileIn);
        (*outFileSize) = 0;
        return nullptr;
    }

    std::unique_ptr<char[]> fileContents{ new char[fileLength + 1] };
    if (std::fread(fileContents.get(), 1, fileLength, fileIn) != std::size_t(fileLength))
    {
        std::fclose(fileIn);
        (*outFileSize) = 0;
        return nullptr;
    }

    fileContents[fileLength] = '\0';
    (*outFileSize) = fileLength;

    std::fclose(fileIn);
    return fileContents;
}

} // namespace VkToolbox
