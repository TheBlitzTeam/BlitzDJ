#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <optional>

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
        explicit BlitzSpotify(
            const std::string& clientId,
            const std::string& redirectUri =
                "http://127.0.0.1:8888/callback"
        );

        ~BlitzSpotify();

        BlitzSpotify(const BlitzSpotify&) = delete;
        BlitzSpotify& operator=(const BlitzSpotify&) = delete;

        // Authentication
        bool initialize();

        std::string generateAuthorizationUrl();
        bool authenticate(
            const std::string& authorizationCode,
            const std::string& returnedState
        );

        bool refreshAccessToken();
        bool isAuthenticated() const;

        void logout();

        void setTokens(const SpotifyTokens& tokens);
        const SpotifyTokens& getTokens() const;

        // Spotify profile
        Json getCurrentUser();

        // Music search
        std::vector<SpotifyTrack> searchTracks(
            const std::string& query,
            int limit = 20
        );

        std::optional<SpotifyTrack> getTrack(
            const std::string& trackId
        );

        // Player controls
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

        // Playback queue
        bool addToQueue(const std::string& trackUri);
        Json getQueue();

        // Playback information
        std::optional<SpotifyPlayback> getPlayback();
        std::optional<SpotifyTrack> getCurrentTrack();

        bool isPlaying();

        // Devices
        std::vector<SpotifyDevice> getDevices();

        bool transferPlayback(
            const std::string& deviceId,
            bool startPlaying = false
        );

        void setDevice(const std::string& deviceId);
        const std::string& getDevice() const;

        // Automatic DJ controls
        void enableAutoDJ(bool enabled);
        bool isAutoDJEnabled() const;

        bool queueAutomaticTrack(
            const SpotifyTrack& track
        );

        bool queueAutomaticTracks(
            const std::vector<SpotifyTrack>& tracks
        );

        // Error handling
        const std::string& getLastError() const;
        long getLastStatusCode() const;

        void clearError();

    private:
        std::string clientId_;
        std::string redirectUri_;
        std::string deviceId_;

        SpotifyTokens tokens_;

        bool initialized_ = false;
        bool autoDJEnabled_ = false;

        std::string codeVerifier_;
        std::string oauthState_;

        std::string lastError_;
        long lastStatusCode_ = 0;

        static constexpr const char* API_URL =
            "https://api.spotify.com/v1";

        static constexpr const char* AUTH_URL =
            "https://accounts.spotify.com/authorize";

        static constexpr const char* TOKEN_URL =
            "https://accounts.spotify.com/api/token";

        // HTTP requests
        SpotifyResponse request(
            const std::string& method,
            const std::string& endpoint,
            const Json& body = nullptr
        );

        SpotifyResponse tokenRequest(
            const std::string& formData
        );

        // OAuth PKCE
        std::string generateCodeVerifier();
        std::string generateCodeChallenge(
            const std::string& verifier
        );

        std::string generateRandomState();

        std::string urlEncode(
            const std::string& value
        ) const;

        bool updateTokens(const Json& response);

        bool ensureValidToken();

        // JSON parsing
        SpotifyTrack parseTrack(const Json& data) const;

        SpotifyDevice parseDevice(const Json& data) const;

        SpotifyPlayback parsePlayback(
            const Json& data
        ) const;

        // Internal helpers
        std::string playerEndpoint(
            const std::string& path
        ) const;

        void setError(
            const std::string& message,
            long statusCode = 0
        );
    };
}
