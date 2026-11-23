#include "BlitzDJBridge.h"
#include "BlitzDJ.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

using blitzdj::BlitzDJ;
using blitzdj::BlitzDJConfig;
using blitzdj::Json;
using blitzdj::SpotifyTrack;

struct blitzdj_handle
{
    std::unique_ptr<BlitzDJ> dj;
};

namespace
{
    std::string safe(const char* value)
    {
        return value ? std::string(value) : std::string();
    }

    char* duplicate(const std::string& value)
    {
        auto* output = static_cast<char*>(std::malloc(value.size() + 1));

        if (output)
        {
            std::memcpy(output, value.c_str(), value.size() + 1);
        }

        return output;
    }

    Json trackJson(const SpotifyTrack& track)
    {
        return {
            {"id", track.id},
            {"uri", track.uri},
            {"name", track.name},
            {"artist", track.artist},
            {"album", track.album},
            {"image_url", track.imageUrl},
            {"duration_ms", track.durationMs},
            {"explicit", track.explicitContent}
        };
    }

    Json tracksJson(const std::vector<SpotifyTrack>& tracks)
    {
        Json array = Json::array();

        for (const auto& track : tracks)
        {
            array.push_back(trackJson(track));
        }

        return array;
    }

    template <typename Function>
    auto guarded(blitzdj_handle* handle, Function function, decltype(function(*handle->dj)) fallback)
    {
        if (!handle || !handle->dj)
        {
            return fallback;
        }

        try
        {
            return function(*handle->dj);
        }
        catch (...)
        {
            return fallback;
        }
    }
}

