#include "BlitzAI.hpp"
#include "BlitzDJBridge.h"
#include "BlitzSpotify.hpp"
#include "Crypto.hpp"
#include "OAuthCallbackServer.hpp"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace blitzdj;

namespace
{
    int failures = 0;

    void check(bool condition, const std::string& name)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAILED: " << name << "\n";
        }
    }

    std::string hex(const std::array<std::uint8_t, 32>& digest)
    {
        std::ostringstream out;

        for (auto byte : digest)
        {
            char buffer[3];
            std::snprintf(buffer, sizeof(buffer), "%02x", byte);
            out << buffer;
        }

        return out.str();
    }

    class FakeHttpClient final : public HttpClient
    {
    public:
        using Handler = std::function<HttpResponse(const HttpRequest&)>;

        explicit FakeHttpClient(std::vector<HttpRequest>& log, Handler handler)
            : log_(log),
              handler_(std::move(handler))
        {
        }

        HttpResponse send(const HttpRequest& request) override
        {
            log_.push_back(request);
            return handler_(request);
        }

    private:
        std::vector<HttpRequest>& log_;
        Handler handler_;
    };

    std::string queryValue(const std::string& url, const std::string& key)
    {
        const auto start = url.find(key + "=");

        if (start == std::string::npos)
        {
            return {};
        }

        const auto valueStart = start + key.size() + 1;
        const auto end = url.find('&', valueStart);
        return BlitzSpotify::urlDecode(url.substr(valueStart, end == std::string::npos ? std::string::npos : end - valueStart));
    }

    void testCrypto()
    {
        check(hex(crypto::sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 empty");
        check(hex(crypto::sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256 abc");
        check(hex(crypto::sha256(std::string(1000000, 'a'))) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "sha256 million");
        check(hex(crypto::sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "sha256 two blocks");

        const std::string text = "BlitzDJ?";
        check(crypto::base64Url(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()) == "QmxpdHpESj8", "base64url");
        check(crypto::randomBytes(32).size() == 32, "random bytes");
    }

    void testAuthenticationFlow()
    {
        const auto sessionPath = (std::filesystem::temp_directory_path() / "blitzdj-test" / "session.json").string();
        std::filesystem::remove_all(std::filesystem::path(sessionPath).parent_path());

        std::vector<HttpRequest> log;
        int playbackCalls = 0;

        auto handler = [&](const HttpRequest& request)
        {
            HttpResponse response;

            if (request.url.find("/api/token") != std::string::npos)
            {
                response.statusCode = 200;
                response.body = R"({"access_token":"access","refresh_token":"refresh","token_type":"Bearer","expires_in":3600})";
            }
            else if (request.url.find("/me/player/next") != std::string::npos)
            {
                ++playbackCalls;
                response.statusCode = playbackCalls == 1 ? 401 : 204;
            }
            else if (request.url.find("/me/player/pause") != std::string::npos)
            {
                response.statusCode = 404;
                response.body = R"({"error":{"status":404,"message":"Player command failed: No active device found","reason":"NO_ACTIVE_DEVICE"}})";
            }
            else if (request.url.find("/me/player/next") == std::string::npos && request.url.find("/me/player/shuffle") != std::string::npos)
            {
                response.statusCode = 403;
                response.body = "Check settings on developer.spotify.com/dashboard, the user may not be registered.";
            }
            else if (request.url.find("/me/player/volume") != std::string::npos)
            {
                response.statusCode = 502;
                response.body = "<html>Bad Gateway</html>";
            }
            else if (request.url.find("/search") != std::string::npos)
            {
                response.statusCode = 200;
                response.body = R"({"tracks":{"items":[
                    {"id":"a","uri":"spotify:track:a","name":"Song A","duration_ms":1000,"artists":[{"name":"Artist One"}],"album":{"name":"X","images":[{"url":"img"}]}},
                    {"id":"b","uri":"spotify:track:b","name":"Song B","duration_ms":2000,"artists":[{"name":"Artist Two"},{"name":"Artist Three"}],"album":{"name":"Y"}}
                ]}})";
            }
            else
            {
                response.statusCode = 404;
                response.body = R"({"error":{"status":404,"message":"Not found"}})";
            }

            return response;
        };

        BlitzSpotify spotify("client", "blitzdj://callback");
        spotify.setHttpClient(std::make_unique<FakeHttpClient>(log, handler));
        spotify.setSessionPath(sessionPath);

        check(spotify.initialize(), "initialize");
        check(!spotify.isAuthenticated(), "not authenticated before login");

        const auto url = spotify.generateAuthorizationUrl();
        check(url.find("code_challenge_method=S256") != std::string::npos, "pkce in url");
        check(url.find("redirect_uri=blitzdj%3A%2F%2Fcallback") != std::string::npos, "redirect encoded");

        const auto state = queryValue(url, "state");
        check(!state.empty(), "state present");

        check(!spotify.handleRedirect("blitzdj://callback?code=abc&state=wrong"), "wrong state rejected");
        check(!spotify.handleRedirect("blitzdj://callback?error=access_denied&state=" + state), "denied rejected");

        BlitzSpotify restored("client", "blitzdj://callback");
        restored.setHttpClient(std::make_unique<FakeHttpClient>(log, handler));
        restored.setSessionPath(sessionPath);
        restored.initialize();
        restored.loadSession();

        check(restored.handleRedirect("blitzdj://callback?code=abc&state=" + BlitzSpotify::urlEncode(state)), "pending login survives restart");
        check(restored.isAuthenticated(), "authenticated after redirect");
        check(log.back().body.find("code_verifier=") != std::string::npos, "verifier sent");

        check(restored.nextTrack(), "401 triggers refresh and retry");
        check(playbackCalls == 2, "request retried once");

        const auto tracks = restored.searchTracks("test query", 50, 20);
        check(tracks.size() == 2, "search parsed");
        check(log.back().url.find("limit=10") != std::string::npos, "search limit clamped");
        check(log.back().url.find("offset=20") != std::string::npos, "search offset sent");
        check(tracks.size() == 2 && tracks[1].artist == "Artist Two, Artist Three", "artists joined");
        check(tracks.size() == 2 && tracks[0].imageUrl == "img", "image parsed");

        BlitzSpotify loaded("client", "blitzdj://callback");
        loaded.setSessionPath(sessionPath);
        check(loaded.loadSession(), "session reload");
        check(loaded.getTokens().refreshToken == "refresh", "refresh token persisted");

        BlitzSpotify otherClient("other", "blitzdj://callback");
        otherClient.setSessionPath(sessionPath);
        check(!otherClient.loadSession(), "session ignored for other client");

        check(!restored.getTrack("missing").has_value(), "404 handled");
        check(restored.getLastError() == "Not found (HTTP 404, GET /tracks/missing)", "api error message");

        std::vector<std::string> logged;
        restored.setErrorLogger([&logged](const std::string& entry) { logged.push_back(entry); });

        check(!restored.pause(), "pause without device fails");
        check(restored.getLastError().rfind("No active Spotify device.", 0) == 0, "no device message");
        check(restored.getLastError().find("PUT /me/player/pause") != std::string::npos, "error has endpoint");

        check(!restored.setShuffle(true), "unregistered user fails");
        check(restored.getLastError().find("User Management") != std::string::npos, "unregistered user message");

        check(!restored.setVolume(40), "volume gateway error fails");
        check(restored.getLastError().find("HTTP 502") != std::string::npos, "error has status");
        check(!logged.empty() && logged.back().find("Bad Gateway") != std::string::npos, "error body logged");

        restored.logout();
        check(!std::filesystem::exists(sessionPath), "logout removes session");
    }

    void testAI()
    {
        BlitzAI ai;
        ai.setRandomSeed(42);
        ai.addFavoriteArtist("  Artist Three ");

        std::vector<SpotifyTrack> candidates;

        for (int i = 0; i < 6; ++i)
        {
            SpotifyTrack track;
            track.id = "t" + std::to_string(i);
            track.uri = "spotify:track:" + track.id;
            track.artist = i == 3 ? "Artist Two, Artist Three" : "Artist " + std::to_string(i % 2);
            candidates.push_back(track);
        }

        candidates.push_back(candidates.front());

        const auto ranked = ai.rankTracks(candidates);
        check(ranked.size() == 6, "duplicates removed");
        check(!ranked.empty() && ranked.front().id == "t3", "favorite featured artist ranked first");

        const auto selected = ai.selectTracks(candidates, 3);
        check(selected.size() == 3, "selected count");

        ai.markQueued(candidates[0]);
        check(ai.isQueued("t0"), "queued tracked");
        check(ai.rankTracks(candidates).size() == 5, "queued excluded");

        ai.recordPlayed(candidates[0]);
        check(!ai.isQueued("t0") && ai.hasPlayed("t0"), "played moves from queue to history");
    }

    void testCallbackServer()
    {
        check(OAuthCallbackServer::supports("http://127.0.0.1:8888/callback"), "loopback supported");
        check(!OAuthCallbackServer::supports("blitzdj://callback"), "custom scheme not served");
        check(!OAuthCallbackServer::supports("http://example.com:8888/callback"), "remote host rejected");

        OAuthCallbackServer server("http://127.0.0.1:18923/callback");

        if (!server.start())
        {
            std::cerr << "Skipping callback server test: " << server.getLastError() << "\n";
            return;
        }

        HttpResponse missing;
        HttpResponse callback;

        std::thread client([&]
        {
            auto http = createDefaultHttpClient();

            HttpRequest request;
            request.url = "http://127.0.0.1:18923/favicon.ico";
            request.timeoutSeconds = 5;
            missing = http->send(request);

            request.url = "http://127.0.0.1:18923/callback?code=abc&state=xyz";
            callback = http->send(request);
        });

        const auto redirect = server.waitForRedirect(std::chrono::seconds(10));
        client.join();

        check(missing.statusCode == 404, "unknown path returns 404");
        check(callback.statusCode == 200, "callback returns 200");
        check(callback.body.find("Spotify authorized") != std::string::npos, "callback page");
        check(redirect && *redirect == "http://127.0.0.1:18923/callback?code=abc&state=xyz", "redirect captured");

        OAuthCallbackServer busy("http://127.0.0.1:18923/callback");
        check(!busy.start(), "port in use detected");

        server.stop();

        OAuthCallbackServer timeout("http://127.0.0.1:18924/callback");
        check(!timeout.waitForRedirect(std::chrono::seconds(1)).has_value(), "timeout returns nothing");
    }

    void testBridge()
    {
        auto* handle = blitzdj_create("client", "blitzdj://callback", "");
        check(handle != nullptr, "bridge create");
        check(blitzdj_is_authenticated(handle) == 0, "bridge not authenticated");

        char* url = blitzdj_authorization_url(handle);
        check(url && std::string(url).find("accounts.spotify.com") != std::string::npos, "bridge url");
        blitzdj_free_string(url);

        blitzdj_set_search_query(handle, "lofi");
        char* query = blitzdj_search_query(handle);
        check(query && std::string(query) == "lofi", "bridge query");
        blitzdj_free_string(query);

        check(blitzdj_start_auto_dj(handle) == 0, "bridge autodj requires login");
        blitzdj_destroy(handle);
        blitzdj_destroy(nullptr);
    }
}

int main()
{
    testCrypto();
    testAuthenticationFlow();
    testAI();
    testCallbackServer();
    testBridge();

    if (failures == 0)
    {
        std::cout << "All BlitzDJ tests passed.\n";
        return 0;
    }

    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
