#include "UI/CoverPlatform.h"

#include "App/Gl.h"

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>

namespace Idler::UI::CoverPlatform
{
    namespace
    {
        constexpr std::uintmax_t kLargestFile = 64ull * 1024 * 1024;

        bool ReadFile(const std::filesystem::path& file, std::vector<unsigned char>& bytes)
        {
            std::error_code error;
            const std::uintmax_t size = std::filesystem::file_size(file, error);
            if (error || size == 0 || size > kLargestFile)
                return false;
            std::ifstream stream(file, std::ios::binary);
            if (!stream)
                return false;
            bytes.resize(static_cast<std::size_t>(size));
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            return static_cast<std::size_t>(stream.gcount()) == bytes.size();
        }

        void Shrink(const unsigned char* source, int stride, int sourceWidth, int sourceHeight, unsigned width, unsigned height, std::vector<float>& target)
        {
            std::vector<float> rows(static_cast<std::size_t>(width) * static_cast<std::size_t>(sourceHeight) * 4, 0.0f);
            const double stepX = static_cast<double>(sourceWidth) / width;
            for (int y = 0; y < sourceHeight; ++y)
            {
                const unsigned char* line = source + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
                for (unsigned x = 0; x < width; ++x)
                {
                    const double from = x * stepX;
                    const double to = from + stepX;
                    float sum[4] = {};
                    for (int column = static_cast<int>(from); column < sourceWidth && column < to; ++column)
                    {
                        const double weight = std::min<double>(column + 1, to) - std::max<double>(column, from);
                        if (weight <= 0.0)
                            continue;
                        for (int channel = 0; channel < 4; ++channel)
                            sum[channel] += static_cast<float>(weight) * line[column * 4 + channel];
                    }
                    float* out = rows.data() + (static_cast<std::size_t>(y) * width + x) * 4;
                    for (int channel = 0; channel < 4; ++channel)
                        out[channel] = sum[channel] / static_cast<float>(stepX);
                }
            }

            target.assign(static_cast<std::size_t>(width) * height * 4, 0.0f);
            const double stepY = static_cast<double>(sourceHeight) / height;
            for (unsigned y = 0; y < height; ++y)
            {
                const double from = y * stepY;
                const double to = from + stepY;
                float* out = target.data() + static_cast<std::size_t>(y) * width * 4;
                for (int row = static_cast<int>(from); row < sourceHeight && row < to; ++row)
                {
                    const double weight = std::min<double>(row + 1, to) - std::max<double>(row, from);
                    if (weight <= 0.0)
                        continue;
                    const float* line = rows.data() + static_cast<std::size_t>(row) * width * 4;
                    for (std::size_t index = 0; index < static_cast<std::size_t>(width) * 4; ++index)
                        out[index] += static_cast<float>(weight) * line[index];
                }
                for (std::size_t index = 0; index < static_cast<std::size_t>(width) * 4; ++index)
                    out[index] /= static_cast<float>(stepY);
            }
        }
    }

    void SetDevice(void*)
    {
    }

    bool UploadsOnWorker()
    {
        return false;
    }

    void BeginWorker()
    {
    }

    void EndWorker()
    {
    }

    bool Decode(const std::filesystem::path& file, unsigned width, unsigned height, float shape, Pixels& pixels)
    {
        std::vector<unsigned char> bytes;
        if (!ReadFile(file, bytes) || bytes.size() > static_cast<std::size_t>(INT32_MAX))
            return false;

        int sourceWidth = 0;
        int sourceHeight = 0;
        int channels = 0;
        unsigned char* decoded = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &sourceWidth, &sourceHeight, &channels, 4);
        if (!decoded)
            return false;
        if (sourceWidth <= 0 || sourceHeight <= 0 || width == 0 || height == 0)
        {
            stbi_image_free(decoded);
            return false;
        }

        int cutX = 0;
        int cutY = 0;
        int cutWidth = sourceWidth;
        int cutHeight = sourceHeight;
        const float aspect = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);
        if (aspect > shape + 0.02f)
        {
            cutWidth = static_cast<int>(static_cast<float>(sourceHeight) * shape);
            cutX = (sourceWidth - cutWidth) / 2;
        }
        else if (aspect < shape - 0.02f)
        {
            cutHeight = static_cast<int>(static_cast<float>(sourceWidth) / shape);
            cutY = (sourceHeight - cutHeight) / 2;
        }
        if (cutWidth <= 0 || cutHeight <= 0)
        {
            stbi_image_free(decoded);
            return false;
        }
        if (static_cast<unsigned>(cutWidth) < width)
        {
            width = static_cast<unsigned>(cutWidth);
            height = static_cast<unsigned>(cutHeight);
        }

        std::vector<float> scaled;
        const unsigned char* start = decoded + (static_cast<std::size_t>(cutY) * static_cast<std::size_t>(sourceWidth) + static_cast<std::size_t>(cutX)) * 4;
        Shrink(start, sourceWidth * 4, cutWidth, cutHeight, width, height, scaled);
        stbi_image_free(decoded);

        pixels.Width = width;
        pixels.Height = height;
        pixels.Bgra.resize(scaled.size());
        for (std::size_t index = 0; index < scaled.size(); index += 4)
        {
            const auto channel = [&](std::size_t offset) { return static_cast<std::uint8_t>(std::clamp(scaled[index + offset] + 0.5f, 0.0f, 255.0f)); };
            pixels.Bgra[index + 0] = channel(2);
            pixels.Bgra[index + 1] = channel(1);
            pixels.Bgra[index + 2] = channel(0);
            pixels.Bgra[index + 3] = channel(3);
        }
        return true;
    }

    ImTextureID Upload(const Pixels& pixels)
    {
        const unsigned texture = Gl::CreateTexture(pixels.Bgra.data(), static_cast<int>(pixels.Width), static_cast<int>(pixels.Height));
        return texture != 0 ? static_cast<ImTextureID>(texture) : ImTextureID{};
    }

    void Release(ImTextureID texture)
    {
        if (texture != ImTextureID{})
            Gl::DeleteTexture(static_cast<unsigned>(texture));
    }
}
