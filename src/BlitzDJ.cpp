#include "BlitzDJ.hpp"

#include <algorithm>
#include <chrono>

namespace blitzdj
{
    namespace
    {
        using Lock = std::lock_guard<std::mutex>;
    }

    BlitzDJ::BlitzDJ(const BlitzDJConfig& config)
        : config_(config),
          spotify_(config.clientId, config.redirectUri),
          random_(std::random_device{}())
    {
        BlitzAIConfig aiConfig;
        aiConfig.avoidExplicit = config.avoidExplicit;
        aiConfig.avoidRepeats = true;

        ai_.setConfig(aiConfig);

        autoDJEnabled_.store(config.autoDJ);
        spotify_.enableAutoDJ(config.autoDJ);
        spotify_.setSessionPath(config.sessionPath);
    }

    BlitzDJ::~BlitzDJ()
    {
        stop();
    }

    bool BlitzDJ::initialize()
    {
        if (!spotify_.initialize())
        {
            return false;
        }

        spotify_.loadSession();
        return true;
    }

    std::string BlitzDJ::getAuthorizationUrl()
    {
        return spotify_.generateAuthorizationUrl();
    }

    bool BlitzDJ::authenticate(const std::string& authorizationCode, const std::string& state)
    {
        return spotify_.authenticate(authorizationCode, state);
    }

    bool BlitzDJ::handleRedirect(const std::string& redirectUrl)
    {
        return spotify_.handleRedirect(redirectUrl);
    }

    bool BlitzDJ::isAuthenticated() const
    {
        return spotify_.isAuthenticated();
    }

    void BlitzDJ::logout()
    {
        stop();
        spotify_.logout();
        ai_.clearHistory();

        Lock lock(stateMutex_);
        lastQueuedTrackId_.clear();
        lastObservedTrackId_.clear();
    }

    bool BlitzDJ::start()
    {
        if (running_.load())
        {
            return true;
        }

        if (!spotify_.isAuthenticated())
        {
            setError("Spotify is not connected.");
            return false;
        }

        if (config_.autoPlay && !spotify_.play())
        {
            setError(spotify_.getLastError());
            return false;
        }

        if (autoDJEnabled_.load() && config_.initialQueueSize > 0)
        {
            queueRecommendedTracks(config_.initialQueueSize);
        }

        if (worker_.joinable())
        {
            worker_.join();
        }

        running_.store(true);

        try
        {
            worker_ = std::thread(&BlitzDJ::runLoop, this);
        }
        catch (...)
        {
            running_.store(false);
            setError("Could not start the AutoDJ worker.");
            return false;
        }

        return true;
    }

