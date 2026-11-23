#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace blitzdj
{
    class OAuthCallbackServer
    {
    public:
        explicit OAuthCallbackServer(const std::string& redirectUri);
        ~OAuthCallbackServer();

        OAuthCallbackServer(const OAuthCallbackServer&) = delete;
        OAuthCallbackServer& operator=(const OAuthCallbackServer&) = delete;

        static bool supports(const std::string& redirectUri);

        bool start();
        void stop();

        bool isListening() const;

        std::optional<std::string> waitForRedirect(
            std::chrono::seconds timeout,
            const std::atomic<bool>* cancel = nullptr
        );

        const std::string& getLastError() const;

    private:
        std::string host_;
        std::string path_;
        int port_ = 0;
        bool valid_ = false;
        bool networkReady_ = false;

        std::intptr_t socket_ = -1;

        std::string lastError_;

        std::optional<std::string> handleClient(std::intptr_t client);
    };
}
