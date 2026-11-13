#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Core/Deflate.h"

#include <climits>
#include <cstdlib>

#if defined(_MSC_VER)
#pragma warning(disable : 4505)
#pragma warning(push, 0)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STBI_NO_FAILURE_STRINGS
#include "stb_image.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace Idler::Deflate
{
    std::optional<std::string> Pack(std::string_view data)
    {
        if (data.size() > static_cast<std::size_t>(INT_MAX))
            return std::nullopt;
        int length = 0;
        unsigned char* packed = stbi_zlib_compress(reinterpret_cast<unsigned char*>(const_cast<char*>(data.data())), static_cast<int>(data.size()), &length, 8);
        if (!packed)
            return std::nullopt;
        std::optional<std::string> raw;
        if (length >= 6)
            raw = std::string(reinterpret_cast<const char*>(packed) + 2, static_cast<std::size_t>(length) - 6);
        std::free(packed);
        return raw;
    }

    std::optional<std::string> Unpack(std::string_view packed, std::size_t size)
    {
        if (size > static_cast<std::size_t>(INT_MAX) || packed.size() > static_cast<std::size_t>(INT_MAX))
            return std::nullopt;
        std::string data(size, '\0');
        if (size == 0)
            return data;
        const int written = stbi_zlib_decode_noheader_buffer(data.data(), static_cast<int>(size), packed.data(), static_cast<int>(packed.size()));
        if (written < 0 || static_cast<std::size_t>(written) != size)
            return std::nullopt;
        return data;
    }
}
