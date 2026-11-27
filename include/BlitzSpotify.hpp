#pragma once

#include "HttpClient.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace blitzdj
{
    using Json = nlohmann::json;

    struct SpotifyTrack
    {
        std::string id;
        std::string uri;
        std::string name;
        std::string artist;
        std::string album;
        std::string imageUrl;

        int durationMs = 0;
        bool explicitContent = false;
        bool playable = true;
    };

    struct SpotifyDevice
    {
        std::string id;
        std::string name;
        std::string type;

        bool active = false;
        bool restricted = false;
        int volume = 100;
    };

    struct SpotifyPlayback
    {
        SpotifyTrack track;
        SpotifyDevice device;

        bool playing = false;
        bool shuffle = false;

        int progressMs = 0;
        int durationMs = 0;

        std::string repeatMode = "off";
    };

    struct SpotifyTokens
    {
        std::string accessToken;
        std::string refreshToken;
        std::string tokenType = "Bearer";

        std::chrono::system_clock::time_point expiresAt{};

        bool isExpired() const
        {
            return accessToken.empty() ||
                std::chrono::system_clock::now() >= expiresAt;
        }
    };

    struct SpotifyResponse
    {
        bool success = false;
        long statusCode = 0;

        std::string error;
        Json data = Json::object();
    };

    class BlitzSpotify
    {
    public:
        static constexpr int SEARCH_PAGE_LIMIT = 10;
        static constexpr int SEARCH_MAX_OFFSET = 1000;

        explicit BlitzSpotify(
            const std::string& clientId,
            const std::string& redirectUri =
                "http://127.0.0.1:8888/callback"
        );

        ~BlitzSpotify();

        BlitzSpotify(const BlitzSpotify&) = delete;
        BlitzSpotify& operator=(const BlitzSpotify&) = delete;

        bool initialize();

        void setHttpClient(std::unique_ptr<HttpClient> client);

        using ErrorLogger = std::function<void(const std::string&)>;

        void setErrorLogger(ErrorLogger logger);

        void setSessionPath(const std::string& path);
        std::string getSessionPath() const;

        bool loadSession();
        bool saveSession();

        std::string generateAuthorizationUrl();

        bool authenticate(
            const std::string& authorizationCode,
            const std::string& returnedState
        );

        bool handleRedirect(const std::string& redirectUrl);

        bool refreshAccessToken();
        bool isAuthenticated() const;

        void logout();

        void setTokens(const SpotifyTokens& tokens);
        SpotifyTokens getTokens() const;

        Json getCurrentUser();

        std::vector<SpotifyTrack> searchTracks(
            const std::string& query,
            int limit = SEARCH_PAGE_LIMIT,
            int offset = 0
        );

        std::optional<SpotifyTrack> getTrack(
            const std::string& trackId
        );

        bool play();
        bool playTrack(const std::string& trackUri);

        bool playTracks(
            const std::vector<std::string>& trackUris
        );

        bool playPlaylist(
            const std::string& playlistUri
        );

        bool pause();
        bool resume();

        bool nextTrack();
        bool previousTrack();

        bool seek(int positionMs);
        bool setVolume(int volume);

        bool setShuffle(bool enabled);
        bool setRepeat(const std::string& mode);

        bool addToQueue(const std::string& trackUri);
        Json getQueue();
        std::vector<SpotifyTrack> getQueueTracks();

        std::optional<SpotifyPlayback> getPlayback();
        std::optional<SpotifyTrack> getCurrentTrack();

        bool isPlaying();

        std::vector<SpotifyDevice> getDevices();

        bool transferPlayback(
            const std::string& deviceId,
            bool startPlaying = false
        );

        void setDevice(const std::string& deviceId);
        std::string getDevice() const;

        void enableAutoDJ(bool enabled);
        bool isAutoDJEnabled() const;

        bool queueAutomaticTrack(
            const SpotifyTrack& track
        );

        bool queueAutomaticTracks(
            const std::vector<SpotifyTrack>& tracks
        );

        std::string getLastError() const;
        long getLastStatusCode() const;

        void clearError();

        static std::string urlEncode(const std::string& value);
        static std::string urlDecode(const std::string& value);

    private:
        std::string clientId_;
        std::string redirectUri_;
        std::string deviceId_;
        std::string sessionPath_;

        SpotifyTokens tokens_;

        std::unique_ptr<HttpClient> http_;

        bool initialized_ = false;
        bool autoDJEnabled_ = false;

        std::string codeVerifier_;
        std::string oauthState_;

        std::string lastError_;
        long lastStatusCode_ = 0;

        ErrorLogger errorLogger_;

        mutable std::recursive_mutex mutex_;

        static constexpr const char* API_URL =
            "https://api.spotify.com/v1";

        static constexpr const char* AUTH_URL =
            "https://accounts.spotify.com/authorize";

        static constexpr const char* TOKEN_URL =
            "https://accounts.spotify.com/api/token";

        static constexpr int MAX_ATTEMPTS = 3;

        SpotifyResponse request(
            const std::string& method,
            const std::string& endpoint,
            const Json& body = nullptr
        );

        SpotifyResponse tokenRequest(
            const std::string& formData
        );

        SpotifyResponse finishResponse(
            const HttpResponse& response,
            const std::string& context
        );

        bool updateTokens(const Json& response);

        bool ensureValidToken();

        SpotifyTrack parseTrack(const Json& data) const;

        SpotifyDevice parseDevice(const Json& data) const;

        SpotifyPlayback parsePlayback(
            const Json& data
        ) const;

        std::string playerEndpoint(
            const std::string& path
        ) const;

        void setError(
            const std::string& message,
            long statusCode = 0
        );
    };
}
