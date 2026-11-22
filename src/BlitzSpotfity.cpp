#include "BlitzSpotify.hpp"

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace blitzdj {
namespace {
size_t writeCallback(char* ptr, size_t size, size_t count, void* data) {
    auto* output = static_cast<std::string*>(data);
    output->append(ptr, size * count);
    return size * count;
}
std::string base64url(const unsigned char* bytes, size_t size) {
    std::string encoded(4 * ((size + 2) / 3), '\0');
    int n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&encoded[0]), bytes, static_cast<int>(size));
    encoded.resize(n);
    for (char& c : encoded) { if (c == '+') c = '-'; else if (c == '/') c = '_'; }
    while (!encoded.empty() && encoded.back() == '=') encoded.pop_back();
    return encoded;
}
std::string getString(const Json& j, const char* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : "";
}
int getInt(const Json& j, const char* key, int fallback = 0) {
    auto it = j.find(key);
    return it != j.end() && it->is_number_integer() ? it->get<int>() : fallback;
}
bool getBool(const Json& j, const char* key, bool fallback = false) {
    auto it = j.find(key);
    return it != j.end() && it->is_boolean() ? it->get<bool>() : fallback;
}
Json objectAt(const Json& j, const char* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_object() ? *it : Json::object();
}
std::string errorMessage(const Json& data, const std::string& fallback) {
    if (!data.is_object()) return fallback;
    auto e = data.find("error");
    if (e != data.end()) {
        if (e->is_string()) return e->get<std::string>();
        if (e->is_object()) {
            auto msg = getString(*e, "message");
            if (!msg.empty()) return msg;
        }
    }
    auto description = getString(data, "error_description");
    return description.empty() ? fallback : description;
}
}

BlitzSpotify::BlitzSpotify(const std::string& clientId, const std::string& redirectUri)
    : clientId_(clientId), redirectUri_(redirectUri) {}
BlitzSpotify::~BlitzSpotify() = default;

bool BlitzSpotify::initialize() {
    if (initialized_) return true;
    if (clientId_.empty() || redirectUri_.empty()) {
        setError("Spotify client ID and redirect URI are required.");
        return false;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        setError("Could not initialize libcurl.");
        return false;
    }
    initialized_ = true;
    clearError();
    return true;
}

