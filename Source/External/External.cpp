
// ================================================================================================
// File: External/External.cpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Definitions for the header-only libraries that need an implementation in a CPP file.
// ================================================================================================

#include "External.hpp"

// ========================================================
// str/str16/str32/str64/etc...
#define STR_IMPLEMENTATION 1
#include "str/str.hpp"
// ========================================================
// The STB libraries emit quite a few warnings, but they
// are presumably harmless, so we'll just suppress them.
#ifdef _MSC_VER
    #pragma warning(disable: 4100) // "Unreferenced formal parameter"
    #pragma warning(disable: 4244) // "Conversion from 'int' to 'uint16', possible loss of data"
    #pragma warning(disable: 4456) // "Declaration of <foo> hides previous local declaration"
    #pragma warning(disable: 4505) // "Unreferenced local function has been removed"
    #pragma warning(disable: 4702) // "Unreachable code"
    #pragma warning(disable: 4996) // "This function or variable may be unsafe. Consider using blah instead"
#endif // _MSC_VER
// ========================================================
// STB Image (stbi)
#define STB_IMAGE_IMPLEMENTATION 1
#define STBI_NO_LINEAR 1
#define STBI_NO_HDR 1
#define STBI_NO_PIC 1
#define STBI_NO_PNM 1
#define STBI_NO_PSD 1
#include "stb/stb_image.h"
// ========================================================
// STB Image Write (stbiw)
#define STB_IMAGE_WRITE_IMPLEMENTATION 1
#include "stb/stb_image_write.h"
// ========================================================
// STB Image Resize (stbir)
#define STB_IMAGE_RESIZE_IMPLEMENTATION 1
#include "stb/stb_image_resize.h"
// ========================================================
