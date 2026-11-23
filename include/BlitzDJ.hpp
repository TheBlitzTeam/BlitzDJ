#pragma once

#include "BlitzAI.hpp"
#include "BlitzSpotify.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
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

        std::string sessionPath;

        std::string searchQuery = "electronic music";

        int searchLimit = 30;
        int searchOffsetRange = 100;
        int updateIntervalMs = 5000;
        int queueThresholdMs = 30000;

        std::size_t initialQueueSize = 0;

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

        bool handleRedirect(const std::string& redirectUrl);

        bool isAuthenticated() const;

        void logout();

        bool start();
        void stop();

        void update();

        bool play();
        bool pause();
        bool resume();
        bool skip();
        bool previous();

        bool playTrack(const std::string& trackUri);

        bool searchAndPlay(const std::string& query);

        bool queueTrack(const std::string& trackUri);

        bool queueRecommendedTracks(std::size_t count);

        void setSearchQuery(const std::string& query);
        std::string getSearchQuery() const;

        void setAutoDJ(bool enabled);
        bool isAutoDJEnabled() const;

        bool isRunning() const;

        std::vector<SpotifyTrack> search(
            const std::string& query,
            int limit = BlitzSpotify::SEARCH_PAGE_LIMIT
        );

        std::vector<SpotifyTrack> recommend(
            const std::string& query,
            std::size_t count = 10
        );

        std::optional<SpotifyPlayback> getPlayback();

        std::string getLastError() const;

        BlitzSpotify& spotify();
        BlitzAI& ai();

        BlitzDJConfig getConfig() const;

    private:
        BlitzDJConfig config_;

        BlitzSpotify spotify_;
        BlitzAI ai_;

        std::atomic<bool> running_{false};
        std::atomic<bool> autoDJEnabled_{false};

        std::thread worker_;
        std::mutex workerMutex_;
        std::condition_variable wakeup_;

        mutable std::mutex stateMutex_;

        std::string lastQueuedTrackId_;
        std::string lastObservedTrackId_;
        std::string lastError_;

        std::mt19937 random_;

        void runLoop();

        bool updateAutoDJ();

        std::vector<SpotifyTrack> fetchCandidates(
            const std::string& query,
            int limit
        );

        bool queueNextRecommendedTrack();

        void observePlayback(
            const SpotifyPlayback& playback
        );

        void setError(const std::string& message);
    };
}
