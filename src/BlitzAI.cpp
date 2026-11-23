#include "BlitzAI.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>

namespace blitzdj
{
    namespace
    {
        using Lock = std::lock_guard<std::mutex>;
    }

    BlitzAI::BlitzAI(BlitzAIConfig config)
        : config_(std::move(config)),
          random_(std::random_device{}())
    {
    }

    void BlitzAI::setConfig(const BlitzAIConfig& config)
    {
        Lock lock(mutex_);
        config_ = config;
        trimHistoryUnlocked();
    }

    BlitzAIConfig BlitzAI::getConfig() const
    {
        Lock lock(mutex_);
        return config_;
    }

    std::string BlitzAI::normalize(const std::string& value)
    {
        std::size_t start = 0;
        std::size_t end = value.size();

        while (start < end && std::isspace(static_cast<unsigned char>(value[start])))
        {
            ++start;
        }

        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
        {
            --end;
        }

        std::string result;
        result.reserve(end - start);

        for (std::size_t i = start; i < end; ++i)
        {
            result.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(value[i]))
            ));
        }

        return result;
    }

    std::vector<std::string> BlitzAI::splitArtists(const std::string& artists)
    {
        std::vector<std::string> result;
        std::size_t position = 0;

        while (position <= artists.size())
        {
            auto end = artists.find(',', position);

            if (end == std::string::npos)
            {
                end = artists.size();
            }

            auto name = normalize(artists.substr(position, end - position));

            if (!name.empty())
            {
                result.push_back(std::move(name));
            }

            position = end + 1;
        }

        return result;
    }

    void BlitzAI::setFavoriteArtists(const std::vector<std::string>& artists)
    {
        Lock lock(mutex_);
        favoriteArtists_.clear();

        for (const auto& artist : artists)
        {
            auto name = normalize(artist);

            if (!name.empty())
            {
                favoriteArtists_.insert(std::move(name));
            }
        }
    }

    void BlitzAI::setFavoriteTracks(const std::vector<std::string>& trackIds)
    {
        Lock lock(mutex_);
        favoriteTracks_.clear();

        for (const auto& id : trackIds)
        {
            if (!id.empty())
            {
                favoriteTracks_.insert(id);
            }
        }
    }

    std::vector<std::string> BlitzAI::getFavoriteArtists() const
    {
        Lock lock(mutex_);
        std::vector<std::string> result(favoriteArtists_.begin(), favoriteArtists_.end());
        std::sort(result.begin(), result.end());
        return result;
    }

    std::vector<std::string> BlitzAI::getFavoriteTracks() const
    {
        Lock lock(mutex_);
        std::vector<std::string> result(favoriteTracks_.begin(), favoriteTracks_.end());
        std::sort(result.begin(), result.end());
        return result;
    }

    void BlitzAI::addFavoriteArtist(const std::string& artist)
    {
        auto name = normalize(artist);

        if (name.empty())
        {
            return;
        }

        Lock lock(mutex_);
        favoriteArtists_.insert(std::move(name));
    }

    void BlitzAI::addFavoriteTrack(const std::string& trackId)
    {
        if (trackId.empty())
        {
            return;
        }

        Lock lock(mutex_);
        favoriteTracks_.insert(trackId);
    }

    void BlitzAI::removeFavoriteArtist(const std::string& artist)
    {
        Lock lock(mutex_);
        favoriteArtists_.erase(normalize(artist));
    }

    void BlitzAI::removeFavoriteTrack(const std::string& trackId)
    {
        Lock lock(mutex_);
        favoriteTracks_.erase(trackId);
    }

    void BlitzAI::trimHistoryUnlocked()
    {
        while (history_.size() > config_.historyLimit)
        {
            auto found = recentTrackCounts_.find(history_.front().id);

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

        while (queuedOrder_.size() > config_.historyLimit)
        {
            queuedTracks_.erase(queuedOrder_.front());
            queuedOrder_.pop_front();
        }
    }

    void BlitzAI::recordPlayed(const SpotifyTrack& track)
    {
        if (track.id.empty())
        {
            return;
        }

        Lock lock(mutex_);

        history_.push_back(track);
        ++recentTrackCounts_[track.id];

        if (queuedTracks_.erase(track.id) != 0)
        {
            queuedOrder_.erase(
                std::remove(queuedOrder_.begin(), queuedOrder_.end(), track.id),
                queuedOrder_.end()
            );
        }

        trimHistoryUnlocked();
    }

    void BlitzAI::markQueuedUnlocked(const std::string& trackId)
    {
        if (trackId.empty() || !queuedTracks_.insert(trackId).second)
        {
            return;
        }

        queuedOrder_.push_back(trackId);
        trimHistoryUnlocked();
    }

    void BlitzAI::markQueued(const SpotifyTrack& track)
    {
        Lock lock(mutex_);
        markQueuedUnlocked(track.id);
    }

    void BlitzAI::clearHistory()
    {
        Lock lock(mutex_);
        history_.clear();
        recentTrackCounts_.clear();
        queuedOrder_.clear();
        queuedTracks_.clear();
    }

    bool BlitzAI::hasPlayed(const std::string& trackId) const
    {
        Lock lock(mutex_);
        return recentTrackCounts_.count(trackId) != 0;
    }

    bool BlitzAI::isQueued(const std::string& trackId) const
    {
        Lock lock(mutex_);
        return queuedTracks_.count(trackId) != 0;
    }

    bool BlitzAI::isRecentUnlocked(const std::string& trackId) const
    {
        return recentTrackCounts_.count(trackId) != 0 ||
            queuedTracks_.count(trackId) != 0;
    }

    bool BlitzAI::isEligibleUnlocked(const SpotifyTrack& track) const
    {
        if (track.id.empty() || track.uri.empty() || !track.playable)
        {
            return false;
        }

        if (config_.avoidExplicit && track.explicitContent)
        {
            return false;
        }

        if (config_.avoidRepeats && isRecentUnlocked(track.id))
        {
            return false;
        }

        return true;
    }

    double BlitzAI::calculateScoreUnlocked(
        const SpotifyTrack& track,
        const std::unordered_set<std::string>& selectedArtists
    ) const
    {
        double score = 1.0;

        if (favoriteTracks_.count(track.id) != 0)
        {
            score += config_.favoriteTrackWeight;
        }

        const auto artists = splitArtists(track.artist);

        const bool favoriteArtist = std::any_of(artists.begin(), artists.end(),
            [this](const std::string& artist)
            {
                return favoriteArtists_.count(artist) != 0;
            });

        if (favoriteArtist)
        {
            score += config_.favoriteArtistWeight;
        }

        const bool repeatedArtist = std::any_of(artists.begin(), artists.end(),
            [&selectedArtists](const std::string& artist)
            {
                return selectedArtists.count(artist) != 0;
            });

        if (!repeatedArtist)
        {
            score += config_.diversityWeight;
        }

        if (config_.preferShortTracks && track.durationMs > 0)
        {
            const double minutes = static_cast<double>(track.durationMs) / 60000.0;
            score += 1.0 / (1.0 + minutes);
        }

        if (isRecentUnlocked(track.id))
        {
            score -= 4.0;
        }

        return score;
    }

    double BlitzAI::scoreTrack(const SpotifyTrack& track) const
    {
        Lock lock(mutex_);

        if (!isEligibleUnlocked(track))
        {
            return -std::numeric_limits<double>::infinity();
        }

        return calculateScoreUnlocked(track, {});
    }

    std::vector<SpotifyTrack> BlitzAI::rankTracksUnlocked(
        const std::vector<SpotifyTrack>& candidates
    ) const
    {
        std::vector<std::pair<double, SpotifyTrack>> scored;
        std::unordered_set<std::string> seen;

        for (const auto& track : candidates)
        {
            if (scored.size() >= config_.candidateLimit)
            {
                break;
            }

            if (!isEligibleUnlocked(track) || !seen.insert(track.id).second)
            {
                continue;
            }

            scored.emplace_back(calculateScoreUnlocked(track, {}), track);
        }

        std::stable_sort(scored.begin(), scored.end(),
            [](const auto& a, const auto& b)
            {
                return a.first > b.first;
            });

        std::vector<SpotifyTrack> ranked;
        ranked.reserve(scored.size());

        for (auto& entry : scored)
        {
            ranked.push_back(std::move(entry.second));
        }

        return ranked;
    }

    std::vector<SpotifyTrack> BlitzAI::rankTracks(
        const std::vector<SpotifyTrack>& candidates
    ) const
    {
        Lock lock(mutex_);
        return rankTracksUnlocked(candidates);
    }

    std::vector<SpotifyTrack> BlitzAI::selectTracks(
        const std::vector<SpotifyTrack>& candidates,
        std::size_t count
    )
    {
        Lock lock(mutex_);

        std::vector<SpotifyTrack> pool = rankTracksUnlocked(candidates);
        std::vector<SpotifyTrack> selected;
        std::unordered_set<std::string> selectedArtists;

        count = std::min(count, pool.size());
        selected.reserve(count);

        const double exploration = std::max(0.0, config_.explorationWeight);
        std::uniform_real_distribution<double> noise(0.0, exploration > 0.0 ? exploration : 1.0);

        while (!pool.empty() && selected.size() < count)
        {
            std::size_t bestIndex = 0;
            double bestScore = -std::numeric_limits<double>::infinity();

            for (std::size_t i = 0; i < pool.size(); ++i)
            {
                double score = calculateScoreUnlocked(pool[i], selectedArtists);

                if (exploration > 0.0)
                {
                    score += noise(random_);
                }

                if (score > bestScore)
                {
                    bestScore = score;
                    bestIndex = i;
                }
            }

            SpotifyTrack chosen = std::move(pool[bestIndex]);
            pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(bestIndex));

            for (auto& artist : splitArtists(chosen.artist))
            {
                selectedArtists.insert(std::move(artist));
            }

            selected.push_back(std::move(chosen));
        }

        return selected;
    }

    bool BlitzAI::queueNext(
        BlitzSpotify& spotify,
        const std::vector<SpotifyTrack>& candidates
    )
    {
        if (!spotify.isAuthenticated() || !spotify.isAutoDJEnabled())
        {
            return false;
        }

        const auto selected = selectTracks(candidates, 1);

        if (selected.empty() || !spotify.queueAutomaticTrack(selected.front()))
        {
            return false;
        }

        markQueued(selected.front());
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

        return queueNext(spotify, spotify.searchTracks(query));
    }

    void BlitzAI::setRandomSeed(std::uint32_t seed)
    {
        Lock lock(mutex_);
        random_.seed(seed);
    }
}
