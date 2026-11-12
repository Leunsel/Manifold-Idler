#include "App/Gl.h"

#include <SDL3/SDL_video.h>

#include <cstring>

#if defined(_WIN32)
#define IDLER_GL_API __stdcall
#else
#define IDLER_GL_API
#endif

namespace Idler::Gl
{
    namespace
    {
        constexpr unsigned kColorBufferBit = 0x00004000;
        constexpr unsigned kTexture2D = 0x0DE1;
        constexpr unsigned kUnsignedByte = 0x1401;
        constexpr unsigned kRgba = 0x1908;
        constexpr unsigned kBgra = 0x80E1;
        constexpr unsigned kTextureMagFilter = 0x2800;
        constexpr unsigned kTextureMinFilter = 0x2801;
        constexpr unsigned kTextureWrapS = 0x2802;
        constexpr unsigned kTextureWrapT = 0x2803;
        constexpr int kLinear = 0x2601;
        constexpr int kClampToEdge = 0x812F;
        constexpr unsigned kUnpackRowLength = 0x0CF2;
        constexpr unsigned kUnpackAlignment = 0x0CF5;
        constexpr unsigned kPackAlignment = 0x0D05;
        constexpr unsigned kTextureBinding2D = 0x8069;

        using ViewportFn = void(IDLER_GL_API*)(int, int, int, int);
        using ClearColorFn = void(IDLER_GL_API*)(float, float, float, float);
        using ClearFn = void(IDLER_GL_API*)(unsigned);
        using ReadPixelsFn = void(IDLER_GL_API*)(int, int, int, int, unsigned, unsigned, void*);
        using GenTexturesFn = void(IDLER_GL_API*)(int, unsigned*);
        using DeleteTexturesFn = void(IDLER_GL_API*)(int, const unsigned*);
        using BindTextureFn = void(IDLER_GL_API*)(unsigned, unsigned);
        using TexParameteriFn = void(IDLER_GL_API*)(unsigned, unsigned, int);
        using TexImage2DFn = void(IDLER_GL_API*)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
        using PixelStoreiFn = void(IDLER_GL_API*)(unsigned, int);
        using GetIntegervFn = void(IDLER_GL_API*)(unsigned, int*);

        struct Functions
        {
            ViewportFn Viewport = nullptr;
            ClearColorFn ClearColor = nullptr;
            ClearFn Clear = nullptr;
            ReadPixelsFn ReadPixels = nullptr;
            GenTexturesFn GenTextures = nullptr;
            DeleteTexturesFn DeleteTextures = nullptr;
            BindTextureFn BindTexture = nullptr;
            TexParameteriFn TexParameteri = nullptr;
            TexImage2DFn TexImage2D = nullptr;
            PixelStoreiFn PixelStorei = nullptr;
            GetIntegervFn GetIntegerv = nullptr;
        };

        Functions g_gl;
        bool g_loaded = false;

        template <typename T>
        bool Find(T& function, const char* name)
        {
            function = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
            return function != nullptr;
        }
    }

    bool Load()
    {
        g_loaded = Find(g_gl.Viewport, "glViewport") && Find(g_gl.ClearColor, "glClearColor") && Find(g_gl.Clear, "glClear") &&
            Find(g_gl.ReadPixels, "glReadPixels") && Find(g_gl.GenTextures, "glGenTextures") && Find(g_gl.DeleteTextures, "glDeleteTextures") &&
            Find(g_gl.BindTexture, "glBindTexture") && Find(g_gl.TexParameteri, "glTexParameteri") && Find(g_gl.TexImage2D, "glTexImage2D") &&
            Find(g_gl.PixelStorei, "glPixelStorei") && Find(g_gl.GetIntegerv, "glGetIntegerv");
        return g_loaded;
    }

    void Clear(int width, int height, float red, float green, float blue)
    {
        if (!g_loaded)
            return;
        g_gl.Viewport(0, 0, width, height);
        g_gl.ClearColor(red, green, blue, 1.0f);
        g_gl.Clear(kColorBufferBit);
    }

    bool ReadPixels(int width, int height, std::vector<std::uint8_t>& rgba)
    {
        if (!g_loaded || width <= 0 || height <= 0)
            return false;
        const std::size_t row = static_cast<std::size_t>(width) * 4;
        std::vector<std::uint8_t> flipped(row * static_cast<std::size_t>(height));
        g_gl.PixelStorei(kPackAlignment, 1);
        g_gl.ReadPixels(0, 0, width, height, kRgba, kUnsignedByte, flipped.data());
        rgba.resize(flipped.size());
        for (int y = 0; y < height; ++y)
            std::memcpy(rgba.data() + static_cast<std::size_t>(y) * row, flipped.data() + static_cast<std::size_t>(height - 1 - y) * row, row);
        for (std::size_t index = 3; index < rgba.size(); index += 4)
            rgba[index] = 255;
        return true;
    }

    unsigned CreateTexture(const std::uint8_t* bgra, int width, int height)
    {
        if (!g_loaded || !bgra || width <= 0 || height <= 0)
            return 0;
        int bound = 0;
        g_gl.GetIntegerv(kTextureBinding2D, &bound);
        unsigned texture = 0;
        g_gl.GenTextures(1, &texture);
        g_gl.BindTexture(kTexture2D, texture);
        g_gl.TexParameteri(kTexture2D, kTextureMinFilter, kLinear);
        g_gl.TexParameteri(kTexture2D, kTextureMagFilter, kLinear);
        g_gl.TexParameteri(kTexture2D, kTextureWrapS, kClampToEdge);
        g_gl.TexParameteri(kTexture2D, kTextureWrapT, kClampToEdge);
        g_gl.PixelStorei(kUnpackRowLength, 0);
        g_gl.PixelStorei(kUnpackAlignment, 4);
        g_gl.TexImage2D(kTexture2D, 0, static_cast<int>(kRgba), width, height, 0, kBgra, kUnsignedByte, bgra);
        g_gl.BindTexture(kTexture2D, static_cast<unsigned>(bound));
        return texture;
    }

    void DeleteTexture(unsigned texture)
    {
        if (g_loaded && texture != 0)
            g_gl.DeleteTextures(1, &texture);
    }
}