extern "C"
{
    blitzdj_handle* blitzdj_create(const char* client_id, const char* redirect_uri, const char* session_path)
    {
        try
        {
            BlitzDJConfig config;
            config.clientId = safe(client_id);
            config.sessionPath = safe(session_path);
            config.autoDJ = true;

            const auto redirect = safe(redirect_uri);

            if (!redirect.empty())
            {
                config.redirectUri = redirect;
            }

            auto* handle = new blitzdj_handle;
            handle->dj = std::make_unique<BlitzDJ>(config);
            handle->dj->initialize();
            return handle;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void blitzdj_destroy(blitzdj_handle* handle)
    {
        delete handle;
    }

    const char* blitzdj_version(void)
    {
        return BLITZDJ_VERSION;
    }

    char* blitzdj_authorization_url(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj) { return duplicate(dj.getAuthorizationUrl()); }, nullptr);
    }

    int blitzdj_handle_redirect(blitzdj_handle* handle, const char* redirect_url)
    {
        const auto url = safe(redirect_url);
        return guarded(handle, [&](BlitzDJ& dj) { return dj.handleRedirect(url) ? 1 : 0; }, 0);
    }

    int blitzdj_is_authenticated(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj) { return dj.isAuthenticated() ? 1 : 0; }, 0);
    }

    void blitzdj_logout(blitzdj_handle* handle)
    {
        guarded(handle, [](BlitzDJ& dj) { dj.logout(); return 0; }, 0);
    }

    int blitzdj_play(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj) { return dj.play() ? 1 : 0; }, 0);
    }

    int blitzdj_pause(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj) { return dj.pause() ? 1 : 0; }, 0);
    }

    int blitzdj_next(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj) { return dj.skip() ? 1 : 0; }, 0);
    }

    int blitzdj_previous(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj) { return dj.previous() ? 1 : 0; }, 0);
    }

    int blitzdj_set_volume(blitzdj_handle* handle, int volume)
    {
        return guarded(handle, [=](BlitzDJ& dj) { return dj.spotify().setVolume(volume) ? 1 : 0; }, 0);
    }

    int blitzdj_seek(blitzdj_handle* handle, int position_ms)
    {
        return guarded(handle, [=](BlitzDJ& dj) { return dj.spotify().seek(position_ms) ? 1 : 0; }, 0);
    }

    int blitzdj_set_shuffle(blitzdj_handle* handle, int enabled)
    {
        return guarded(handle, [=](BlitzDJ& dj) { return dj.spotify().setShuffle(enabled != 0) ? 1 : 0; }, 0);
    }

    char* blitzdj_playback_json(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj)
        {
            const auto playback = dj.getPlayback();

            if (!playback)
            {
                return duplicate("{\"active\":false}");
            }

            const Json data = {
                {"active", true},
                {"playing", playback->playing},
                {"shuffle", playback->shuffle},
                {"repeat", playback->repeatMode},
                {"progress_ms", playback->progressMs},
                {"duration_ms", playback->durationMs},
                {"track", trackJson(playback->track)},
                {"device", {
                    {"id", playback->device.id},
                    {"name", playback->device.name},
                    {"type", playback->device.type},
                    {"volume", playback->device.volume}
                }}
            };

            return duplicate(data.dump());
        }, nullptr);
    }

    char* blitzdj_devices_json(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj)
        {
            Json array = Json::array();

            for (const auto& device : dj.spotify().getDevices())
            {
                array.push_back({
                    {"id", device.id},
                    {"name", device.name},
                    {"type", device.type},
                    {"active", device.active},
                    {"restricted", device.restricted},
                    {"volume", device.volume}
                });
            }

            return duplicate(array.dump());
        }, nullptr);
    }

    char* blitzdj_search_json(blitzdj_handle* handle, const char* query, int limit)
    {
        const auto text = safe(query);
        return guarded(handle, [&](BlitzDJ& dj) { return duplicate(tracksJson(dj.search(text, limit)).dump()); }, nullptr);
    }

    char* blitzdj_recommend_json(blitzdj_handle* handle, const char* query, int count)
    {
        const auto text = safe(query);
        const auto amount = static_cast<std::size_t>(count > 0 ? count : 10);
        return guarded(handle, [&](BlitzDJ& dj) { return duplicate(tracksJson(dj.recommend(text, amount)).dump()); }, nullptr);
    }

    int blitzdj_play_track(blitzdj_handle* handle, const char* track_uri)
    {
        const auto uri = safe(track_uri);
        return guarded(handle, [&](BlitzDJ& dj) { return dj.playTrack(uri) ? 1 : 0; }, 0);
    }

    int blitzdj_queue_track(blitzdj_handle* handle, const char* track_uri)
    {
        const auto uri = safe(track_uri);
        return guarded(handle, [&](BlitzDJ& dj) { return dj.queueTrack(uri) ? 1 : 0; }, 0);
    }

    int blitzdj_transfer_playback(blitzdj_handle* handle, const char* device_id)
    {
        const auto id = safe(device_id);
        return guarded(handle, [&](BlitzDJ& dj) { return dj.spotify().transferPlayback(id) ? 1 : 0; }, 0);
    }

    int blitzdj_start_auto_dj(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj)
        {
            dj.setAutoDJ(true);
            return dj.start() ? 1 : 0;
        }, 0);
    }

    void blitzdj_stop_auto_dj(blitzdj_handle* handle)
    {
        guarded(handle, [](BlitzDJ& dj) { dj.stop(); return 0; }, 0);
    }

    int blitzdj_is_auto_dj_running(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj) { return dj.isRunning() ? 1 : 0; }, 0);
    }

    void blitzdj_set_search_query(blitzdj_handle* handle, const char* query)
    {
        const auto text = safe(query);
        guarded(handle, [&](BlitzDJ& dj) { dj.setSearchQuery(text); return 0; }, 0);
    }

    char* blitzdj_search_query(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj) { return duplicate(dj.getSearchQuery()); }, nullptr);
    }

    int blitzdj_queue_recommendations(blitzdj_handle* handle, int count)
    {
        const auto amount = static_cast<std::size_t>(count > 0 ? count : 1);
        return guarded(handle, [=](BlitzDJ& dj) { return dj.queueRecommendedTracks(amount) ? 1 : 0; }, 0);
    }

    void blitzdj_add_favorite_artist(blitzdj_handle* handle, const char* artist)
    {
        const auto name = safe(artist);
        guarded(handle, [&](BlitzDJ& dj) { dj.ai().addFavoriteArtist(name); return 0; }, 0);
    }

    void blitzdj_add_favorite_track(blitzdj_handle* handle, const char* track_id)
    {
        const auto id = safe(track_id);
        guarded(handle, [&](BlitzDJ& dj) { dj.ai().addFavoriteTrack(id); return 0; }, 0);
    }

    char* blitzdj_last_error(blitzdj_handle* handle)
    {
        return guarded(handle, [](BlitzDJ& dj) { return duplicate(dj.getLastError()); }, nullptr);
    }

    void blitzdj_free_string(char* value)
    {
        std::free(value);
    }
}