    void BlitzDJ::stop()
    {
        {
            std::lock_guard<std::mutex> lock(workerMutex_);
            running_.store(false);
        }

        wakeup_.notify_all();

        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id())
        {
            worker_.join();
        }
    }

    void BlitzDJ::runLoop()
    {
        while (running_.load())
        {
            update();

            const int interval = std::max(250, config_.updateIntervalMs);

            std::unique_lock<std::mutex> lock(workerMutex_);
            wakeup_.wait_for(lock, std::chrono::milliseconds(interval), [this]
            {
                return !running_.load();
            });
        }
    }

    void BlitzDJ::update()
    {
        if (!autoDJEnabled_.load() || !spotify_.isAuthenticated())
        {
            return;
        }

        updateAutoDJ();
    }

    bool BlitzDJ::updateAutoDJ()
    {
        const auto playback = spotify_.getPlayback();

        if (!playback)
        {
            return false;
        }

        observePlayback(*playback);

        if (!playback->playing || playback->track.id.empty())
        {
            return false;
        }

        const int remainingMs = std::max(0, playback->durationMs - playback->progressMs);

        if (remainingMs > config_.queueThresholdMs)
        {
            return true;
        }

        {
            Lock lock(stateMutex_);

            if (lastQueuedTrackId_ == playback->track.id)
            {
                return true;
            }
        }

        if (!queueNextRecommendedTrack())
        {
            return false;
        }

        Lock lock(stateMutex_);
        lastQueuedTrackId_ = playback->track.id;
        return true;
    }

    void BlitzDJ::observePlayback(const SpotifyPlayback& playback)
    {
        if (playback.track.id.empty())
        {
            return;
        }

        {
            Lock lock(stateMutex_);

            if (lastObservedTrackId_ == playback.track.id)
            {
                return;
            }

            lastObservedTrackId_ = playback.track.id;
            lastQueuedTrackId_.clear();
        }

        ai_.recordPlayed(playback.track);
    }

    std::vector<SpotifyTrack> BlitzDJ::fetchCandidates(const std::string& query, int limit)
    {
        std::vector<SpotifyTrack> candidates;

        if (query.empty())
        {
            return candidates;
        }

        const int pageSize = BlitzSpotify::SEARCH_PAGE_LIMIT;
        const int pages = std::clamp((limit + pageSize - 1) / pageSize, 1, 5);
        const int range = std::clamp(config_.searchOffsetRange, 0, BlitzSpotify::SEARCH_MAX_OFFSET - pages * pageSize);

        int offset = 0;

        if (range > 0)
        {
            Lock lock(stateMutex_);
            std::uniform_int_distribution<int> distribution(0, range / pageSize);
            offset = distribution(random_) * pageSize;
        }

        for (int page = 0; page < pages; ++page)
        {
            auto tracks = spotify_.searchTracks(query, pageSize, offset + page * pageSize);

            if (tracks.empty())
            {
                break;
            }

            candidates.insert(candidates.end(), tracks.begin(), tracks.end());

            if (static_cast<int>(tracks.size()) < pageSize)
            {
                break;
            }
        }

        if (candidates.empty() && offset > 0)
        {
            for (int page = 0; page < pages; ++page)
            {
                auto tracks = spotify_.searchTracks(query, pageSize, page * pageSize);

                if (tracks.empty())
                {
                    break;
                }

                candidates.insert(candidates.end(), tracks.begin(), tracks.end());
            }
        }

        return candidates;
    }

    bool BlitzDJ::queueNextRecommendedTrack()
    {
        const auto candidates = fetchCandidates(getSearchQuery(), config_.searchLimit);

        if (candidates.empty())
        {
            setError("No tracks were found for the AutoDJ search.");
            return false;
        }

        const auto selected = ai_.selectTracks(candidates, 1);

        if (selected.empty())
        {
            setError("Every candidate track was played recently.");
            return false;
        }

        if (!spotify_.queueAutomaticTrack(selected.front()))
        {
            setError(spotify_.getLastError());
            return false;
        }

        ai_.markQueued(selected.front());
        return true;
    }

    bool BlitzDJ::queueRecommendedTracks(std::size_t count)
    {
        if (!spotify_.isAuthenticated() || count == 0)
        {
            setError("Spotify is not connected.");
            return false;
        }

        const int limit = std::max(config_.searchLimit, static_cast<int>(count) * 3);
        const auto selected = ai_.selectTracks(fetchCandidates(getSearchQuery(), limit), count);

        if (selected.empty())
        {
            setError("No new tracks are available to queue.");
            return false;
        }

        for (const auto& track : selected)
        {
            if (!spotify_.addToQueue(track.uri))
            {
                setError(spotify_.getLastError());
                return false;
            }

            ai_.markQueued(track);
        }

        return true;
    }

    bool BlitzDJ::play()
    {
        return spotify_.play();
    }

    bool BlitzDJ::pause()
    {
        return spotify_.pause();
    }

    bool BlitzDJ::resume()
    {
        return spotify_.resume();
    }

    bool BlitzDJ::skip()
    {
        return spotify_.nextTrack();
    }

    bool BlitzDJ::previous()
    {
        return spotify_.previousTrack();
    }

    bool BlitzDJ::playTrack(const std::string& trackUri)
    {
        {
            Lock lock(stateMutex_);
            lastQueuedTrackId_.clear();
        }

        return spotify_.playTrack(trackUri);
    }

    bool BlitzDJ::searchAndPlay(const std::string& query)
    {
        const auto selected = ai_.selectTracks(fetchCandidates(query, BlitzSpotify::SEARCH_PAGE_LIMIT * 2), 1);

        if (selected.empty())
        {
            setError("No tracks were found.");
            return false;
        }

        return playTrack(selected.front().uri);
    }

    bool BlitzDJ::queueTrack(const std::string& trackUri)
    {
        return spotify_.addToQueue(trackUri);
    }

    void BlitzDJ::setSearchQuery(const std::string& query)
    {
        if (query.empty())
        {
            return;
        }

        Lock lock(stateMutex_);
        config_.searchQuery = query;
        lastQueuedTrackId_.clear();
    }

    std::string BlitzDJ::getSearchQuery() const
    {
        Lock lock(stateMutex_);
        return config_.searchQuery;
    }

    void BlitzDJ::setAutoDJ(bool enabled)
    {
        autoDJEnabled_.store(enabled);
        spotify_.enableAutoDJ(enabled);
    }

    bool BlitzDJ::isAutoDJEnabled() const
    {
        return autoDJEnabled_.load();
    }

    bool BlitzDJ::isRunning() const
    {
        return running_.load();
    }

    std::vector<SpotifyTrack> BlitzDJ::search(const std::string& query, int limit)
    {
        return spotify_.searchTracks(query, limit);
    }

    std::vector<SpotifyTrack> BlitzDJ::recommend(const std::string& query, std::size_t count)
    {
        const int limit = std::max(config_.searchLimit, static_cast<int>(count) * 3);
        return ai_.selectTracks(fetchCandidates(query, limit), count);
    }

    std::optional<SpotifyPlayback> BlitzDJ::getPlayback()
    {
        return spotify_.getPlayback();
    }

    std::string BlitzDJ::getLastError() const
    {
        const auto spotifyError = spotify_.getLastError();

        if (!spotifyError.empty())
        {
            return spotifyError;
        }

        Lock lock(stateMutex_);
        return lastError_;
    }

    void BlitzDJ::setError(const std::string& message)
    {
        Lock lock(stateMutex_);
        lastError_ = message;
    }

    BlitzSpotify& BlitzDJ::spotify()
    {
        return spotify_;
    }

    BlitzAI& BlitzDJ::ai()
    {
        return ai_;
    }

    BlitzDJConfig BlitzDJ::getConfig() const
    {
        Lock lock(stateMutex_);
        return config_;
    }
}
