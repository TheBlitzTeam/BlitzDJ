#include "BlitzSpotify.hpp"
#include "Crypto.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

namespace blitzdj
{
    namespace
    {
        using Lock = std::lock_guard<std::recursive_mutex>;

        std::string getString(const Json& json, const char* key)
        {
            auto it = json.find(key);
            return it != json.end() && it->is_string() ? it->get<std::string>() : std::string();
        }

        int getInt(const Json& json, const char* key, int fallback = 0)
        {
            auto it = json.find(key);
            return it != json.end() && it->is_number_integer() ? it->get<int>() : fallback;
        }

        std::int64_t getInt64(const Json& json, const char* key, std::int64_t fallback = 0)
        {
            auto it = json.find(key);
            return it != json.end() && it->is_number_integer() ? it->get<std::int64_t>() : fallback;
        }

        bool getBool(const Json& json, const char* key, bool fallback = false)
        {
            auto it = json.find(key);
            return it != json.end() && it->is_boolean() ? it->get<bool>() : fallback;
        }

        const Json& objectAt(const Json& json, const char* key)
        {
            static const Json empty = Json::object();
            auto it = json.find(key);
            return it != json.end() && it->is_object() ? *it : empty;
        }

        const Json& arrayAt(const Json& json, const char* key)
        {
            static const Json empty = Json::array();
            auto it = json.find(key);
            return it != json.end() && it->is_array() ? *it : empty;
        }

        std::string errorMessage(const Json& data, const std::string& fallback)
        {
            if (!data.is_object())
            {
                return fallback;
            }

            auto error = data.find("error");

            if (error != data.end())
            {
                if (error->is_object())
                {
                    const auto message = getString(*error, "message");

                    if (!message.empty())
                    {
                        return message;
                    }
                }

                const auto description = getString(data, "error_description");

                if (!description.empty())
                {
                    return description;
                }

                if (error->is_string())
                {
                    return error->get<std::string>();
                }
            }

            return fallback;
        }

        std::string randomToken(std::size_t bytes)
        {
            const auto data = crypto::randomBytes(bytes);
            return crypto::base64Url(data.data(), data.size());
        }

        bool isTrackUri(const std::string& uri)
        {
            return uri.rfind("spotify:track:", 0) == 0;
        }
    }

    BlitzSpotify::BlitzSpotify(const std::string& clientId, const std::string& redirectUri)
        : clientId_(clientId),
          redirectUri_(redirectUri)
    {
    }

    BlitzSpotify::~BlitzSpotify() = default;

    bool BlitzSpotify::initialize()
    {
        Lock lock(mutex_);

        if (initialized_)
        {
            return true;
        }

        if (clientId_.empty() || redirectUri_.empty())
        {
            setError("Spotify client ID and redirect URI are required.");
            return false;
        }

        if (!http_)
        {
            http_ = createDefaultHttpClient();
        }

        if (!http_)
        {
            setError("Could not create HTTP client.");
            return false;
        }

        initialized_ = true;
        clearError();
        return true;
    }

    void BlitzSpotify::setHttpClient(std::unique_ptr<HttpClient> client)
    {
        Lock lock(mutex_);
        http_ = std::move(client);
    }

    void BlitzSpotify::setSessionPath(const std::string& path)
    {
        Lock lock(mutex_);
        sessionPath_ = path;
    }

    std::string BlitzSpotify::getSessionPath() const
    {
        Lock lock(mutex_);
        return sessionPath_;
    }

    bool BlitzSpotify::loadSession()
    {
        Lock lock(mutex_);

        if (sessionPath_.empty())
        {
            return false;
        }

        std::ifstream file(sessionPath_, std::ios::binary);

        if (!file)
        {
            return false;
        }

        const Json data = Json::parse(file, nullptr, false);

        if (!data.is_object())
        {
            setError("Saved Spotify session is corrupted.");
            return false;
        }

        if (getString(data, "client_id") != clientId_)
        {
            return false;
        }

        tokens_.accessToken = getString(data, "access_token");
        tokens_.refreshToken = getString(data, "refresh_token");

        const auto tokenType = getString(data, "token_type");
        tokens_.tokenType = tokenType.empty() ? "Bearer" : tokenType;

        tokens_.expiresAt = std::chrono::system_clock::time_point(
            std::chrono::seconds(getInt64(data, "expires_at"))
        );

        deviceId_ = getString(data, "device_id");

        const auto& pending = objectAt(data, "pending");

        if (codeVerifier_.empty())
        {
            codeVerifier_ = getString(pending, "verifier");
            oauthState_ = getString(pending, "state");
        }

        return isAuthenticated();
    }