std::string BlitzSpotify::generateRandomState() {
    unsigned char bytes[24];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("Secure random generation failed.");
    return base64url(bytes, sizeof(bytes));
}
std::string BlitzSpotify::generateCodeVerifier() {
    unsigned char bytes[48];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("Secure random generation failed.");
    return base64url(bytes, sizeof(bytes));
}
std::string BlitzSpotify::generateCodeChallenge(const std::string& verifier) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int size = 0;
    if (EVP_Digest(verifier.data(), verifier.size(), digest, &size, EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("SHA-256 generation failed.");
    return base64url(digest, size);
}
std::string BlitzSpotify::urlEncode(const std::string& value) const {
    std::ostringstream out;
    out << std::uppercase << std::hex;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out << c;
        else out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return out.str();
}
std::string BlitzSpotify::generateAuthorizationUrl() {
    if (!initialize()) return "";
    try {
        codeVerifier_ = generateCodeVerifier();
        oauthState_ = generateRandomState();
        const std::string scopes = "user-read-private user-read-playback-state user-read-currently-playing user-modify-playback-state";
        return std::string(AUTH_URL) + "?response_type=code&client_id=" + urlEncode(clientId_) +
            "&redirect_uri=" + urlEncode(redirectUri_) + "&scope=" + urlEncode(scopes) +
            "&state=" + urlEncode(oauthState_) + "&code_challenge_method=S256&code_challenge=" +
            urlEncode(generateCodeChallenge(codeVerifier_));
    } catch (const std::exception& e) { setError(e.what()); return ""; }
}

SpotifyResponse BlitzSpotify::tokenRequest(const std::string& formData) {
    SpotifyResponse result;
    if (!initialize()) { result.error = lastError_; return result; }
    CURL* curl = curl_easy_init();
    if (!curl) { setError("Could not create HTTP client."); result.error = lastError_; return result; }
    std::string response;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_URL, TOKEN_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, formData.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(formData.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.statusCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (code == CURLE_OK && !response.empty()) {
        result.data = Json::parse(response, nullptr, false);
        if (result.data.is_discarded()) result.data = Json::object();
    }
    result.success = code == CURLE_OK && result.statusCode >= 200 && result.statusCode < 300;
    if (!result.success) {
        result.error = code != CURLE_OK ? curl_easy_strerror(code) : errorMessage(result.data, "Spotify token request failed.");
        setError(result.error, result.statusCode);
    } else clearError();
    return result;
}

bool BlitzSpotify::updateTokens(const Json& response) {
    if (!response.is_object() || getString(response, "access_token").empty()) {
        setError("Token response does not contain an access token.");
        return false;
    }
    tokens_.accessToken = getString(response, "access_token");
    std::string refresh = getString(response, "refresh_token");
    if (!refresh.empty()) tokens_.refreshToken = refresh;
    std::string type = getString(response, "token_type");
    if (!type.empty()) tokens_.tokenType = type;
    int seconds = std::max(1, getInt(response, "expires_in", 3600));
    tokens_.expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(std::max(1, seconds - 60));
    clearError();
    return true;
}
bool BlitzSpotify::authenticate(const std::string& authorizationCode, const std::string& returnedState) {
    if (codeVerifier_.empty() || oauthState_.empty() || authorizationCode.empty() ||
        returnedState.empty() || returnedState != oauthState_) {
        setError("Invalid authorization code or OAuth state.");
        return false;
    }
    std::string form = "grant_type=authorization_code&code=" + urlEncode(authorizationCode) +
        "&redirect_uri=" + urlEncode(redirectUri_) + "&client_id=" + urlEncode(clientId_) +
        "&code_verifier=" + urlEncode(codeVerifier_);
    auto response = tokenRequest(form);
    if (!response.success) return false;
    if (!updateTokens(response.data)) return false;
    codeVerifier_.clear(); oauthState_.clear();
    return true;
}
bool BlitzSpotify::refreshAccessToken() {
    if (tokens_.refreshToken.empty()) { setError("No refresh token is available."); return false; }
    auto response = tokenRequest("grant_type=refresh_token&refresh_token=" + urlEncode(tokens_.refreshToken) +
        "&client_id=" + urlEncode(clientId_));
    return response.success && updateTokens(response.data);
}
bool BlitzSpotify::ensureValidToken() {
    if (!tokens_.isExpired()) return true;
    return refreshAccessToken();
}
bool BlitzSpotify::isAuthenticated() const { return !tokens_.isExpired(); }
void BlitzSpotify::logout() { tokens_ = SpotifyTokens{}; codeVerifier_.clear(); oauthState_.clear(); deviceId_.clear(); autoDJEnabled_ = false; clearError(); }
void BlitzSpotify::setTokens(const SpotifyTokens& tokens) { tokens_ = tokens; }
const SpotifyTokens& BlitzSpotify::getTokens() const { return tokens_; }

SpotifyResponse BlitzSpotify::request(const std::string& method, const std::string& endpoint, const Json& body) {
    SpotifyResponse result;
    if (!initialize() || !ensureValidToken()) { result.error = lastError_; return result; }
    CURL* curl = curl_easy_init();
    if (!curl) { setError("Could not create HTTP client."); result.error = lastError_; return result; }
    std::string response;
    std::string payload = body.is_null() ? "" : body.dump();
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + tokens_.accessToken).c_str());
    headers = curl_slist_append(headers, "Accept: application/json");
    if (!body.is_null()) headers = curl_slist_append(headers, "Content-Type: application/json");
    const std::string url = std::string(API_URL) + endpoint;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    if (!body.is_null()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    } else if (method == "POST" || method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.statusCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (code == CURLE_OK && !response.empty()) {
        result.data = Json::parse(response, nullptr, false);
        if (result.data.is_discarded()) result.data = Json::object();
    }
    result.success = code == CURLE_OK && result.statusCode >= 200 && result.statusCode < 300;
    if (!result.success) {
        result.error = code != CURLE_OK ? curl_easy_strerror(code) : errorMessage(result.data, "Spotify API request failed.");
        setError(result.error, result.statusCode);
    } else clearError();
    return result;
}

SpotifyTrack BlitzSpotify::parseTrack(const Json& data) const {
    SpotifyTrack track;
    if (!data.is_object()) return track;
    track.id = getString(data, "id"); track.uri = getString(data, "uri"); track.name = getString(data, "name");
    track.durationMs = getInt(data, "duration_ms");
    track.explicitContent = getBool(data, "explicit");
    track.playable = getBool(data, "is_playable", true);
    auto artists = data.value("artists", Json::array());
    if (artists.is_array()) for (const auto& artist : artists) {
        if (!artist.is_object()) continue;
        if (!track.artist.empty()) track.artist += ", ";
        track.artist += getString(artist, "name");
    }
    auto album = objectAt(data, "album");
    track.album = getString(album, "name");
    auto images = album.value("images", Json::array());
    if (images.is_array() && !images.empty() && images[0].is_object()) track.imageUrl = getString(images[0], "url");
    return track;
}
SpotifyDevice BlitzSpotify::parseDevice(const Json& data) const {
    SpotifyDevice device;
    if (!data.is_object()) return device;
    device.id = getString(data, "id"); device.name = getString(data, "name"); device.type = getString(data, "type");
    device.active = getBool(data, "is_active"); device.restricted = getBool(data, "is_restricted");
    device.volume = getInt(data, "volume_percent", 100);
    return device;
}
SpotifyPlayback BlitzSpotify::parsePlayback(const Json& data) const {
    SpotifyPlayback playback;
    if (!data.is_object()) return playback;
    playback.device = parseDevice(objectAt(data, "device"));
    playback.playing = getBool(data, "is_playing"); playback.shuffle = getBool(data, "shuffle_state");
    playback.repeatMode = getString(data, "repeat_state");
    playback.progressMs = getInt(data, "progress_ms");
    auto item = objectAt(data, "item");
    if (getString(item, "type") == "track") playback.track = parseTrack(item);
    playback.durationMs = playback.track.durationMs;
    return playback;
}
Json BlitzSpotify::getCurrentUser() { auto r = request("GET", "/me"); return r.success ? r.data : Json::object(); }
std::vector<SpotifyTrack> BlitzSpotify::searchTracks(const std::string& query, int limit) {
    std::vector<SpotifyTrack> tracks;
    if (query.empty()) { setError("Search query cannot be empty."); return tracks; }
    limit = std::clamp(limit, 1, 10);
    auto r = request("GET", "/search?q=" + urlEncode(query) + "&type=track&limit=" + std::to_string(limit));
    if (!r.success) return tracks;
    auto items = objectAt(r.data, "tracks").value("items", Json::array());
    if (items.is_array()) for (const auto& item : items) if (item.is_object()) tracks.push_back(parseTrack(item));
    return tracks;
}
std::optional<SpotifyTrack> BlitzSpotify::getTrack(const std::string& trackId) {
    if (trackId.empty()) { setError("Track ID cannot be empty."); return std::nullopt; }
    auto r = request("GET", "/tracks/" + urlEncode(trackId));
    if (!r.success) return std::nullopt;
    return parseTrack(r.data);
}
std::string BlitzSpotify::playerEndpoint(const std::string& path) const {
    return "/me/player" + path + (deviceId_.empty() ? "" : (path.find('?') == std::string::npos ? "?" : "&") + std::string("device_id=") + urlEncode(deviceId_));
}
bool BlitzSpotify::play() { return request("PUT", playerEndpoint("/play")).success; }
bool BlitzSpotify::resume() { return play(); }
bool BlitzSpotify::playTrack(const std::string& uri) { return playTracks({uri}); }
bool BlitzSpotify::playTracks(const std::vector<std::string>& uris) {
    if (uris.empty()) { setError("At least one track URI is required."); return false; }
    for (const auto& uri : uris) if (uri.rfind("spotify:track:", 0) != 0) { setError("Invalid Spotify track URI."); return false; }
    return request("PUT", playerEndpoint("/play"), Json{{"uris", uris}}).success;
}
bool BlitzSpotify::playPlaylist(const std::string& uri) {
    if (uri.rfind("spotify:playlist:", 0) != 0) { setError("Invalid Spotify playlist URI."); return false; }
    return request("PUT", playerEndpoint("/play"), Json{{"context_uri", uri}}).success;
}
bool BlitzSpotify::pause() { return request("PUT", playerEndpoint("/pause")).success; }
bool BlitzSpotify::nextTrack() { return request("POST", playerEndpoint("/next")).success; }
bool BlitzSpotify::previousTrack() { return request("POST", playerEndpoint("/previous")).success; }
bool BlitzSpotify::seek(int positionMs) {
    if (positionMs < 0) { setError("Position must be nonnegative."); return false; }
    return request("PUT", playerEndpoint("/seek?position_ms=" + std::to_string(positionMs))).success;
}
bool BlitzSpotify::setVolume(int volume) {
    if (volume < 0 || volume > 100) { setError("Volume must be between 0 and 100."); return false; }
    return request("PUT", playerEndpoint("/volume?volume_percent=" + std::to_string(volume))).success;
}
bool BlitzSpotify::setShuffle(bool enabled) { return request("PUT", playerEndpoint(std::string("/shuffle?state=") + (enabled ? "true" : "false"))).success; }
bool BlitzSpotify::setRepeat(const std::string& mode) {
    if (mode != "off" && mode != "track" && mode != "context") { setError("Repeat mode must be off, track, or context."); return false; }
    return request("PUT", playerEndpoint("/repeat?state=" + mode)).success;
}
bool BlitzSpotify::addToQueue(const std::string& uri) {
    if (uri.rfind("spotify:track:", 0) != 0) { setError("Invalid Spotify track URI."); return false; }
    return request("POST", playerEndpoint("/queue?uri=" + urlEncode(uri))).success;
}
Json BlitzSpotify::getQueue() { auto r = request("GET", "/me/player/queue"); return r.success ? r.data : Json::object(); }
std::optional<SpotifyPlayback> BlitzSpotify::getPlayback() {
    auto r = request("GET", "/me/player");
    if (!r.success || r.statusCode == 204 || r.data.empty()) return std::nullopt;
    return parsePlayback(r.data);
}
std::optional<SpotifyTrack> BlitzSpotify::getCurrentTrack() {
    auto playback = getPlayback();
    if (!playback || playback->track.uri.empty()) return std::nullopt;
    return playback->track;
}
bool BlitzSpotify::isPlaying() { auto playback = getPlayback(); return playback && playback->playing; }
std::vector<SpotifyDevice> BlitzSpotify::getDevices() {
    std::vector<SpotifyDevice> devices;
    auto r = request("GET", "/me/player/devices");
    if (!r.success) return devices;
    auto items = r.data.value("devices", Json::array());
    if (items.is_array()) for (const auto& item : items) if (item.is_object()) devices.push_back(parseDevice(item));
    return devices;
}
bool BlitzSpotify::transferPlayback(const std::string& id, bool startPlaying) {
    if (id.empty()) { setError("Device ID cannot be empty."); return false; }
    auto r = request("PUT", "/me/player", Json{{"device_ids", Json::array({id})}, {"play", startPlaying}});
    if (r.success) deviceId_ = id;
    return r.success;
}
void BlitzSpotify::setDevice(const std::string& id) { deviceId_ = id; }
const std::string& BlitzSpotify::getDevice() const { return deviceId_; }
void BlitzSpotify::enableAutoDJ(bool enabled) { autoDJEnabled_ = enabled; }
bool BlitzSpotify::isAutoDJEnabled() const { return autoDJEnabled_; }
bool BlitzSpotify::queueAutomaticTrack(const SpotifyTrack& track) {
    if (!autoDJEnabled_) { setError("AutoDJ is disabled."); return false; }
    return addToQueue(track.uri);
}
bool BlitzSpotify::queueAutomaticTracks(const std::vector<SpotifyTrack>& tracks) {
    if (!autoDJEnabled_) { setError("AutoDJ is disabled."); return false; }
    if (tracks.empty()) { setError("No tracks were provided."); return false; }
    for (const auto& track : tracks) if (!addToQueue(track.uri)) return false;
    return true;
}
const std::string& BlitzSpotify::getLastError() const { return lastError_; }
long BlitzSpotify::getLastStatusCode() const { return lastStatusCode_; }
void BlitzSpotify::clearError() { lastError_.clear(); lastStatusCode_ = 0; }
void BlitzSpotify::setError(const std::string& message, long statusCode) { lastError_ = message; lastStatusCode_ = statusCode; }
}
