#include "BlitzAI.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <random>
#include <utility>

namespace blitzdj
{
    BlitzAI::BlitzAI(BlitzAIConfig config)
        : config_(std::move(config)),
          random_(std::random_device{}())
    {
    }

    void BlitzAI::setConfig(const BlitzAIConfig& config)
    {
        config_ = config;
        trimHistory();
    }

    const BlitzAIConfig& BlitzAI::getConfig() const
    {
        return config_;
    }

    std::string BlitzAI::normalize(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());

        for (unsigned char character : value)
        {
            result.push_back(static_cast<char>(
                std::tolower(character)
            ));
        }

        return result;
    }

    void BlitzAI::setFavoriteArtists(
        const std::vector<std::string>& artists
    )
    {
        favoriteArtists_.clear();

        for (const auto& artist : artists)
        {
            addFavoriteArtist(artist);
        }
    }

    void BlitzAI::setFavoriteTracks(
        const std::vector<std::string>& trackIds
    )
    {
        favoriteTracks_.clear();

        for (const auto& id : trackIds)
        {
            addFavoriteTrack(id);
        }
    }

    void BlitzAI::addFavoriteArtist(
        const std::string& artist
    )
    {
        if (!artist.empty())
        {
            favoriteArtists_.insert(normalize(artist));
        }
    }

    void BlitzAI::addFavoriteTrack(
        const std::string& trackId
    )
    {
        if (!trackId.empty())
        {
            favoriteTracks_.insert(trackId);
        }
    }

    void BlitzAI::removeFavoriteArtist(
        const std::string& artist
    )
    {
        favoriteArtists_.erase(normalize(artist));
    }

    void BlitzAI::removeFavoriteTrack(
        const std::string& trackId
    )
    {
        favoriteTracks_.erase(trackId);
    }

    void BlitzAI::trimHistory()
    {
        while (history_.size() > config_.historyLimit)
        {
            const auto& oldest = history_.front();
            auto found = recentTrackCounts_.find(oldest.id);

            if (found != recentTrackCounts_.end())
            {
                if (found->second <= 1)
                {
                    recentTrackCounts_.erase(found);
                }
                else
                {
                    --found->second;
                }
            }

            history_.pop_front();
        }
    }

    void BlitzAI::recordPlayed(const SpotifyTrack& track)
    {
        if (track.id.empty())
        {
            return;
        }

        history_.push_back(track);
        ++recentTrackCounts_[track.id];

        trimHistory();
    }

    void BlitzAI::clearHistory()
    {
        history_.clear();
        recentTrackCounts_.clear();
    }

    bool BlitzAI::hasPlayed(
        const std::string& trackId
    ) const
    {
        return recentTrackCounts_.find(trackId) !=
               recentTrackCounts_.end();
    }

    bool BlitzAI::isEligible(
        const SpotifyTrack& track
    ) const
    {
        if (track.id.empty() || track.uri.empty())
        {
            return false;
        }

        if (!track.playable)
        {
            return false;
        }

        if (config_.avoidExplicit && track.explicitContent)
        {
            return false;
        }

        if (config_.avoidRepeats && hasPlayed(track.id))
        {
            return false;
        }

        return true;
    }

    double BlitzAI::calculateScore(
        const SpotifyTrack& track,
        const std::unordered_set<std::string>& selectedArtists
    ) const
    {
        double score = 1.0;

        if (favoriteTracks_.count(track.id) != 0)
        {
            score += config_.favoriteTrackWeight;
        }

        const std::string artist = normalize(track.artist);

        if (favoriteArtists_.count(artist) != 0)
        {
            score += config_.favoriteArtistWeight;
        }

        if (selectedArtists.count(artist) == 0)
        {
            score += config_.diversityWeight;
        }

        if (config_.preferShortTracks && track.durationMs > 0)
        {
            const double minutes =
                static_cast<double>(track.durationMs) / 60000.0;

            score += 1.0 / (1.0 + minutes);
        }

        if (hasPlayed(track.id))
        {
            score -= 4.0;
        }

        return score;
    }

    double BlitzAI::scoreTrack(
        const SpotifyTrack& track
    ) const
    {
        if (!isEligible(track))
        {
            return -std::numeric_limits<double>::infinity();
        }

        return calculateScore(track, {});
    }

    std::vector<SpotifyTrack> BlitzAI::rankTracks(
        const std::vector<SpotifyTrack>& candidates
    ) const
    {
        std::vector<SpotifyTrack> ranked;
        std::unordered_set<std::string> seen;

        const std::size_t limit = config_.candidateLimit;

        for (const auto& track : candidates)
        {
            if (ranked.size() >= limit)
            {
                break;
            }

            if (!isEligible(track))
            {
                continue;
            }

            if (!seen.insert(track.id).second)
            {
                continue;
            }

            ranked.push_back(track);
        }

        std::stable_sort(
            ranked.begin(),
            ranked.end(),
            [this](const SpotifyTrack& a, const SpotifyTrack& b)
            {
                return scoreTrack(a) > scoreTrack(b);
            }
        );

        return ranked;
    }

    std::vector<SpotifyTrack> BlitzAI::selectTracks(
        const std::vector<SpotifyTrack>& candidates,
        std::size_t count
    )
    {
        std::vector<SpotifyTrack> pool = rankTracks(candidates);
        std::vector<SpotifyTrack> selected;

        std::unordered_set<std::string> selectedArtists;

        count = std::min(count, pool.size());
        selected.reserve(count);

        while (!pool.empty() && selected.size() < count)
        {
            std::size_t bestIndex = 0;
            double bestScore =
                -std::numeric_limits<double>::infinity();

            for (std::size_t i = 0; i < pool.size(); ++i)
            {
                double score = calculateScore(
                    pool[i],
                    selectedArtists
                );

                const double exploration =
                    std::max(0.0, config_.explorationWeight);

                if (exploration > 0.0)
                {
                    std::uniform_real_distribution<double> distribution(
                        0.0,
                        exploration
                    );

                    score += distribution(random_);
                }

                if (score > bestScore)
                {
                    bestScore = score;
                    bestIndex = i;
                }
            }

            SpotifyTrack chosen = pool[bestIndex];

            selectedArtists.insert(normalize(chosen.artist));
            selected.push_back(std::move(chosen));

            pool.erase(pool.begin() + bestIndex);
        }

        return selected;
    }

    bool BlitzAI::queueNext(
        BlitzSpotify& spotify,
        const std::vector<SpotifyTrack>& candidates
    )
    {
        if (!spotify.isAuthenticated())
        {
            return false;
        }

        if (!spotify.isAutoDJEnabled())
        {
            return false;
        }

        auto selected = selectTracks(candidates, 1);

        if (selected.empty())
        {
            return false;
        }

        const SpotifyTrack& track = selected.front();

        if (!spotify.queueAutomaticTrack(track))
        {
            return false;
        }

        recordPlayed(track);
        return true;
    }

    bool BlitzAI::queueNextFromSearch(
        BlitzSpotify& spotify,
        const std::string& query
    )
    {
        if (query.empty())
        {
            return false;
        }

        const auto tracks = spotify.searchTracks(
            query,
            50
        );

        return queueNext(spotify, tracks);
    }

    void BlitzAI::setRandomSeed(std::uint32_t seed)
    {
        random_.seed(seed);
    }
}