    bool BlitzSpotify::saveSession()
    {
        Lock lock(mutex_);

        if (sessionPath_.empty())
        {
            return true;
        }

        std::error_code error;
        const std::filesystem::path path(sessionPath_);

        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path(), error);
        }

        Json data = {
            {"client_id", clientId_},
            {"access_token", tokens_.accessToken},
            {"refresh_token", tokens_.refreshToken},
            {"token_type", tokens_.tokenType},
            {"expires_at", std::chrono::duration_cast<std::chrono::seconds>(
                tokens_.expiresAt.time_since_epoch()).count()},
            {"device_id", deviceId_}
        };

        if (!codeVerifier_.empty())
        {
            data["pending"] = {
                {"verifier", codeVerifier_},
                {"state", oauthState_}
            };
        }

        const auto temporary = path.string() + ".tmp";

        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);

            if (!file)
            {
                setError("Could not write Spotify session file.");
                return false;
            }

            file << data.dump(2);
        }

        std::filesystem::rename(temporary, path, error);

        if (error)
        {
            std::filesystem::remove(path, error);
            std::filesystem::rename(temporary, path, error);
        }

        if (error)
        {
            setError("Could not save Spotify session: " + error.message());
            return false;
        }

        return true;
    }

    std::string BlitzSpotify::urlEncode(const std::string& value)
    {
        std::ostringstream out;
        out << std::uppercase << std::hex;

        for (unsigned char c : value)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            {
                out << c;
            }
            else
            {
                out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
            }
        }

        return out.str();
    }

    std::string BlitzSpotify::urlDecode(const std::string& value)
    {
        std::string output;
        output.reserve(value.size());

        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const char c = value[i];

            if (c == '+')
            {
                output.push_back(' ');
            }
            else if (c == '%' && i + 2 < value.size() &&
                std::isxdigit(static_cast<unsigned char>(value[i + 1])) &&
                std::isxdigit(static_cast<unsigned char>(value[i + 2])))
            {
                output.push_back(static_cast<char>(std::stoi(value.substr(i + 1, 2), nullptr, 16)));
                i += 2;
            }
            else
            {
                output.push_back(c);
            }
        }

        return output;
    }

    std::string BlitzSpotify::generateAuthorizationUrl()
    {
        Lock lock(mutex_);

        if (!initialize())
        {
            return "";
        }

        codeVerifier_ = randomToken(48);
        oauthState_ = randomToken(24);

        const auto digest = crypto::sha256(codeVerifier_);
        const auto challenge = crypto::base64Url(digest.data(), digest.size());

        const std::string scopes =
            "user-read-private user-read-playback-state "
            "user-read-currently-playing user-modify-playback-state";

        saveSession();

        return std::string(AUTH_URL) +
            "?response_type=code&client_id=" + urlEncode(clientId_) +
            "&redirect_uri=" + urlEncode(redirectUri_) +
            "&scope=" + urlEncode(scopes) +
            "&state=" + urlEncode(oauthState_) +
            "&code_challenge_method=S256&code_challenge=" + challenge;
    }

    SpotifyResponse BlitzSpotify::finishResponse(const HttpResponse& response, const std::string& fallbackError)
    {
        SpotifyResponse result;
        result.statusCode = response.statusCode;

        if (!response.body.empty())
        {
            result.data = Json::parse(response.body, nullptr, false);

            if (result.data.is_discarded())
            {
                result.data = Json::object();
            }
        }

        result.success = response.error.empty() &&
            response.statusCode >= 200 && response.statusCode < 300;

        if (result.success)
        {
            clearError();
        }
        else
        {
            result.error = !response.error.empty()
                ? response.error
                : errorMessage(result.data, fallbackError);

            setError(result.error, result.statusCode);
        }

        return result;
    }

    SpotifyResponse BlitzSpotify::tokenRequest(const std::string& formData)
    {
        Lock lock(mutex_);

        if (!initialize())
        {
            SpotifyResponse result;
            result.error = lastError_;
            return result;
        }

        HttpRequest http;
        http.method = "POST";
        http.url = TOKEN_URL;
        http.body = formData;
        http.headers = {
            {"Content-Type", "application/x-www-form-urlencoded"},
            {"Accept", "application/json"}
        };

        HttpResponse response;

        for (int attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt)
        {
            response = http_->send(http);

            if (response.statusCode != 429 || attempt == MAX_ATTEMPTS)
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(
                std::clamp(response.retryAfterSeconds, 1, 5)
            ));
        }

        return finishResponse(response, "Spotify token request failed.");
    }

    bool BlitzSpotify::updateTokens(const Json& response)
    {
        const auto accessToken = getString(response, "access_token");

        if (accessToken.empty())
        {
            setError("Token response does not contain an access token.");
            return false;
        }

        tokens_.accessToken = accessToken;

        const auto refresh = getString(response, "refresh_token");

        if (!refresh.empty())
        {
            tokens_.refreshToken = refresh;
        }

        const auto type = getString(response, "token_type");

        if (!type.empty())
        {
            tokens_.tokenType = type;
        }

        const int seconds = std::max(61, getInt(response, "expires_in", 3600));

        tokens_.expiresAt = std::chrono::system_clock::now() +
            std::chrono::seconds(seconds - 60);

        saveSession();
        clearError();
        return true;
    }

    bool BlitzSpotify::authenticate(const std::string& authorizationCode, const std::string& returnedState)
    {
        Lock lock(mutex_);

        if (codeVerifier_.empty() || oauthState_.empty())
        {
            setError("No pending Spotify login. Generate a new authorization URL.");
            return false;
        }

        if (authorizationCode.empty() || returnedState != oauthState_)
        {
            setError("Invalid authorization code or OAuth state.");
            return false;
        }

        const auto response = tokenRequest(
            "grant_type=authorization_code&code=" + urlEncode(authorizationCode) +
            "&redirect_uri=" + urlEncode(redirectUri_) +
            "&client_id=" + urlEncode(clientId_) +
            "&code_verifier=" + urlEncode(codeVerifier_)
        );

        if (!response.success)
        {
            return false;
        }

        codeVerifier_.clear();
        oauthState_.clear();

        return updateTokens(response.data);
    }

    bool BlitzSpotify::handleRedirect(const std::string& redirectUrl)
    {
        std::string query = redirectUrl;

        const auto start = query.find_first_of("?#");

        if (start != std::string::npos)
        {
            query = query.substr(start + 1);
        }

        std::string code;
        std::string state;
        std::string error;

        std::size_t position = 0;

        while (position <= query.size())
        {
            auto end = query.find_first_of("&#", position);

            if (end == std::string::npos)
            {
                end = query.size();
            }

            const auto pair = query.substr(position, end - position);
            const auto equals = pair.find('=');

            if (equals != std::string::npos)
            {
                const auto key = pair.substr(0, equals);
                const auto value = urlDecode(pair.substr(equals + 1));

                if (key == "code")
                {
                    code = value;
                }
                else if (key == "state")
                {
                    state = value;
                }
                else if (key == "error")
                {
                    error = value;
                }
            }

            position = end + 1;
        }

        Lock lock(mutex_);

        if (!error.empty())
        {
            setError("Spotify authorization failed: " + error);
            return false;
        }

        if (code.empty())
        {
            setError("The redirect URL does not contain an authorization code.");
            return false;
        }

        return authenticate(code, state);
    }

    bool BlitzSpotify::refreshAccessToken()
    {
        Lock lock(mutex_);

        if (tokens_.refreshToken.empty())
        {
            setError("No refresh token is available.");
            return false;
        }

        const auto response = tokenRequest(
            "grant_type=refresh_token&refresh_token=" + urlEncode(tokens_.refreshToken) +
            "&client_id=" + urlEncode(clientId_)
        );

        if (!response.success)
        {
            if (response.statusCode == 400 || response.statusCode == 401)
            {
                tokens_ = SpotifyTokens{};
                saveSession();
                setError("Spotify session expired. Please log in again.", response.statusCode);
            }

            return false;
        }

        return updateTokens(response.data);
    }

    bool BlitzSpotify::ensureValidToken()
    {
        if (!tokens_.isExpired())
        {
            return true;
        }

        if (tokens_.refreshToken.empty())
        {
            setError("Spotify is not connected.");
            return false;
        }

        return refreshAccessToken();
    }

    bool BlitzSpotify::isAuthenticated() const
    {
        Lock lock(mutex_);
        return !tokens_.accessToken.empty() &&
            (!tokens_.isExpired() || !tokens_.refreshToken.empty());
    }

    void BlitzSpotify::logout()
    {
        Lock lock(mutex_);

        tokens_ = SpotifyTokens{};
        codeVerifier_.clear();
        oauthState_.clear();
        deviceId_.clear();
        autoDJEnabled_ = false;

        if (!sessionPath_.empty())
        {
            std::error_code error;
            std::filesystem::remove(sessionPath_, error);
        }

        clearError();
    }

    void BlitzSpotify::setTokens(const SpotifyTokens& tokens)
    {
        Lock lock(mutex_);
        tokens_ = tokens;
        saveSession();
    }

    SpotifyTokens BlitzSpotify::getTokens() const
    {
        Lock lock(mutex_);
        return tokens_;
    }

    SpotifyResponse BlitzSpotify::request(const std::string& method, const std::string& endpoint, const Json& body)
    {
        Lock lock(mutex_);

        if (!initialize() || !ensureValidToken())
        {
            SpotifyResponse result;
            result.error = lastError_;
            result.statusCode = lastStatusCode_;
            return result;
        }

        const bool idempotent = method == "GET" || method == "PUT";
        bool refreshed = false;

        HttpRequest http;
        http.method = method;
        http.url = std::string(API_URL) + endpoint;

        if (!body.is_null())
        {
            http.body = body.dump();
        }

        HttpResponse response;

        for (int attempt = 1; ; ++attempt)
        {
            http.headers = {
                {"Authorization", tokens_.tokenType + " " + tokens_.accessToken},
                {"Accept", "application/json"}
            };

            if (!body.is_null())
            {
                http.headers.emplace_back("Content-Type", "application/json");
            }

            response = http_->send(http);

            if (response.statusCode == 401 && !refreshed && !tokens_.refreshToken.empty())
            {
                refreshed = true;

                if (!refreshAccessToken())
                {
                    SpotifyResponse result;
                    result.error = lastError_;
                    result.statusCode = 401;
                    return result;
                }

                continue;
            }

            const bool transient = response.statusCode == 429 ||
                (idempotent && (!response.error.empty() || response.statusCode >= 500));

            if (!transient || attempt >= MAX_ATTEMPTS)
            {
                break;
            }

            const int delay = response.statusCode == 429
                ? std::clamp(response.retryAfterSeconds, 1, 5)
                : attempt;

            std::this_thread::sleep_for(std::chrono::seconds(delay));
        }

        return finishResponse(response, "Spotify API request failed.");
    }

    SpotifyTrack BlitzSpotify::parseTrack(const Json& data) const
    {
        SpotifyTrack track;

        if (!data.is_object())
        {
            return track;
        }

        track.id = getString(data, "id");
        track.uri = getString(data, "uri");
        track.name = getString(data, "name");
        track.durationMs = getInt(data, "duration_ms");
        track.explicitContent = getBool(data, "explicit");
        track.playable = getBool(data, "is_playable", true);

        for (const auto& artist : arrayAt(data, "artists"))
        {
            if (!artist.is_object())
            {
                continue;
            }

            if (!track.artist.empty())
            {
                track.artist += ", ";
            }

            track.artist += getString(artist, "name");
        }

        const auto& album = objectAt(data, "album");
        track.album = getString(album, "name");

        const auto& images = arrayAt(album, "images");

        if (!images.empty() && images[0].is_object())
        {
            track.imageUrl = getString(images[0], "url");
        }

        return track;
    }

    SpotifyDevice BlitzSpotify::parseDevice(const Json& data) const
    {
        SpotifyDevice device;

        if (!data.is_object())
        {
            return device;
        }

        device.id = getString(data, "id");
        device.name = getString(data, "name");
        device.type = getString(data, "type");
        device.active = getBool(data, "is_active");
        device.restricted = getBool(data, "is_restricted");
        device.volume = getInt(data, "volume_percent", 100);

        return device;
    }

    SpotifyPlayback BlitzSpotify::parsePlayback(const Json& data) const
    {
        SpotifyPlayback playback;

        if (!data.is_object())
        {
            return playback;
        }

        playback.device = parseDevice(objectAt(data, "device"));
        playback.playing = getBool(data, "is_playing");
        playback.shuffle = getBool(data, "shuffle_state");
        playback.progressMs = getInt(data, "progress_ms");

        const auto repeat = getString(data, "repeat_state");
        playback.repeatMode = repeat.empty() ? "off" : repeat;

        const auto& item = objectAt(data, "item");

        if (getString(item, "type") == "track")
        {
            playback.track = parseTrack(item);
        }

        playback.durationMs = playback.track.durationMs;
        return playback;
    }

    Json BlitzSpotify::getCurrentUser()
    {
        auto response = request("GET", "/me");
        return response.success ? response.data : Json::object();
    }

    std::vector<SpotifyTrack> BlitzSpotify::searchTracks(const std::string& query, int limit, int offset)
    {
        std::vector<SpotifyTrack> tracks;

        if (query.empty())
        {
            Lock lock(mutex_);
            setError("Search query cannot be empty.");
            return tracks;
        }

        limit = std::clamp(limit, 1, SEARCH_PAGE_LIMIT);
        offset = std::clamp(offset, 0, SEARCH_MAX_OFFSET - limit);

        const auto response = request(
            "GET",
            "/search?q=" + urlEncode(query) +
            "&type=track&limit=" + std::to_string(limit) +
            "&offset=" + std::to_string(offset)
        );

        if (!response.success)
        {
            return tracks;
        }

        for (const auto& item : arrayAt(objectAt(response.data, "tracks"), "items"))
        {
            if (item.is_object())
            {
                tracks.push_back(parseTrack(item));
            }
        }

        return tracks;
    }

    std::optional<SpotifyTrack> BlitzSpotify::getTrack(const std::string& trackId)
    {
        if (trackId.empty())
        {
            Lock lock(mutex_);
            setError("Track ID cannot be empty.");
            return std::nullopt;
        }

        const auto response = request("GET", "/tracks/" + urlEncode(trackId));

        if (!response.success)
        {
            return std::nullopt;
        }

        return parseTrack(response.data);
    }

    std::string BlitzSpotify::playerEndpoint(const std::string& path) const
    {
        if (deviceId_.empty())
        {
            return "/me/player" + path;
        }

        const char separator = path.find('?') == std::string::npos ? '?' : '&';
        return "/me/player" + path + separator + "device_id=" + urlEncode(deviceId_);
    }

    bool BlitzSpotify::play()
    {
        Lock lock(mutex_);
        return request("PUT", playerEndpoint("/play")).success;
    }

    bool BlitzSpotify::resume()
    {
        return play();
    }

    bool BlitzSpotify::playTrack(const std::string& trackUri)
    {
        return playTracks({trackUri});
    }

    bool BlitzSpotify::playTracks(const std::vector<std::string>& trackUris)
    {
        Lock lock(mutex_);

        if (trackUris.empty())
        {
            setError("At least one track URI is required.");
            return false;
        }

        if (!std::all_of(trackUris.begin(), trackUris.end(), isTrackUri))
        {
            setError("Invalid Spotify track URI.");
            return false;
        }

        return request("PUT", playerEndpoint("/play"), Json{{"uris", trackUris}}).success;
    }

    bool BlitzSpotify::playPlaylist(const std::string& playlistUri)
    {
        Lock lock(mutex_);

        if (playlistUri.rfind("spotify:playlist:", 0) != 0)
        {
            setError("Invalid Spotify playlist URI.");
            return false;
        }

        return request("PUT", playerEndpoint("/play"), Json{{"context_uri", playlistUri}}).success;
    }

    bool BlitzSpotify::pause()
    {
        Lock lock(mutex_);
        return request("PUT", playerEndpoint("/pause")).success;
    }

    bool BlitzSpotify::nextTrack()
    {
        Lock lock(mutex_);
        return request("POST", playerEndpoint("/next")).success;
    }

    bool BlitzSpotify::previousTrack()
    {
        Lock lock(mutex_);
        return request("POST", playerEndpoint("/previous")).success;
    }

    bool BlitzSpotify::seek(int positionMs)
    {
        Lock lock(mutex_);

        if (positionMs < 0)
        {
            setError("Position must be nonnegative.");
            return false;
        }

        return request("PUT", playerEndpoint("/seek?position_ms=" + std::to_string(positionMs))).success;
    }

    bool BlitzSpotify::setVolume(int volume)
    {
        Lock lock(mutex_);

        if (volume < 0 || volume > 100)
        {
            setError("Volume must be between 0 and 100.");
            return false;
        }

        return request("PUT", playerEndpoint("/volume?volume_percent=" + std::to_string(volume))).success;
    }

    bool BlitzSpotify::setShuffle(bool enabled)
    {
        Lock lock(mutex_);
        return request("PUT", playerEndpoint(std::string("/shuffle?state=") + (enabled ? "true" : "false"))).success;
    }

    bool BlitzSpotify::setRepeat(const std::string& mode)
    {
        Lock lock(mutex_);

        if (mode != "off" && mode != "track" && mode != "context")
        {
            setError("Repeat mode must be off, track, or context.");
            return false;
        }

        return request("PUT", playerEndpoint("/repeat?state=" + mode)).success;
    }

    bool BlitzSpotify::addToQueue(const std::string& trackUri)
    {
        Lock lock(mutex_);

        if (!isTrackUri(trackUri))
        {
            setError("Invalid Spotify track URI.");
            return false;
        }

        return request("POST", playerEndpoint("/queue?uri=" + urlEncode(trackUri))).success;
    }

    Json BlitzSpotify::getQueue()
    {
        auto response = request("GET", "/me/player/queue");
        return response.success ? response.data : Json::object();
    }

    std::optional<SpotifyPlayback> BlitzSpotify::getPlayback()
    {
        const auto response = request("GET", "/me/player");

        if (!response.success || response.statusCode == 204 || response.data.empty())
        {
            return std::nullopt;
        }

        return parsePlayback(response.data);
    }

    std::optional<SpotifyTrack> BlitzSpotify::getCurrentTrack()
    {
        auto playback = getPlayback();

        if (!playback || playback->track.uri.empty())
        {
            return std::nullopt;
        }

        return playback->track;
    }

    bool BlitzSpotify::isPlaying()
    {
        const auto playback = getPlayback();
        return playback && playback->playing;
    }

    std::vector<SpotifyDevice> BlitzSpotify::getDevices()
    {
        std::vector<SpotifyDevice> devices;

        const auto response = request("GET", "/me/player/devices");

        if (!response.success)
        {
            return devices;
        }

        for (const auto& item : arrayAt(response.data, "devices"))
        {
            if (item.is_object())
            {
                devices.push_back(parseDevice(item));
            }
        }

        return devices;
    }

    bool BlitzSpotify::transferPlayback(const std::string& deviceId, bool startPlaying)
    {
        Lock lock(mutex_);

        if (deviceId.empty())
        {
            setError("Device ID cannot be empty.");
            return false;
        }

        const auto response = request(
            "PUT",
            "/me/player",
            Json{{"device_ids", Json::array({deviceId})}, {"play", startPlaying}}
        );

        if (response.success)
        {
            deviceId_ = deviceId;
            saveSession();
        }

        return response.success;
    }

    void BlitzSpotify::setDevice(const std::string& deviceId)
    {
        Lock lock(mutex_);
        deviceId_ = deviceId;
        saveSession();
    }

    std::string BlitzSpotify::getDevice() const
    {
        Lock lock(mutex_);
        return deviceId_;
    }

    void BlitzSpotify::enableAutoDJ(bool enabled)
    {
        Lock lock(mutex_);
        autoDJEnabled_ = enabled;
    }

    bool BlitzSpotify::isAutoDJEnabled() const
    {
        Lock lock(mutex_);
        return autoDJEnabled_;
    }

    bool BlitzSpotify::queueAutomaticTrack(const SpotifyTrack& track)
    {
        Lock lock(mutex_);

        if (!autoDJEnabled_)
        {
            setError("AutoDJ is disabled.");
            return false;
        }

        return addToQueue(track.uri);
    }

    bool BlitzSpotify::queueAutomaticTracks(const std::vector<SpotifyTrack>& tracks)
    {
        Lock lock(mutex_);

        if (!autoDJEnabled_)
        {
            setError("AutoDJ is disabled.");
            return false;
        }

        if (tracks.empty())
        {
            setError("No tracks were provided.");
            return false;
        }

        for (const auto& track : tracks)
        {
            if (!addToQueue(track.uri))
            {
                return false;
            }
        }

        return true;
    }

    std::string BlitzSpotify::getLastError() const
    {
        Lock lock(mutex_);
        return lastError_;
    }

    long BlitzSpotify::getLastStatusCode() const
    {
        Lock lock(mutex_);
        return lastStatusCode_;
    }

    void BlitzSpotify::clearError()
    {
        Lock lock(mutex_);
        lastError_.clear();
        lastStatusCode_ = 0;
    }

    void BlitzSpotify::setError(const std::string& message, long statusCode)
    {
        Lock lock(mutex_);
        lastError_ = message;
        lastStatusCode_ = statusCode;
    }
}
