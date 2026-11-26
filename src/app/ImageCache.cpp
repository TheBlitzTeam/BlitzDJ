#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "ImageCache.hpp"

#include "HttpClient.hpp"

#include <imgui_impl_opengl3_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>

#include <algorithm>

namespace blitzdj::app
{
    namespace
    {
        constexpr int THUMBNAIL_SIZE = 128;
        constexpr int FULL_SIZE = 640;

        std::vector<std::uint8_t> resize(const std::uint8_t* source, int width, int height, int limit, int& outWidth, int& outHeight)
        {
            const int largest = std::max(width, height);

            if (largest <= limit)
            {
                outWidth = width;
                outHeight = height;
                return std::vector<std::uint8_t>(source, source + static_cast<std::size_t>(width) * height * 4);
            }

            outWidth = std::max(1, width * limit / largest);
            outHeight = std::max(1, height * limit / largest);

            std::vector<std::uint8_t> output(static_cast<std::size_t>(outWidth) * outHeight * 4);

            for (int y = 0; y < outHeight; ++y)
            {
                const int y0 = y * height / outHeight;
                const int y1 = std::max(y0 + 1, (y + 1) * height / outHeight);

                for (int x = 0; x < outWidth; ++x)
                {
                    const int x0 = x * width / outWidth;
                    const int x1 = std::max(x0 + 1, (x + 1) * width / outWidth);

                    unsigned int sum[4] = {0, 0, 0, 0};
                    unsigned int count = 0;

                    for (int sy = y0; sy < y1; ++sy)
                    {
                        const std::uint8_t* row = source + (static_cast<std::size_t>(sy) * width + x0) * 4;

                        for (int sx = x0; sx < x1; ++sx, row += 4)
                        {
                            sum[0] += row[0];
                            sum[1] += row[1];
                            sum[2] += row[2];
                            sum[3] += row[3];
                            ++count;
                        }
                    }

                    std::uint8_t* target = &output[(static_cast<std::size_t>(y) * outWidth + x) * 4];

                    for (int c = 0; c < 4; ++c)
                    {
                        target[c] = static_cast<std::uint8_t>(sum[c] / count);
                    }
                }
            }

            return output;
        }

        ImVec4 dominantColor(const std::uint8_t* pixels, int width, int height)
        {
            double weightSum = 0.0;
            double r = 0.0;
            double g = 0.0;
            double b = 0.0;

            const int step = std::max(1, std::max(width, height) / 48);

            for (int y = 0; y < height; y += step)
            {
                for (int x = 0; x < width; x += step)
                {
                    const std::uint8_t* pixel = pixels + (static_cast<std::size_t>(y) * width + x) * 4;

                    const double pr = pixel[0] / 255.0;
                    const double pg = pixel[1] / 255.0;
                    const double pb = pixel[2] / 255.0;

                    const double maximum = std::max({pr, pg, pb});
                    const double minimum = std::min({pr, pg, pb});
                    const double saturation = maximum > 0.0 ? (maximum - minimum) / maximum : 0.0;
                    const double weight = 0.05 + saturation * saturation * maximum;

                    r += pr * weight;
                    g += pg * weight;
                    b += pb * weight;
                    weightSum += weight;
                }
            }

            if (weightSum <= 0.0)
            {
                return ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
            }

            return ImVec4(
                static_cast<float>(r / weightSum),
                static_cast<float>(g / weightSum),
                static_cast<float>(b / weightSum),
                1.0f
            );
        }
    }

    ImageCache::ImageCache(TaskQueue& loader)
        : loader_(loader)
    {
    }

    ImageCache::~ImageCache()
    {
        for (auto& [key, entry] : entries_)
        {
            if (entry.texture != 0)
            {
                glDeleteTextures(1, &entry.texture);
            }
        }
    }

    ImageCache::Decoded ImageCache::decode(const std::string& key, const std::string& url, bool thumbnail)
    {
        Decoded result;
        result.key = key;
        result.failed = true;

        auto http = createDefaultHttpClient();

        HttpRequest request;
        request.url = url;
        request.timeoutSeconds = 20;

        const auto response = http->send(request);

        if (!response.error.empty() || response.statusCode != 200 || response.body.empty())
        {
            return result;
        }

        int width = 0;
        int height = 0;
        int channels = 0;

        std::uint8_t* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(response.body.data()),
            static_cast<int>(response.body.size()),
            &width,
            &height,
            &channels,
            4
        );

        if (!pixels)
        {
            return result;
        }

        result.pixels = resize(pixels, width, height, thumbnail ? THUMBNAIL_SIZE : FULL_SIZE, result.width, result.height);
        result.color = dominantColor(pixels, width, height);
        result.failed = false;

        stbi_image_free(pixels);
        return result;
    }

    Image ImageCache::get(const std::string& url, bool thumbnail)
    {
        Image image;

        if (url.empty())
        {
            return image;
        }

        const std::string key = (thumbnail ? "t:" : "f:") + url;
        auto& entry = entries_[key];

        if (entry.texture != 0)
        {
            image.texture = static_cast<ImTextureID>(entry.texture);
            image.color = entry.color;
            image.ready = true;
            return image;
        }

        if (!entry.loading && !entry.failed)
        {
            entry.loading = true;

            loader_.post([this, key, url, thumbnail]
            {
                auto decoded = decode(key, url, thumbnail);

                std::lock_guard<std::mutex> lock(pendingMutex_);
                pending_.push_back(std::move(decoded));
            });
        }

        return image;
    }

    void ImageCache::uploadPending()
    {
        std::vector<Decoded> pending;

        {
            std::lock_guard<std::mutex> lock(pendingMutex_);
            pending.swap(pending_);
        }

        for (auto& decoded : pending)
        {
            auto& entry = entries_[decoded.key];
            entry.loading = false;

            if (decoded.failed)
            {
                entry.failed = true;
                continue;
            }

            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, decoded.width, decoded.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, decoded.pixels.data());

            entry.texture = texture;
            entry.color = decoded.color;
        }
    }
}
