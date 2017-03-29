#pragma once

// ================================================================================================
// -*- C++ -*-
// File: External/External.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: External libraries and Git submodules.
// ================================================================================================

#if defined(_MSC_VER) && !_HAS_EXCEPTIONS
    // "noexcept used with no exception handling mode specified; termination on exception is not guaranteed"
    // Visual Studio doesn't simply ignore 'noexcept' if compiling with C++ exceptions disabled.
    // Some of my libraries below use noexcept decoration, which causes this warning. We can ignore
    // this warning since exceptions cannot be thrown when they are disabled!
    #pragma warning(disable: 4577)
#endif // _MSC_VER && !_HAS_EXCEPTIONS

// ========================================================
#define ARRAY_VIEW_USE_ASSERTS 1
#include "array_view/array_view.hpp"
// ========================================================
#include "hash_index/hash_index.hpp"
// ========================================================
#include "enum_helpers/enum_helpers.hpp"
// ========================================================
#include "vectormath/vectormath.hpp"
// ========================================================
#include "str/str.hpp"
// ========================================================
#include "lib-cfg/cfg.hpp"
// ========================================================
