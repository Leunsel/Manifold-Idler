#pragma once

#define IDLER_VERSION_MAJOR 1
#define IDLER_VERSION_MINOR 3
#define IDLER_VERSION_PATCH 0
#define IDLER_VERSION_TEXT "1.3.0"

#ifndef RC_INVOKED
namespace Idler
{
    inline constexpr const char* kVersion = IDLER_VERSION_TEXT;
}
#endif
