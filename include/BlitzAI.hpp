#pragma once

#include "BlitzSpotify.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace blitzdj
{
    struct BlitzAIConfig
    {
        std::size_t historyLimit = 50;
        std::size_t candidateLimit = 100;

        double favoriteArtistWeight = 3.0;
        double favoriteTrackWeight = 5.0;
        double diversityWeight = 2.0;
        double explorationWeight = 1.0;

        bool avoidExplicit = false;
        bool avoidRepeats = true;
        bool preferShortTracks = false;
    };

    class BlitzAI
    {
    public:
        explicit BlitzAI(
            BlitzAIConfig config = BlitzAIConfig{}
        );

        void setConfig(const BlitzAIConfig& config);
        BlitzAIConfig getConfig() const;

        void setFavoriteArtists(
            const std::vector<std::string>& artists
        );

        void setFavoriteTracks(
            const std::vector<std::string>& trackIds
        );

        std::vector<std::string> getFavoriteArtists() const;
        std::vector<std::string> getFavoriteTracks() const;

        void addFavoriteArtist(const std::string& artist);
        void addFavoriteTrack(const std::string& trackId);

        void removeFavoriteArtist(const std::string& artist);
        void removeFavoriteTrack(const std::string& trackId);

        void recordPlayed(const SpotifyTrack& track);
        void markQueued(const SpotifyTrack& track);
        void clearHistory();

        bool hasPlayed(const std::string& trackId) const;
        bool isQueued(const std::string& trackId) const;

        double scoreTrack(const SpotifyTrack& track) const;

        std::vector<SpotifyTrack> rankTracks(
            const std::vector<SpotifyTrack>& candidates
        ) const;

        std::vector<SpotifyTrack> selectTracks(
            const std::vector<SpotifyTrack>& candidates,
            std::size_t count
        );

        bool queueNext(
            BlitzSpotify& spotify,
            const std::vector<SpotifyTrack>& candidates
        );

        bool queueNextFromSearch(
            BlitzSpotify& spotify,
            const std::string& query
        );

        void setRandomSeed(std::uint32_t seed);

    private:
        BlitzAIConfig config_;

        std::unordered_set<std::string> favoriteArtists_;
        std::unordered_set<std::string> favoriteTracks_;

        std::deque<SpotifyTrack> history_;
        std::unordered_map<std::string, std::size_t> recentTrackCounts_;

        std::deque<std::string> queuedOrder_;
        std::unordered_set<std::string> queuedTracks_;

        std::mt19937 random_;

        mutable std::mutex mutex_;

        static std::string normalize(const std::string& value);

        static std::vector<std::string> splitArtists(const std::string& artists);

        bool isRecentUnlocked(const std::string& trackId) const;

        bool isEligibleUnlocked(const SpotifyTrack& track) const;

        double calculateScoreUnlocked(
            const SpotifyTrack& track,
            const std::unordered_set<std::string>& selectedArtists
        ) const;

        std::vector<SpotifyTrack> rankTracksUnlocked(
            const std::vector<SpotifyTrack>& candidates
        ) const;

        void markQueuedUnlocked(const std::string& trackId);

        void trimHistoryUnlocked();
    };
}
