#pragma once

#include "TaskQueue.hpp"

#include <imgui.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace blitzdj::app
{
    struct Image
    {
        ImTextureID texture = 0;
        ImVec4 color = ImVec4(0, 0, 0, 0);
        bool ready = false;
    };

    class ImageCache
    {
    public:
        explicit ImageCache(TaskQueue& loader);
        ~ImageCache();

        ImageCache(const ImageCache&) = delete;
        ImageCache& operator=(const ImageCache&) = delete;

        Image get(const std::string& url, bool thumbnail);

        void uploadPending();

    private:
        struct Entry
        {
            unsigned int texture = 0;
            ImVec4 color = ImVec4(0, 0, 0, 0);
            bool loading = false;
            bool failed = false;
        };

        struct Decoded
        {
            std::string key;
            std::vector<std::uint8_t> pixels;
            int width = 0;
            int height = 0;
            ImVec4 color;
            bool failed = false;
        };

        TaskQueue& loader_;

        std::unordered_map<std::string, Entry> entries_;

        std::mutex pendingMutex_;
        std::vector<Decoded> pending_;

        static Decoded decode(const std::string& key, const std::string& url, bool thumbnail);
    };
}
