
// ================================================================================================
// File: VkToolbox/Utils.cpp
// Author: Guilherme R. Lampert
// Created on: 18/02/17
// Brief: Miscellaneous helper code.
// ================================================================================================

#include "Utils.hpp"
#include "External.hpp"

#include <direct.h>
#include <sys/stat.h>

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
    ScopedFileHandle fileIn = openFile(inFilename, "rb");
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
        (*outFileSize) = 0;
        return nullptr;
    }

    std::unique_ptr<char[]> fileContents{ new char[fileLength + 1] };
    if (std::fread(fileContents.get(), 1, fileLength, fileIn) != std::size_t(fileLength))
    {
        (*outFileSize) = 0;
        return nullptr;
    }

    fileContents[fileLength] = '\0';
    (*outFileSize) = fileLength;

    return fileContents;
}

FILE * openFile(const char * const filename, const char * const mode)
{
    assert(filename != nullptr && mode != nullptr);

    #ifdef _MSC_VER
    FILE * file = nullptr;
    fopen_s(&file, filename, mode);
    #else // !_MSC_VER
    FILE * file = std::fopen(filename, mode);
    #endif // _MSC_VER

    return file;
}

bool probeFile(const char * const filename)
{
    assert(filename != nullptr);

    struct _stat buf;
    const int result = _stat(filename, &buf);
    if (result != 0)
    {
        return false;
    }
    return (buf.st_mode & _S_IFREG) != 0;
}

const char * currentPath(str * inOutPathStr)
{
    assert(inOutPathStr != nullptr);

    char cwd[512];
    _getcwd(cwd, sizeof(cwd));

    inOutPathStr->set(cwd);
    return inOutPathStr->c_str();
}

} // namespace VkToolbox
