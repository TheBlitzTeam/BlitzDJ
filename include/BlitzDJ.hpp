#pragma once

#include "BlitzAI.hpp"
#include "BlitzSpotify.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace blitzdj
{
    struct BlitzDJConfig
    {
        std::string clientId;
        std::string redirectUri =
            "http://127.0.0.1:8888/callback";

        std::string searchQuery = "electronic music";

        int searchLimit = 50;
        int updateIntervalMs = 5000;
        int queueThresholdMs = 30000;

        std::size_t initialQueueSize = 3;

        bool autoDJ = true;
        bool autoPlay = false;
        bool avoidExplicit = false;
    };

    class BlitzDJ
    {
    public:
        explicit BlitzDJ(const BlitzDJConfig& config);
        ~BlitzDJ();

        BlitzDJ(const BlitzDJ&) = delete;
        BlitzDJ& operator=(const BlitzDJ&) = delete;

        bool initialize();

        std::string getAuthorizationUrl();

        bool authenticate(
            const std::string& authorizationCode,
            const std::string& state
        );

        bool start();
        void stop();

        void update();

        bool play();
        bool pause();
        bool resume();
        bool skip();

        bool playTrack(const std::string& trackUri);

        bool searchAndPlay(const std::string& query);

        bool queueTrack(const std::string& trackUri);

        bool queueRecommendedTracks(std::size_t count);

        void setSearchQuery(const std::string& query);

        void setAutoDJ(bool enabled);
        bool isAutoDJEnabled() const;

        bool isRunning() const;

        std::vector<SpotifyTrack> search(
            const std::string& query,
            int limit = 20
        );

        std::vector<SpotifyTrack> recommend(
            const std::string& query,
            std::size_t count = 10
        );

        std::optional<SpotifyPlayback> getPlayback();

        BlitzSpotify& spotify();
        BlitzAI& ai();

        const BlitzDJConfig& getConfig() const;

    private:
        BlitzDJConfig config_;

        BlitzSpotify spotify_;
        BlitzAI ai_;

        std::atomic<bool> running_{false};
        std::atomic<bool> autoDJEnabled_{false};

        std::thread worker_;

        std::string lastQueuedTrackId_;
        std::string lastObservedTrackId_;

        void runLoop();

        bool updateAutoDJ();
        bool initializeQueue();

        std::vector<SpotifyTrack> fetchCandidates();

        bool queueNextRecommendedTrack();

        void observePlayback(
            const SpotifyPlayback& playback
        );
    };
}
