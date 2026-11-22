#include "BlitzDJ.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

namespace blitzdj
{
    BlitzDJ::BlitzDJ(const BlitzDJConfig& config)
        : config_(config),
          spotify_(config.clientId, config.redirectUri),
          ai_()
    {
        BlitzAIConfig aiConfig;
        aiConfig.avoidExplicit = config.avoidExplicit;
        aiConfig.avoidRepeats = true;

        ai_.setConfig(aiConfig);

        autoDJEnabled_.store(config.autoDJ);
        spotify_.enableAutoDJ(config.autoDJ);
    }

    BlitzDJ::~BlitzDJ()
    {
        stop();
    }

    bool BlitzDJ::initialize()
    {
        return spotify_.initialize();
    }

    std::string BlitzDJ::getAuthorizationUrl()
    {
        return spotify_.generateAuthorizationUrl();
    }

    bool BlitzDJ::authenticate(
        const std::string& authorizationCode,
        const std::string& state
    )
    {
        return spotify_.authenticate(
            authorizationCode,
            state
        );
    }

    bool BlitzDJ::start()
    {
        if (running_.load())
        {
            return true;
        }

        if (!spotify_.isAuthenticated())
        {
            return false;
        }

        if (config_.autoPlay)
        {
            if (!spotify_.play())
            {
                return false;
            }
        }

        running_.store(true);

        try
        {
            worker_ = std::thread(
                &BlitzDJ::runLoop,
                this
            );
        }
        catch (...)
        {
            running_.store(false);
            return false;
        }

        return true;
    }

    void BlitzDJ::stop()
    {
        running_.store(false);

        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    void BlitzDJ::runLoop()
    {
        while (running_.load())
        {
            update();

            const int interval = std::max(
                250,
                config_.updateIntervalMs
            );

            int elapsed = 0;

            while (running_.load() && elapsed < interval)
            {
                const int step = std::min(
                    250,
                    interval - elapsed
                );

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(step)
                );

                elapsed += step;
            }
        }
    }

    void BlitzDJ::update()
    {
        if (!spotify_.isAuthenticated())
        {
            return;
        }

        if (!autoDJEnabled_.load())
        {
            return;
        }

        updateAutoDJ();
    }

    bool BlitzDJ::updateAutoDJ()
    {
        auto playback = spotify_.getPlayback();

        if (!playback.has_value())
        {
            return false;
        }

        observePlayback(*playback);

        if (!playback->playing)
        {
            return false;
        }

        const int remainingMs = std::max(
            0,
            playback->durationMs - playback->progressMs
        );

        if (remainingMs > config_.queueThresholdMs)
        {
            return true;
        }

        if (playback->track.id.empty())
        {
            return false;
        }

        if (lastQueuedTrackId_ == playback->track.id)
        {
            return true;
        }

        if (!queueNextRecommendedTrack())
        {
            return false;
        }

        lastQueuedTrackId_ = playback->track.id;
        return true;
    }

    void BlitzDJ::observePlayback(
        const SpotifyPlayback& playback
    )
    {
        if (playback.track.id.empty())
        {
            return;
        }

        if (lastObservedTrackId_ == playback.track.id)
        {
            return;
        }

        lastObservedTrackId_ = playback.track.id;
        lastQueuedTrackId_.clear();

        ai_.recordPlayed(playback.track);
    }

    std::vector<SpotifyTrack> BlitzDJ::fetchCandidates()
    {
        const int limit = std::clamp(
            config_.searchLimit,
            1,
            50
        );

        return spotify_.searchTracks(
            config_.searchQuery,
            limit
        );
    }

    bool BlitzDJ::queueNextRecommendedTrack()
    {
        const auto candidates = fetchCandidates();

        if (candidates.empty())
        {
            return false;
        }

        const auto selected = ai_.selectTracks(
            candidates,
            1
        );

        if (selected.empty())
        {
            return false;
        }

        return spotify_.queueAutomaticTrack(
            selected.front()
        );
    }

    bool BlitzDJ::initializeQueue()
    {
        return queueRecommendedTracks(
            config_.initialQueueSize
        );
    }

    bool BlitzDJ::queueRecommendedTracks(
        std::size_t count
    )
    {
        if (!spotify_.isAuthenticated() || count == 0)
        {
            return false;
        }

        const auto candidates = fetchCandidates();

        const auto selected = ai_.selectTracks(
            candidates,
            count
        );

        if (selected.empty())
        {
            return false;
        }

        bool success = true;

        for (const auto& track : selected)
        {
            if (!spotify_.addToQueue(track.uri))
            {
                success = false;
                break;
            }
        }

        return success;
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

    bool BlitzDJ::playTrack(
        const std::string& trackUri
    )
    {
        lastQueuedTrackId_.clear();
        return spotify_.playTrack(trackUri);
    }

    bool BlitzDJ::searchAndPlay(
        const std::string& query
    )
    {
        const auto tracks = spotify_.searchTracks(
            query,
            20
        );

        if (tracks.empty())
        {
            return false;
        }

        const auto selected = ai_.selectTracks(
            tracks,
            1
        );

        if (selected.empty())
        {
            return false;
        }

        return playTrack(selected.front().uri);
    }

    bool BlitzDJ::queueTrack(
        const std::string& trackUri
    )
    {
        return spotify_.addToQueue(trackUri);
    }

    void BlitzDJ::setSearchQuery(
        const std::string& query
    )
    {
        if (!query.empty())
        {
            config_.searchQuery = query;
            lastQueuedTrackId_.clear();
        }
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

    std::vector<SpotifyTrack> BlitzDJ::search(
        const std::string& query,
        int limit
    )
    {
        return spotify_.searchTracks(query, limit);
    }

    std::vector<SpotifyTrack> BlitzDJ::recommend(
        const std::string& query,
        std::size_t count
    )
    {
        const auto candidates = spotify_.searchTracks(
            query,
            50
        );

        return ai_.selectTracks(candidates, count);
    }

    std::optional<SpotifyPlayback> BlitzDJ::getPlayback()
    {
        return spotify_.getPlayback();
    }

    BlitzSpotify& BlitzDJ::spotify()
    {
        return spotify_;
    }

    BlitzAI& BlitzDJ::ai()
    {
        return ai_;
    }

    const BlitzDJConfig& BlitzDJ::getConfig() const
    {
        return config_;
    }
}
