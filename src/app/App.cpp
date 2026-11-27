#include "App.hpp"

#include "Icons.hpp"
#include "OAuthCallbackServer.hpp"
#include "Platform.hpp"
#include "Strings.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <thread>

namespace blitzdj::app
{
    namespace
    {
        const ImVec4 BACKGROUND(0.055f, 0.059f, 0.075f, 1.0f);
        const ImVec4 SURFACE(0.086f, 0.094f, 0.122f, 1.0f);
        const ImVec4 SURFACE_HIGH(0.122f, 0.133f, 0.169f, 1.0f);
        const ImVec4 OUTLINE(0.173f, 0.188f, 0.231f, 1.0f);
        const ImVec4 ACCENT(1.0f, 0.761f, 0.102f, 1.0f);
        const ImVec4 ON_ACCENT(0.102f, 0.075f, 0.0f, 1.0f);
        const ImVec4 TEXT_PRIMARY(0.957f, 0.957f, 0.965f, 1.0f);
        const ImVec4 TEXT_SECONDARY(0.604f, 0.612f, 0.659f, 1.0f);

        constexpr float MAX_CONTENT_WIDTH = 640.0f;
        constexpr auto POLL_INTERVAL = std::chrono::seconds(3);
        constexpr auto SPEAKER_INTERVAL = std::chrono::seconds(5);
        constexpr auto RECONNECT_INTERVAL = std::chrono::seconds(10);
        constexpr auto SCAN_DURATION = std::chrono::seconds(12);
        constexpr auto NEARBY_REFRESH = std::chrono::milliseconds(1500);
        constexpr auto PROXIMITY_INTERVAL = std::chrono::seconds(30);
        constexpr int PROXIMITY_RSSI = -70;

        const char* const MOODS_EN[] = {"Party", "Funk", "Pop", "Electronic", "Hip-hop", "Sertanejo", "Rock", "Lo-fi"};
        const char* const MOODS_PT[] = {"Festa", "Funk", "Pop", "Eletrônica", "Hip-hop", "Sertanejo", "Rock", "Lo-fi"};

        ImU32 color(const ImVec4& value, float alpha = 1.0f)
        {
            return ImGui::GetColorU32(ImVec4(value.x, value.y, value.z, value.w * alpha));
        }

        ImVec4 mix(const ImVec4& a, const ImVec4& b, float t)
        {
            return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
        }

        float luminance(const ImVec4& value)
        {
            return 0.2126f * value.x + 0.7152f * value.y + 0.0722f * value.z;
        }

        ImVec4 readableAccent(const ImVec4& art)
        {
            ImVec4 result = art;
            const float maximum = std::max({art.x, art.y, art.z});

            if (maximum > 0.0f)
            {
                const float boost = 0.95f / maximum;
                result = ImVec4(std::min(1.0f, art.x * boost), std::min(1.0f, art.y * boost), std::min(1.0f, art.z * boost), 1.0f);
            }

            if (luminance(result) < 0.35f)
            {
                result = mix(result, ImVec4(1, 1, 1, 1), 0.4f);
            }

            return result;
        }

        std::string formatTime(int milliseconds)
        {
            const int total = std::max(0, milliseconds / 1000);
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "%d:%02d", total / 60, total % 60);
            return buffer;
        }

        std::string format(const char* pattern, const std::string& value)
        {
            const int size = std::snprintf(nullptr, 0, pattern, value.c_str());

            if (size <= 0)
            {
                return pattern;
            }

            std::string output(static_cast<std::size_t>(size), '\0');
            std::snprintf(output.data(), output.size() + 1, pattern, value.c_str());
            return output;
        }

        std::string fitText(const std::string& text, float width)
        {
            if (ImGui::CalcTextSize(text.c_str()).x <= width)
            {
                return text;
            }

            const float ellipsis = ImGui::CalcTextSize("...").x;
            std::string output = text;

            while (!output.empty())
            {
                do
                {
                    output.pop_back();
                }
                while (!output.empty() && (static_cast<unsigned char>(output.back()) & 0xC0) == 0x80);

                if (!output.empty() && (static_cast<unsigned char>(output.back()) & 0x80) != 0)
                {
                    output.pop_back();
                    continue;
                }

                if (ImGui::CalcTextSize(output.c_str()).x + ellipsis <= width)
                {
                    break;
                }
            }

            return output + "...";
        }

        void text(const std::string& value, const ImVec4& tint, float width)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, tint);
            ImGui::TextUnformatted(fitText(value, width).c_str());
            ImGui::PopStyleColor();
        }

        void wrappedText(const char* value, const ImVec4& tint, float width)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, tint);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
            ImGui::TextUnformatted(value);
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }

        bool iconButton(const char* id, Icon icon, float size, const ImVec4& tint, float iconScale = 0.6f)
        {
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 center(position.x + size * 0.5f, position.y + size * 0.5f);

            auto* list = ImGui::GetWindowDrawList();

            if (hovered)
            {
                list->AddCircleFilled(center, size * 0.5f, color(TEXT_PRIMARY, ImGui::IsItemActive() ? 0.16f : 0.08f), 32);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            drawIcon(list, icon, center, size * iconScale, color(tint));
            return pressed;
        }

        bool circleButton(const char* id, Icon icon, float size, const ImVec4& background, const ImVec4& foreground)
        {
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 center(position.x + size * 0.5f, position.y + size * 0.5f);

            auto* list = ImGui::GetWindowDrawList();
            const float grow = hovered ? 1.0f : 0.0f;

            list->AddCircleFilled(ImVec2(center.x, center.y + size * 0.04f), size * 0.5f, IM_COL32(0, 0, 0, 90), 48);
            list->AddCircleFilled(center, size * 0.5f + grow, color(hovered ? mix(background, ImVec4(1, 1, 1, 1), 0.12f) : background), 48);
            drawIcon(list, icon, center, size * 0.5f, color(foreground));

            if (hovered)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            return pressed;
        }

        bool pillButton(const char* label, bool primary, float width = 0.0f, float height = 0.0f)
        {
            const ImVec4 background = primary ? ACCENT : SURFACE_HIGH;
            const ImVec4 foreground = primary ? ON_ACCENT : TEXT_PRIMARY;

            ImGui::PushStyleColor(ImGuiCol_Button, background);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, mix(background, ImVec4(1, 1, 1, 1), 0.1f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, mix(background, ImVec4(0, 0, 0, 1), 0.1f));
            ImGui::PushStyleColor(ImGuiCol_Text, foreground);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f);

            const bool pressed = ImGui::Button(label, ImVec2(width, height));

            if (ImGui::IsItemHovered())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            return pressed;
        }

        bool chip(const char* label, bool selected)
        {
            const ImVec4 background = selected ? ImVec4(0.24f, 0.18f, 0.0f, 1.0f) : SURFACE_HIGH;

            ImGui::PushStyleColor(ImGuiCol_Button, background);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, mix(background, ImVec4(1, 1, 1, 1), 0.08f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, background);
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? ACCENT : TEXT_PRIMARY);
            ImGui::PushStyleColor(ImGuiCol_Border, selected ? ACCENT : OUTLINE);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f * ImGui::GetFontSize() / 16.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

            const bool pressed = ImGui::Button(label);

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(5);
            return pressed;
        }

        bool toggle(const char* id, bool value)
        {
            const float height = ImGui::GetFrameHeight();
            const float width = height * 1.8f;
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, height));

            auto* list = ImGui::GetWindowDrawList();
            const float radius = height * 0.5f;

            list->AddRectFilled(position, ImVec2(position.x + width, position.y + height), color(value ? ACCENT : OUTLINE), radius);

            const float knob = value ? position.x + width - radius : position.x + radius;
            list->AddCircleFilled(ImVec2(knob, position.y + radius), radius - height * 0.14f, color(value ? ON_ACCENT : TEXT_SECONDARY), 24);

            if (ImGui::IsItemHovered())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            return pressed;
        }

        bool seekBar(const char* id, float& fraction, float width, const ImVec4& accent, bool& active)
        {
            const float height = ImGui::GetFontSize() * 1.25f;
            const ImVec2 position = ImGui::GetCursorScreenPos();

            ImGui::InvisibleButton(id, ImVec2(width, height));

            const bool hovered = ImGui::IsItemHovered();
            active = ImGui::IsItemActive();

            if (active)
            {
                fraction = std::clamp((ImGui::GetIO().MousePos.x - position.x) / width, 0.0f, 1.0f);
            }

            auto* list = ImGui::GetWindowDrawList();
            const float y = position.y + height * 0.5f;
            const float thickness = (hovered || active) ? height * 0.26f : height * 0.18f;
            const float filled = position.x + width * fraction;

            list->AddRectFilled(ImVec2(position.x, y - thickness * 0.5f), ImVec2(position.x + width, y + thickness * 0.5f), color(TEXT_PRIMARY, 0.18f), thickness);
            list->AddRectFilled(ImVec2(position.x, y - thickness * 0.5f), ImVec2(filled, y + thickness * 0.5f), color(accent), thickness);

            if (hovered || active)
            {
                list->AddCircleFilled(ImVec2(filled, y), height * 0.38f, color(TEXT_PRIMARY), 24);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            return ImGui::IsItemDeactivated();
        }

        bool beginCard(const char* id, float width)
        {
            const float padding = ImGui::GetFontSize();

            ImGui::PushStyleColor(ImGuiCol_ChildBg, SURFACE);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, padding * 1.2f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));

            return ImGui::BeginChild(
                id,
                ImVec2(width, 0),
                ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                ImGuiWindowFlags_NoScrollbar
            );
        }

        void endCard()
        {
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }

        Icon deviceIcon(const std::string& type)
        {
            std::string lower = type;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (lower == "computer")
            {
                return Icon::Computer;
            }

            if (lower == "smartphone" || lower == "tablet")
            {
                return Icon::Phone;
            }

            return Icon::Speaker;
        }

        Text proximityText(int rssi)
        {
            if (rssi >= -55)
            {
                return Text::ProximityVeryClose;
            }

            if (rssi >= -70)
            {
                return Text::ProximityNear;
            }

            if (rssi >= -85)
            {
                return Text::ProximityInRange;
            }

            return Text::ProximityFar;
        }

        int signalBars(int rssi)
        {
            if (rssi >= -55)
            {
                return 4;
            }

            if (rssi >= -70)
            {
                return 3;
            }

            if (rssi >= -85)
            {
                return 2;
            }

            return 1;
        }

        bool isPortuguese()
        {
            return std::string(tr(Text::Search)) == "Buscar";
        }
    }

    App::App() = default;

    App::~App()
    {
        stopping_->store(true);

        if (loginCancel_)
        {
            loginCancel_->store(true);
        }

        if (dj_)
        {
            dj_->stop();
        }

        if (bluetooth_)
        {
            bluetooth_->shutdown();
        }
    }

    bool App::initialize()
    {
        initializeLanguage();

        const auto directory = configDirectory();
        settingsPath_ = directory / "settings.json";
        loadSettings();

        std::string clientId;

        if (const char* value = std::getenv("SPOTIFY_CLIENT_ID"); value && *value)
        {
            clientId = value;
        }

        if (clientId.empty())
        {
            clientId = settings_.value("client_id", std::string());
        }

        if (clientId.empty())
        {
            clientId = BLITZDJ_DEFAULT_CLIENT_ID;
        }

        BlitzDJConfig config;
        config.clientId = clientId;
        config.sessionPath = (directory / "session.json").string();
        config.searchQuery = settings_.value("search_query", std::string("party hits"));
        config.autoDJ = true;

        if (const char* redirect = std::getenv("SPOTIFY_REDIRECT_URI"); redirect && *redirect)
        {
            config.redirectUri = redirect;
        }

        dj_ = std::make_unique<BlitzDJ>(config);
        configuredRedirect_ = config.redirectUri;

        if (!dj_->initialize())
        {
            return false;
        }

        for (const auto& artist : settings_.value("favorite_artists", Json::array()))
        {
            if (artist.is_string())
            {
                dj_->ai().addFavoriteArtist(artist.get<std::string>());
            }
        }

        std::snprintf(queryBuffer_, sizeof(queryBuffer_), "%s", config.searchQuery.c_str());
        partyMode_ = settings_.value("party_mode", false);
        proximityConnect_ = settings_.value("proximity_connect", false);

        BluetoothConfig bluetoothConfig;
        bluetoothConfig.audioDevicesOnly = true;
        bluetooth_ = std::make_unique<Bluetooth>(bluetoothConfig);
        bluetoothReady_ = BLITZDJ_HAS_BLUETOOTH && bluetooth_->initialize();

        authenticated_ = dj_->isAuthenticated();

        if (authenticated_)
        {
            refreshPlayback();
        }
        else
        {
            startLogin();
        }

        return true;
    }

    void App::setFonts(const Fonts& fonts, float scale)
    {
        fonts_ = fonts;
        scale_ = scale;
    }

    void App::loadSettings()
    {
        std::ifstream file(settingsPath_);

        if (file)
        {
            auto data = Json::parse(file, nullptr, false);

            if (data.is_object())
            {
                settings_ = std::move(data);
            }
        }
    }

    void App::saveSettings()
    {
        if (dj_)
        {
            settings_["client_id"] = dj_->getConfig().clientId;
            settings_["search_query"] = dj_->getSearchQuery();
            settings_["favorite_artists"] = dj_->ai().getFavoriteArtists();
        }

        settings_["party_mode"] = partyMode_;
        settings_["proximity_connect"] = proximityConnect_;

        std::error_code error;
        std::filesystem::create_directories(settingsPath_.parent_path(), error);

        std::ofstream file(settingsPath_, std::ios::trunc);

        if (file)
        {
            file << settings_.dump(2);
        }
    }

    bool App::wantsFastRefresh() const
    {
        return hasPlayback_ && playback_.playing;
    }

    void App::showToast(const std::string& message)
    {
        if (message.empty())
        {
            return;
        }

        toast_ = message;
        toastUntil_ = Clock::now() + std::chrono::milliseconds(2600);
    }

    void App::command(std::function<bool(BlitzDJ&)> action, const char* success, bool refresh)
    {
        const std::string successMessage = success ? success : "";

        network_.post([this, action = std::move(action), successMessage, refresh]
        {
            const bool ok = action(*dj_);
            const std::string error = ok ? std::string() : dj_->getLastError();

            main_.post([this, ok, error, successMessage, refresh]
            {
                showToast(ok ? successMessage : error);

                if (refresh)
                {
                    lastPoll_ = Clock::now() - POLL_INTERVAL + std::chrono::milliseconds(500);
                }
            });
        });
    }

    void App::startLogin()
    {
        if (loggingIn_)
        {
            return;
        }

        loggingIn_ = true;
        manualLogin_ = false;
        loginError_.clear();
        loginCancel_ = std::make_shared<std::atomic<bool>>(false);

        auto cancel = loginCancel_;
        const std::string configured = configuredRedirect_;

        network_.post([this, cancel, configured]
        {
            bool success = false;
            bool manual = false;
            std::string error;
            std::string manualUrl;

            const auto url = dj_->getAuthorizationUrl();

            if (url.empty())
            {
                error = dj_->getLastError();
            }
            else
            {
                OAuthCallbackServer server(configured);

                if (!server.start())
                {
                    manual = true;
                    manualUrl = url;
                    error = server.getLastError();
                    openUrl(url);
                }
                else
                {
                    openUrl(url);

                    const auto redirect = server.waitForRedirect(std::chrono::minutes(5), cancel.get());
                    server.stop();

                    if (redirect)
                    {
                        success = dj_->handleRedirect(*redirect);
                        error = success ? std::string() : dj_->getLastError();
                    }
                    else if (!cancel->load())
                    {
                        error = server.getLastError();
                    }
                }
            }

            main_.post([this, success, manual, manualUrl, error]
            {
                loggingIn_ = false;
                authenticated_ = success || dj_->isAuthenticated();

                if (manual && !authenticated_)
                {
                    manualLogin_ = true;
                    manualLoginUrl_ = manualUrl;
                    pasteBuffer_[0] = '\0';
                    loginError_.clear();
                    return;
                }

                loginError_ = error;

                if (authenticated_)
                {
                    saveSettings();
                    refreshPlayback();
                }
            });
        });
    }

    void App::finishManualLogin()
    {
        const std::string pasted = pasteBuffer_;

        if (pasted.empty() || loggingIn_)
        {
            return;
        }

        loggingIn_ = true;
        loginError_.clear();

        network_.post([this, pasted]
        {
            const bool success = dj_->handleRedirect(pasted);
            const std::string error = success ? std::string() : dj_->getLastError();

            main_.post([this, success, error]
            {
                loggingIn_ = false;
                authenticated_ = success || dj_->isAuthenticated();
                loginError_ = error;

                if (authenticated_)
                {
                    manualLogin_ = false;
                    saveSettings();
                    refreshPlayback();
                }
            });
        });
    }

    void App::cancelLogin()
    {
        if (loginCancel_)
        {
            loginCancel_->store(true);
        }
    }

    void App::logout()
    {
        setAutoDJ(false);

        network_.post([this]
        {
            dj_->logout();

            main_.post([this]
            {
                authenticated_ = false;
                hasPlayback_ = false;
                queue_.clear();
                devices_.clear();
                lastTrackId_.clear();
            });
        });
    }

    void App::refreshPlayback()
    {
        if (pollInFlight_ || !authenticated_)
        {
            return;
        }

        pollInFlight_ = true;
        lastPoll_ = Clock::now();

        network_.post([this]
        {
            auto playback = dj_->getPlayback();
            const bool running = dj_->isRunning();
            const bool stillAuthenticated = dj_->isAuthenticated();

            main_.post([this, playback, running, stillAuthenticated]
            {
                pollInFlight_ = false;
                autoDJ_ = running;
                hasPlayback_ = playback.has_value() && !playback->track.id.empty();

                if (!stillAuthenticated)
                {
                    authenticated_ = false;
                    return;
                }

                if (playback)
                {
                    playback_ = *playback;
                    playbackAt_ = Clock::now();

                    if (!changingVolume_)
                    {
                        volumeFraction_ = static_cast<float>(playback_.device.volume) / 100.0f;
                    }

                    if (playback_.track.id != lastTrackId_)
                    {
                        lastTrackId_ = playback_.track.id;
                        loadQueue();
                    }
                }
            });
        });
    }

    void App::loadQueue()
    {
        network_.post([this]
        {
            auto tracks = dj_->spotify().getQueueTracks();

            if (tracks.size() > 10)
            {
                tracks.resize(10);
            }

            main_.post([this, tracks = std::move(tracks)]() mutable
            {
                queue_ = std::move(tracks);
            });
        });
    }

    void App::loadSuggestions()
    {
        if (loadingSuggestions_)
        {
            return;
        }

        loadingSuggestions_ = true;
        const std::string query = queryBuffer_;

        network_.post([this, query]
        {
            auto tracks = dj_->recommend(query.empty() ? dj_->getSearchQuery() : query, 8);

            main_.post([this, tracks = std::move(tracks)]() mutable
            {
                loadingSuggestions_ = false;
                suggestions_ = std::move(tracks);
            });
        });
    }

    void App::search()
    {
        const std::string query = searchBuffer_;

        if (query.empty() || searching_)
        {
            return;
        }

        searching_ = true;

        network_.post([this, query]
        {
            auto tracks = dj_->search(query);

            main_.post([this, tracks = std::move(tracks)]() mutable
            {
                searching_ = false;
                searched_ = true;
                results_ = std::move(tracks);
            });
        });
    }

    void App::loadDevices()
    {
        network_.post([this]
        {
            auto devices = dj_->spotify().getDevices();

            main_.post([this, devices = std::move(devices)]() mutable
            {
                devices_ = std::move(devices);
            });
        });
    }

    void App::loadSpeakers()
    {
        if (!bluetoothReady_ || loadingSpeakers_)
        {
            return;
        }

        loadingSpeakers_ = true;

        bluetoothQueue_.post([this]
        {
            auto devices = bluetooth_->getAudioDevices();

            std::sort(devices.begin(), devices.end(), [](const BluetoothDevice& a, const BluetoothDevice& b)
            {
                if (a.connected != b.connected)
                {
                    return a.connected;
                }

                return a.name < b.name;
            });

            main_.post([this, devices = std::move(devices)]() mutable
            {
                loadingSpeakers_ = false;
                speakers_ = std::move(devices);
            });
        });
    }

    void App::checkSpeakers()
    {
        if ((!bluetoothReady_ && !audioOutputsSupported()) || !partyMode_)
        {
            return;
        }

        const auto now = Clock::now();

        if (now - lastSpeakerCheck_ < SPEAKER_INTERVAL)
        {
            return;
        }

        lastSpeakerCheck_ = now;

        const bool playing = hasPlayback_ && playback_.playing;
        const bool native = bluetoothReady_;
        const bool tryReconnect = native && !lostSpeaker_.empty() && now - lastReconnectAttempt_ >= RECONNECT_INTERVAL;
        const std::string lost = lostSpeaker_;

        if (tryReconnect)
        {
            lastReconnectAttempt_ = now;
        }

        bluetoothQueue_.post([this, playing, tryReconnect, lost, native]
        {
            if (tryReconnect)
            {
                bluetooth_->reconnectDevice(lost);
            }

            std::vector<std::string> connected;

            if (native)
            {
                for (const auto& device : bluetooth_->getConnectedDevices())
                {
                    if (device.audioCapable)
                    {
                        connected.push_back(device.id);
                    }
                }
            }
            else
            {
                for (const auto& output : getAudioOutputDevices())
                {
                    if (output.bluetooth)
                    {
                        connected.push_back(output.id);
                    }
                }
            }

            main_.post([this, connected = std::move(connected), playing]() mutable
            {
                if (lostSpeaker_.empty())
                {
                    for (const auto& id : connectedSpeakers_)
                    {
                        if (std::find(connected.begin(), connected.end(), id) == connected.end())
                        {
                            lostSpeaker_ = id;
                            resumeAfterReconnect_ = playing;
                            lastReconnectAttempt_ = Clock::now() - RECONNECT_INTERVAL + std::chrono::seconds(3);
                            break;
                        }
                    }
                }
                else if (std::find(connected.begin(), connected.end(), lostSpeaker_) != connected.end())
                {
                    lostSpeaker_.clear();

                    if (resumeAfterReconnect_)
                    {
                        resumeAfterReconnect_ = false;
                        command([](BlitzDJ& dj) { return dj.play(); }, tr(Text::PartyResumed));
                    }
                    else
                    {
                        showToast(tr(Text::SpeakerReconnected));
                    }
                }

                connectedSpeakers_ = std::move(connected);
            });
        });
    }

    void App::loadOutputs()
    {
        if (!audioOutputsSupported() || loadingOutputs_)
        {
            return;
        }

        loadingOutputs_ = true;
        lastOutputsRefresh_ = Clock::now();

        bluetoothQueue_.post([this]
        {
            auto outputs = getAudioOutputDevices();

            std::stable_sort(outputs.begin(), outputs.end(), [](const AudioOutputDevice& a, const AudioOutputDevice& b)
            {
                return a.isDefault && !b.isDefault;
            });

            main_.post([this, outputs = std::move(outputs)]() mutable
            {
                loadingOutputs_ = false;
                outputsLoaded_ = true;
                outputs_ = std::move(outputs);
            });
        });
    }

    void App::startScan()
    {
        if (!bluetoothReady_ || scanning_)
        {
            return;
        }

        scanning_ = true;
        scanned_ = true;
        nearby_.clear();
        scanUntil_ = Clock::now() + SCAN_DURATION;
        lastNearbyRefresh_ = Clock::now();

        const auto bluetooth = bluetooth_.get();
        bluetoothQueue_.post([bluetooth] { bluetooth->startScan(); });
    }

    void App::updateScan()
    {
        if (!scanning_)
        {
            return;
        }

        const auto now = Clock::now();

        if (now >= scanUntil_)
        {
            scanning_ = false;

            const auto bluetooth = bluetooth_.get();
            bluetoothQueue_.post([bluetooth] { bluetooth->stopScan(); });
            loadNearby();
            loadSpeakers();
            return;
        }

        if (now - lastNearbyRefresh_ >= NEARBY_REFRESH)
        {
            lastNearbyRefresh_ = now;
            loadNearby();
        }
    }

    void App::loadNearby()
    {
        if (!bluetoothReady_ || loadingNearby_)
        {
            return;
        }

        loadingNearby_ = true;

        bluetoothQueue_.post([this]
        {
            auto devices = bluetooth_->getNearbyAudioDevices();

            main_.post([this, devices = std::move(devices)]() mutable
            {
                loadingNearby_ = false;
                nearby_ = std::move(devices);
            });
        });
    }

    void App::checkProximity()
    {
        if (!bluetoothReady_ || !proximityConnect_ || proximityBusy_ || scanning_)
        {
            return;
        }

        const auto now = Clock::now();

        if (now - lastProximityCheck_ < PROXIMITY_INTERVAL)
        {
            return;
        }

        lastProximityCheck_ = now;
        proximityBusy_ = true;

        const auto stopping = stopping_;

        bluetoothQueue_.post([this, stopping]
        {
            std::string connectedName;
            bool attempted = false;
            bool success = false;

            const auto connected = bluetooth_->getConnectedDevices();
            const bool hasSpeaker = std::any_of(connected.begin(), connected.end(), [](const BluetoothDevice& device)
            {
                return device.audioCapable;
            });

            if (!hasSpeaker && bluetooth_->startScan())
            {
                for (int i = 0; i < 16 && !stopping->load(); ++i)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                auto devices = bluetooth_->getNearbyAudioDevices();
                bluetooth_->stopScan();

                for (const auto& device : devices)
                {
                    if (stopping->load())
                    {
                        break;
                    }

                    if (device.paired && device.signalStrength != 0 && device.signalStrength >= PROXIMITY_RSSI)
                    {
                        attempted = true;
                        connectedName = device.name.empty() ? device.address : device.name;
                        success = bluetooth_->connectAudioDevice(device.id);
                        break;
                    }
                }
            }

            if (stopping->load())
            {
                return;
            }

            main_.post([this, attempted, success, connectedName]
            {
                proximityBusy_ = false;

                if (attempted)
                {
                    showToast(success ? format(tr(Text::ConnectingTo), connectedName) : bluetooth_->getLastErrorMessage());
                    loadSpeakers();
                }
            });
        });
    }

    void App::setProximityConnect(bool enabled)
    {
        proximityConnect_ = enabled;
        lastProximityCheck_ = Clock::now() - PROXIMITY_INTERVAL + std::chrono::seconds(2);
        saveSettings();
    }

    void App::setAutoDJ(bool enabled)
    {
        autoDJ_ = enabled;

        if (enabled)
        {
            applyQuery(queryBuffer_);
            command([](BlitzDJ& dj)
            {
                dj.setAutoDJ(true);
                return dj.start();
            }, nullptr, true);
        }
        else
        {
            network_.post([this] { dj_->stop(); });
        }
    }

    void App::setPartyMode(bool enabled)
    {
        partyMode_ = enabled;
        lostSpeaker_.clear();
        resumeAfterReconnect_ = false;
        connectedSpeakers_.clear();
        lastSpeakerCheck_ = Clock::time_point{};
        saveSettings();

        if (enabled && !autoDJ_)
        {
            setAutoDJ(true);
        }
    }

    void App::applyQuery(const std::string& query)
    {
        const std::string value = query.empty() ? "party hits" : query;
        std::snprintf(queryBuffer_, sizeof(queryBuffer_), "%s", value.c_str());
        dj_->setSearchQuery(value);
        saveSettings();
    }

    int App::currentProgress() const
    {
        if (!hasPlayback_)
        {
            return 0;
        }

        int progress = playback_.progressMs;

        if (playback_.playing)
        {
            progress += static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - playbackAt_).count());
        }

        return std::clamp(progress, 0, std::max(0, playback_.durationMs));
    }

    void App::update()
    {
        main_.drain();
        images_.uploadPending();

        if (authenticated_)
        {
            const auto now = Clock::now();

            if (now - lastPoll_ >= POLL_INTERVAL)
            {
                refreshPlayback();
            }

            if (hasPlayback_ && playback_.playing && playback_.durationMs > 0 && currentProgress() >= playback_.durationMs &&
                now - lastPoll_ > std::chrono::milliseconds(800))
            {
                refreshPlayback();
            }

            checkSpeakers();
        }

        updateScan();
        checkProximity();

        if (page_ == Page::Speakers && Clock::now() - lastOutputsRefresh_ >= std::chrono::seconds(4))
        {
            loadOutputs();

            if (bluetoothReady_ && !scanning_)
            {
                loadSpeakers();
            }
        }

        const auto art = hasPlayback_ ? images_.get(playback_.track.imageUrl, false) : Image{};
        targetArtColor_ = art.ready ? art.color : ImVec4(0.1f, 0.1f, 0.12f, 1.0f);

        const float blend = std::min(1.0f, ImGui::GetIO().DeltaTime * 3.0f);
        artColor_ = mix(artColor_, targetArtColor_, blend);
    }

    void App::setPage(Page page)
    {
        page_ = page;

        switch (page)
        {
            case Page::Player:
                loadQueue();
                break;

            case Page::AutoDJ:
                if (suggestions_.empty())
                {
                    loadSuggestions();
                }
                break;

            case Page::Search:
                break;

            case Page::Speakers:
                loadDevices();
                loadSpeakers();
                loadOutputs();
                break;
        }
    }

    void App::drawBackground()
    {
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        auto* list = ImGui::GetBackgroundDrawList();

        const ImVec4 tint = mix(BACKGROUND, artColor_, 0.55f);
        const ImVec4 middle = mix(BACKGROUND, artColor_, 0.18f);

        list->AddRectFilled(ImVec2(0, 0), size, color(BACKGROUND));
        list->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(size.x, size.y * 0.55f), color(tint), color(tint), color(middle), color(middle));
        list->AddRectFilledMultiColor(ImVec2(0, size.y * 0.55f), size, color(middle), color(middle), color(BACKGROUND), color(BACKGROUND));
    }

    void App::drawArt(const std::string& url, ImVec2 size, float rounding, bool thumbnail)
    {
        const ImVec2 position = ImGui::GetCursorScreenPos();
        const ImVec2 end(position.x + size.x, position.y + size.y);
        auto* list = ImGui::GetWindowDrawList();

        const auto image = images_.get(url, thumbnail);

        if (image.ready)
        {
            list->AddImageRounded(image.texture, position, end, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, rounding);
        }
        else
        {
            list->AddRectFilledMultiColor(position, end, color(SURFACE_HIGH), color(SURFACE), color(SURFACE), color(SURFACE_HIGH));
            list->AddRect(position, end, color(BACKGROUND), rounding, 0, rounding * 0.6f);
            drawIcon(list, Icon::Music, ImVec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f), size.x * 0.4f, color(TEXT_PRIMARY, 0.25f));
        }

        ImGui::Dummy(size);
    }

    void App::drawTrackList(const std::vector<SpotifyTrack>& tracks, const char* id, bool queueButton, bool playButton)
    {
        const float unit = ImGui::GetFontSize();
        const float rowHeight = unit * 3.6f;
        const float thumb = unit * 3.0f;
        const float width = ImGui::GetContentRegionAvail().x;
        const float buttonSize = unit * 2.3f;

        ImGui::PushID(id);

        for (std::size_t index = 0; index < tracks.size(); ++index)
        {
            const auto& track = tracks[index];
            ImGui::PushID(static_cast<int>(index));

            const ImVec2 start = ImGui::GetCursorScreenPos();
            const float buttons = (queueButton ? buttonSize : 0.0f) + (playButton ? buttonSize : 0.0f);

            ImGui::SetNextItemAllowOverlap();
            const bool clicked = ImGui::InvisibleButton("row", ImVec2(width, rowHeight));
            const bool hovered = ImGui::IsItemHovered();

            if (hovered)
            {
                ImGui::GetWindowDrawList()->AddRectFilled(start, ImVec2(start.x + width, start.y + rowHeight), color(TEXT_PRIMARY, 0.05f), unit * 0.6f);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::SetCursorScreenPos(ImVec2(start.x + unit * 0.3f, start.y + (rowHeight - thumb) * 0.5f));
            drawArt(track.imageUrl, ImVec2(thumb, thumb), unit * 0.5f, true);

            const float textX = start.x + unit * 0.3f + thumb + unit * 0.8f;
            const float textWidth = width - (textX - start.x) - buttons - unit * 0.5f;

            ImGui::SetCursorScreenPos(ImVec2(textX, start.y + rowHeight * 0.5f - unit * 1.15f));
            text(track.name, TEXT_PRIMARY, textWidth);

            ImGui::SetCursorScreenPos(ImVec2(textX, start.y + rowHeight * 0.5f + unit * 0.1f));
            ImGui::PushFont(fonts_.small);
            text(track.album.empty() ? track.artist : track.artist + "  •  " + track.album, TEXT_SECONDARY, textWidth);
            ImGui::PopFont();

            float buttonX = start.x + width - buttons;
            const float buttonY = start.y + (rowHeight - buttonSize) * 0.5f;

            if (queueButton)
            {
                ImGui::SetCursorScreenPos(ImVec2(buttonX, buttonY));

                if (iconButton("queue", Icon::QueueAdd, buttonSize, TEXT_SECONDARY, 0.5f))
                {
                    const std::string uri = track.uri;
                    command([uri](BlitzDJ& dj) { return dj.queueTrack(uri); }, tr(Text::Queued));
                    loadQueue();
                }

                buttonX += buttonSize;
            }

            if (playButton)
            {
                ImGui::SetCursorScreenPos(ImVec2(buttonX, buttonY));

                if (iconButton("play", Icon::Play, buttonSize, ACCENT, 0.5f))
                {
                    const std::string uri = track.uri;
                    command([uri](BlitzDJ& dj) { return dj.playTrack(uri); });
                }
            }

            if (clicked && !ImGui::IsAnyItemActive())
            {
                const std::string uri = track.uri;
                command([uri](BlitzDJ& dj) { return dj.playTrack(uri); });
            }

            ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + rowHeight));
            ImGui::Dummy(ImVec2(0, 0));
            ImGui::PopID();
        }

        ImGui::PopID();
    }

    void App::drawLogin()
    {
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        const float unit = ImGui::GetFontSize();
        const float width = std::min(size.x - unit * 4.0f, unit * 24.0f);
        const float x = (size.x - width) * 0.5f;

        ImGui::SetCursorPos(ImVec2(x, std::max(unit * 2.0f, size.y * 0.22f)));
        ImGui::BeginGroup();

        const float logo = unit * 6.0f;
        const ImVec2 logoPosition(x + (width - logo) * 0.5f, ImGui::GetCursorScreenPos().y);
        auto* list = ImGui::GetWindowDrawList();
        list->AddRectFilled(logoPosition, ImVec2(logoPosition.x + logo, logoPosition.y + logo), color(ACCENT), logo * 0.24f);
        drawIcon(list, Icon::Bolt, ImVec2(logoPosition.x + logo * 0.5f, logoPosition.y + logo * 0.5f), logo * 0.62f, color(ON_ACCENT));
        ImGui::Dummy(ImVec2(width, logo + unit * 1.2f));

        ImGui::PushFont(fonts_.display);
        const float titleWidth = ImGui::CalcTextSize("BlitzDJ").x;
        ImGui::SetCursorPosX(x + (width - titleWidth) * 0.5f);
        ImGui::TextColored(ACCENT, "BlitzDJ");
        ImGui::PopFont();

        const float taglineWidth = std::min(width, ImGui::CalcTextSize(tr(Text::Tagline)).x);
        ImGui::SetCursorPosX(x + (width - taglineWidth) * 0.5f);
        wrappedText(tr(Text::Tagline), TEXT_SECONDARY, taglineWidth);
        ImGui::Dummy(ImVec2(0, unit * 2.0f));

        ImGui::SetCursorPosX(x);

        if (loggingIn_)
        {
            const float waitingWidth = ImGui::CalcTextSize(tr(Text::WaitingBrowser)).x;
            ImGui::SetCursorPosX(x + (width - waitingWidth) * 0.5f);
            ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::WaitingBrowser));
            ImGui::Dummy(ImVec2(0, unit * 0.6f));
            ImGui::SetCursorPosX(x);

            if (pillButton(tr(Text::Cancel), false, width, unit * 3.0f))
            {
                cancelLogin();
            }
        }
        else if (manualLogin_)
        {
            ImGui::SetCursorPosX(x);
            ImGui::PushFont(fonts_.title);
            ImGui::TextColored(ACCENT, "%s", tr(Text::ManualLoginTitle));
            ImGui::PopFont();
            ImGui::SetCursorPosX(x);
            ImGui::PushFont(fonts_.small);
            wrappedText(tr(Text::ManualLoginInstructions), TEXT_SECONDARY, width);
            ImGui::PopFont();

            ImGui::Dummy(ImVec2(0, unit * 0.6f));
            ImGui::SetCursorPosX(x);
            ImGui::SetNextItemWidth(width);

            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }

            if (ImGui::InputTextWithHint("##paste", tr(Text::PasteUrlHint), pasteBuffer_, sizeof(pasteBuffer_), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                finishManualLogin();
            }

            ImGui::Dummy(ImVec2(0, unit * 0.4f));
            ImGui::SetCursorPosX(x);

            if (pillButton(tr(Text::FinishLogin), true, width, unit * 3.0f))
            {
                finishManualLogin();
            }

            ImGui::Dummy(ImVec2(0, unit * 0.3f));
            ImGui::SetCursorPosX(x);

            if (pillButton(tr(Text::OpenBrowserAgain), false, width, unit * 2.8f))
            {
                openUrl(manualLoginUrl_);
            }
        }
        else if (pillButton(tr(Text::ConnectSpotify), true, width, unit * 3.2f))
        {
            startLogin();
        }

        if (!loginError_.empty() && !loggingIn_)
        {
            ImGui::Dummy(ImVec2(0, unit * 0.6f));
            ImGui::SetCursorPosX(x);
            wrappedText(loginError_.c_str(), ImVec4(1.0f, 0.45f, 0.45f, 1.0f), width);
        }

        ImGui::Dummy(ImVec2(0, unit * 1.2f));
        ImGui::PushFont(fonts_.small);
        const std::string redirect = "Redirect URI: " + configuredRedirect_;
        const float redirectWidth = ImGui::CalcTextSize(redirect.c_str()).x;
        ImGui::SetCursorPosX(x + std::max(0.0f, (width - redirectWidth) * 0.5f));
        ImGui::TextColored(TEXT_SECONDARY, "%s", redirect.c_str());
        ImGui::PopFont();

        ImGui::EndGroup();
    }

    void App::drawPlayer(float width)
    {
        const float unit = ImGui::GetFontSize();
        const auto& track = playback_.track;
        const ImVec4 accent = hasPlayback_ ? readableAccent(artColor_) : ACCENT;

        ImGui::Dummy(ImVec2(0, unit * 0.4f));

        const float chipWidth = std::min(width * 0.45f, unit * 11.0f);
        ImGui::BeginGroup();
        ImGui::PushFont(fonts_.small);
        ImGui::TextColored(TEXT_SECONDARY, "%s", tr(Text::NowPlaying));
        ImGui::PopFont();

        const std::string device = playback_.device.name;
        text(hasPlayback_ && !device.empty() ? format(tr(Text::PlayingOn), device) : "BlitzDJ", TEXT_PRIMARY, width - chipWidth - unit);
        ImGui::EndGroup();

        ImGui::SameLine(ImGui::GetCursorStartPos().x + width - chipWidth);
        {
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const float height = unit * 2.2f;

            if (ImGui::InvisibleButton("device", ImVec2(chipWidth, height)))
            {
                setPage(Page::Speakers);
            }

            const bool hovered = ImGui::IsItemHovered();
            auto* list = ImGui::GetWindowDrawList();
            list->AddRectFilled(position, ImVec2(position.x + chipWidth, position.y + height), color(hovered ? OUTLINE : SURFACE_HIGH), height * 0.5f);
            drawIcon(list, deviceIcon(playback_.device.type), ImVec2(position.x + height * 0.55f, position.y + height * 0.5f), height * 0.5f, color(accent));

            const std::string label = fitText(device.empty() ? tr(Text::Speakers) : device, chipWidth - height * 1.3f);
            list->AddText(ImVec2(position.x + height * 1.05f, position.y + (height - unit) * 0.5f), color(TEXT_PRIMARY), label.c_str());

            if (hovered)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
        }

        ImGui::Dummy(ImVec2(0, unit * 1.0f));

        const float available = ImGui::GetContentRegionAvail().y;
        const float artSize = std::clamp(std::min(width, available * 0.5f), unit * 10.0f, width);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - artSize) * 0.5f);

        {
            const ImVec2 position = ImGui::GetCursorScreenPos();
            auto* list = ImGui::GetWindowDrawList();

            for (int i = 1; i <= 4; ++i)
            {
                const float spread = unit * 0.35f * static_cast<float>(i);
                list->AddRectFilled(
                    ImVec2(position.x - spread * 0.5f, position.y + spread * 0.3f),
                    ImVec2(position.x + artSize + spread * 0.5f, position.y + artSize + spread * 1.2f),
                    IM_COL32(0, 0, 0, 22),
                    unit * 1.4f + spread
                );
            }
        }

        drawArt(hasPlayback_ ? track.imageUrl : std::string(), ImVec2(artSize, artSize), unit * 1.4f, false);

        ImGui::Dummy(ImVec2(0, unit * 1.0f));

        const float heartSize = unit * 2.4f;
        ImGui::BeginGroup();
        ImGui::PushFont(fonts_.headline);
        text(hasPlayback_ ? track.name : tr(Text::NothingPlaying), TEXT_PRIMARY, width - heartSize - unit);
        ImGui::PopFont();
        text(hasPlayback_ ? track.artist : tr(Text::OpenSpotifyHint), TEXT_SECONDARY, width - heartSize - unit);

        if (hasPlayback_ && !track.album.empty())
        {
            ImGui::PushFont(fonts_.small);
            text(track.album, TEXT_SECONDARY, width - heartSize - unit);
            ImGui::PopFont();
        }

        ImGui::EndGroup();

        if (hasPlayback_)
        {
            ImGui::SameLine(ImGui::GetCursorStartPos().x + width - heartSize);

            if (iconButton("favorite", Icon::Heart, heartSize, TEXT_SECONDARY, 0.5f))
            {
                const std::string artist = track.artist.substr(0, track.artist.find(','));
                dj_->ai().addFavoriteArtist(artist);
                saveSettings();
                showToast(format(tr(Text::FavoriteAdded), artist));
            }
        }

        ImGui::Dummy(ImVec2(0, unit * 0.6f));

        const int progress = currentProgress();
        const int duration = hasPlayback_ ? playback_.durationMs : 0;

        if (!seeking_)
        {
            seekFraction_ = duration > 0 ? static_cast<float>(progress) / static_cast<float>(duration) : 0.0f;
        }

        if (seekBar("seek", seekFraction_, width, accent, seeking_) && duration > 0)
        {
            const int position = static_cast<int>(seekFraction_ * static_cast<float>(duration));
            playback_.progressMs = position;
            playbackAt_ = Clock::now();
            command([position](BlitzDJ& dj) { return dj.spotify().seek(position); });
        }

        ImGui::PushFont(fonts_.small);
        const int shown = seeking_ ? static_cast<int>(seekFraction_ * static_cast<float>(duration)) : progress;
        ImGui::TextColored(TEXT_SECONDARY, "%s", formatTime(shown).c_str());
        const std::string total = formatTime(duration);
        ImGui::SameLine(ImGui::GetCursorStartPos().x + width - ImGui::CalcTextSize(total.c_str()).x);
        ImGui::TextColored(TEXT_SECONDARY, "%s", total.c_str());
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, unit * 0.4f));

        const float small = unit * 2.6f;
        const float medium = unit * 3.4f;
        const float large = unit * 4.8f;
        const float rowY = ImGui::GetCursorPosY();
        const float startX = ImGui::GetCursorPosX();

        ImGui::SetCursorPos(ImVec2(startX, rowY + (large - small) * 0.5f));

        if (iconButton("shuffle", Icon::Shuffle, small, playback_.shuffle ? accent : TEXT_SECONDARY, 0.55f))
        {
            const bool enabled = !playback_.shuffle;
            playback_.shuffle = enabled;
            command([enabled](BlitzDJ& dj) { return dj.spotify().setShuffle(enabled); });
        }

        const float centerX = startX + width * 0.5f;

        ImGui::SetCursorPos(ImVec2(centerX - large * 0.5f - unit * 1.2f - medium, rowY + (large - medium) * 0.5f));

        if (iconButton("previous", Icon::Previous, medium, TEXT_PRIMARY, 0.55f))
        {
            command([](BlitzDJ& dj) { return dj.previous(); });
        }

        ImGui::SetCursorPos(ImVec2(centerX - large * 0.5f, rowY));

        if (circleButton("playpause", playback_.playing ? Icon::Pause : Icon::Play, large, ACCENT, ON_ACCENT))
        {
            const bool pause = playback_.playing;
            playback_.playing = !pause;
            playback_.progressMs = progress;
            playbackAt_ = Clock::now();
            command([pause](BlitzDJ& dj) { return pause ? dj.pause() : dj.play(); });
        }

        ImGui::SetCursorPos(ImVec2(centerX + large * 0.5f + unit * 1.2f, rowY + (large - medium) * 0.5f));

        if (iconButton("next", Icon::Next, medium, TEXT_PRIMARY, 0.55f))
        {
            command([](BlitzDJ& dj) { return dj.skip(); });
        }

        ImGui::SetCursorPos(ImVec2(startX + width - small, rowY + (large - small) * 0.5f));

        const bool repeatOn = playback_.repeatMode != "off";

        if (iconButton("repeat", playback_.repeatMode == "track" ? Icon::RepeatOne : Icon::Repeat, small, repeatOn ? accent : TEXT_SECONDARY, 0.55f))
        {
            const std::string next = playback_.repeatMode == "off" ? "context" : (playback_.repeatMode == "context" ? "track" : "off");
            playback_.repeatMode = next;
            command([next](BlitzDJ& dj) { return dj.spotify().setRepeat(next); });
        }

        ImGui::SetCursorPos(ImVec2(startX, rowY + large + unit * 0.8f));

        {
            const float iconSize = unit * 1.6f;
            const ImVec2 position = ImGui::GetCursorScreenPos();
            drawIcon(ImGui::GetWindowDrawList(), Icon::Volume, ImVec2(position.x + iconSize * 0.5f, position.y + unit * 0.62f), iconSize, color(TEXT_SECONDARY));
            ImGui::Dummy(ImVec2(iconSize, 0));
            ImGui::SameLine(0, unit * 0.6f);

            const float sliderWidth = width - iconSize - unit * 3.6f;

            if (seekBar("volume", volumeFraction_, sliderWidth, TEXT_PRIMARY, changingVolume_))
            {
                const int volume = static_cast<int>(std::round(volumeFraction_ * 100.0f));
                command([volume](BlitzDJ& dj) { return dj.spotify().setVolume(volume); }, nullptr, false);
            }

            ImGui::SameLine(0, unit * 0.6f);
            ImGui::PushFont(fonts_.small);
            ImGui::TextColored(TEXT_SECONDARY, "%d%%", static_cast<int>(std::round(volumeFraction_ * 100.0f)));
            ImGui::PopFont();
        }

        ImGui::Dummy(ImVec2(0, unit * 0.8f));

        if (beginCard("autodj_card", width))
        {
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const float iconSize = unit * 1.8f;
            drawIcon(ImGui::GetWindowDrawList(), Icon::Bolt, ImVec2(position.x + iconSize * 0.5f, position.y + unit * 1.2f), iconSize, color(autoDJ_ ? ACCENT : TEXT_SECONDARY));
            ImGui::Dummy(ImVec2(iconSize, 0));
            ImGui::SameLine(0, unit * 0.8f);

            const float cardInner = ImGui::GetContentRegionAvail().x;
            const float cardEnd = ImGui::GetCursorPosX() + cardInner;
            const float toggleWidth = ImGui::GetFrameHeight() * 1.8f;

            ImGui::BeginGroup();
            ImGui::PushFont(fonts_.title);
            ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::AutoDJ));
            ImGui::PopFont();
            ImGui::PushFont(fonts_.small);
            text(autoDJ_ ? format(tr(Text::AutoDJOn), dj_->getSearchQuery()) : tr(Text::AutoDJOff), TEXT_SECONDARY, cardInner - toggleWidth - unit);
            ImGui::PopFont();
            ImGui::EndGroup();

            if (ImGui::IsItemClicked())
            {
                setPage(Page::AutoDJ);
            }

            ImGui::SameLine(cardEnd - toggleWidth);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + unit * 0.3f);

            if (toggle("autodj_toggle", autoDJ_))
            {
                setAutoDJ(!autoDJ_);
            }
        }

        endCard();

        if (!queue_.empty())
        {
            ImGui::Dummy(ImVec2(0, unit * 0.8f));
            ImGui::PushFont(fonts_.title);
            ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::UpNext));
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, unit * 0.2f));
            drawTrackList(queue_, "queue", false, true);
        }

        ImGui::Dummy(ImVec2(0, unit));
    }

    void App::drawAutoDJ(float width)
    {
        const float unit = ImGui::GetFontSize();

        ImGui::Dummy(ImVec2(0, unit * 0.4f));
        ImGui::PushFont(fonts_.headline);
        ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::AutoDJ));
        ImGui::PopFont();
        wrappedText(tr(Text::AutoDJDescription), TEXT_SECONDARY, width);
        ImGui::Dummy(ImVec2(0, unit * 0.6f));

        if (beginCard("autodj_settings", width))
        {
            const float inner = ImGui::GetContentRegionAvail().x;
            const float lineEnd = ImGui::GetCursorPosX() + inner;

            ImGui::PushFont(fonts_.title);
            ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::AutoDJEnable));
            ImGui::PopFont();
            ImGui::SameLine(lineEnd - ImGui::GetFrameHeight() * 1.8f);

            if (toggle("autodj_enable", autoDJ_))
            {
                setAutoDJ(!autoDJ_);
            }

            ImGui::Dummy(ImVec2(0, unit * 0.5f));
            ImGui::SetNextItemWidth(inner);

            if (ImGui::InputTextWithHint("##query", tr(Text::AutoDJQueryHint), queryBuffer_, sizeof(queryBuffer_), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                applyQuery(queryBuffer_);
                loadSuggestions();
            }

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                applyQuery(queryBuffer_);
            }

            ImGui::Dummy(ImVec2(0, unit * 0.3f));

            const auto& moods = isPortuguese() ? MOODS_PT : MOODS_EN;
            float lineWidth = 0.0f;

            for (int i = 0; i < 8; ++i)
            {
                const float chipWidth = ImGui::CalcTextSize(moods[i]).x + ImGui::GetStyle().FramePadding.x * 2.0f;

                if (i > 0 && lineWidth + chipWidth + unit * 0.5f <= inner)
                {
                    ImGui::SameLine(0, unit * 0.5f);
                    lineWidth += chipWidth + unit * 0.5f;
                }
                else
                {
                    lineWidth = chipWidth;
                }

                if (chip(moods[i], dj_->getSearchQuery() == moods[i]))
                {
                    applyQuery(moods[i]);
                    loadSuggestions();
                }
            }

            ImGui::Dummy(ImVec2(0, unit * 0.4f));

            if (pillButton(tr(Text::QueueFive), true, inner, unit * 2.8f))
            {
                applyQuery(queryBuffer_);
                command([](BlitzDJ& dj) { return dj.queueRecommendedTracks(5); }, tr(Text::Done));
                loadQueue();
            }
        }

        endCard();

        ImGui::Dummy(ImVec2(0, unit * 0.8f));
        ImGui::PushFont(fonts_.title);
        ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::Suggestions));
        ImGui::PopFont();
        ImGui::SameLine(ImGui::GetCursorStartPos().x + width - unit * 2.4f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - unit * 0.4f);

        if (iconButton("refresh_suggestions", Icon::Refresh, unit * 2.4f, TEXT_SECONDARY, 0.55f))
        {
            loadSuggestions();
        }

        if (loadingSuggestions_)
        {
            ImGui::TextColored(TEXT_SECONDARY, "...");
        }
        else if (suggestions_.empty())
        {
            wrappedText(tr(Text::SuggestionsEmpty), TEXT_SECONDARY, width);
        }
        else
        {
            drawTrackList(suggestions_, "suggestions", true, true);
        }

        ImGui::Dummy(ImVec2(0, unit));
    }

    void App::drawSearch(float width)
    {
        const float unit = ImGui::GetFontSize();

        ImGui::Dummy(ImVec2(0, unit * 0.4f));
        ImGui::PushFont(fonts_.headline);
        ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::Search));
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, unit * 0.4f));

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(unit * 2.4f, unit * 0.8f));
        ImGui::SetNextItemWidth(width);

        const ImVec2 position = ImGui::GetCursorScreenPos();

        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputTextWithHint("##search", tr(Text::SearchHint), searchBuffer_, sizeof(searchBuffer_), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            search();
        }

        const float height = ImGui::GetItemRectSize().y;
        ImGui::PopStyleVar();

        drawIcon(ImGui::GetWindowDrawList(), Icon::Search, ImVec2(position.x + unit * 1.3f, position.y + height * 0.5f), unit * 1.4f, color(TEXT_SECONDARY));

        ImGui::Dummy(ImVec2(0, unit * 0.6f));

        if (searching_)
        {
            ImGui::TextColored(TEXT_SECONDARY, "...");
        }
        else if (results_.empty())
        {
            wrappedText(searched_ ? tr(Text::NoResults) : tr(Text::SearchEmpty), TEXT_SECONDARY, width);
        }
        else
        {
            drawTrackList(results_, "results", true, true);
        }

        ImGui::Dummy(ImVec2(0, unit));
    }

    void App::drawSpeakers(float width)
    {
        const float unit = ImGui::GetFontSize();
        const float rowHeight = unit * 3.4f;
        const float iconSize = unit * 2.6f;

        ImGui::Dummy(ImVec2(0, unit * 0.4f));
        ImGui::PushFont(fonts_.headline);
        ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::Speakers));
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, unit * 0.4f));

        auto deviceRow = [&](const char* id, Icon icon, const std::string& name, const std::string& subtitle, bool highlighted, const char* action) -> bool
        {
            ImGui::PushID(id);

            const ImVec2 start = ImGui::GetCursorScreenPos();
            const float inner = ImGui::GetContentRegionAvail().x;
            auto* list = ImGui::GetWindowDrawList();

            const ImVec2 iconCenter(start.x + iconSize * 0.5f, start.y + rowHeight * 0.5f);
            list->AddCircleFilled(iconCenter, iconSize * 0.5f, color(highlighted ? ImVec4(0.24f, 0.18f, 0.0f, 1.0f) : SURFACE_HIGH), 32);
            drawIcon(list, icon, iconCenter, iconSize * 0.55f, color(highlighted ? ACCENT : TEXT_PRIMARY));

            float actionWidth = 0.0f;

            if (action)
            {
                actionWidth = ImGui::CalcTextSize(action).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            }

            const float textX = start.x + iconSize + unit * 0.8f;
            const float textWidth = inner - iconSize - unit * 1.6f - actionWidth;

            ImGui::SetCursorScreenPos(ImVec2(textX, start.y + rowHeight * 0.5f - unit * 1.15f));
            text(name, TEXT_PRIMARY, textWidth);
            ImGui::SetCursorScreenPos(ImVec2(textX, start.y + rowHeight * 0.5f + unit * 0.1f));
            ImGui::PushFont(fonts_.small);
            text(subtitle, highlighted ? ACCENT : TEXT_SECONDARY, textWidth);
            ImGui::PopFont();

            bool pressed = false;

            if (action)
            {
                ImGui::SetCursorScreenPos(ImVec2(start.x + inner - actionWidth, start.y + (rowHeight - ImGui::GetFrameHeight()) * 0.5f));
                pressed = pillButton(action, false);
            }

            ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + rowHeight));
            ImGui::Dummy(ImVec2(0, 0));
            ImGui::PopID();
            return pressed;
        };

        if (beginCard("output", width))
        {
            ImGui::PushFont(fonts_.small);
            ImGui::TextColored(TEXT_SECONDARY, "%s", tr(Text::CurrentOutput));
            ImGui::PopFont();

            std::string output = playback_.device.name;
            std::string outputType = playback_.device.type;
            Icon outputIcon = deviceIcon(playback_.device.type);

            for (const auto& speaker : speakers_)
            {
                if (speaker.connected)
                {
                    output = speaker.name;
                    outputType = "Bluetooth";
                    outputIcon = Icon::Bluetooth;
                    break;
                }
            }

            for (const auto& device : outputs_)
            {
                if (device.isDefault)
                {
                    output = device.name;
                    outputType = device.bluetooth ? "Bluetooth" : tr(Text::SystemOutput);
                    outputIcon = device.bluetooth ? Icon::Bluetooth : Icon::Volume;
                    break;
                }
            }

            const char* action = audioOutputsSupported() ? tr(Text::SoundSettings) : nullptr;

            if (deviceRow("current", outputIcon, output.empty() ? std::string(tr(Text::NothingPlaying)) : output, outputType, true, action))
            {
                openSoundSettings();
            }
        }

        endCard();

        ImGui::Dummy(ImVec2(0, unit * 0.6f));

        if (beginCard("party", width))
        {
            const float lineEnd = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::PushFont(fonts_.title);
            ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::PartyMode));
            ImGui::PopFont();
            ImGui::SameLine(lineEnd - ImGui::GetFrameHeight() * 1.8f);

            if (toggle("party_toggle", partyMode_))
            {
                setPartyMode(!partyMode_);
            }

            ImGui::PushFont(fonts_.small);
            wrappedText(tr(Text::PartyModeDescription), TEXT_SECONDARY, ImGui::GetContentRegionAvail().x);
            ImGui::PopFont();
        }

        endCard();

        ImGui::Dummy(ImVec2(0, unit * 0.8f));
        ImGui::PushFont(fonts_.title);
        ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::BluetoothSpeakers));
        ImGui::PopFont();

        if (bluetoothReady_)
        {
            ImGui::SameLine(ImGui::GetCursorStartPos().x + width - unit * 2.4f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - unit * 0.4f);

            if (iconButton("refresh_speakers", Icon::Refresh, unit * 2.4f, TEXT_SECONDARY, 0.55f))
            {
                const auto bluetooth = bluetooth_.get();
                bluetoothQueue_.post([bluetooth]
                {
                    bluetooth->startScan();
                });
                loadSpeakers();
            }

            if (speakers_.empty())
            {
                wrappedText(tr(Text::BluetoothEmpty), TEXT_SECONDARY, width);
            }

            for (const auto& speaker : speakers_)
            {
                const std::string id = speaker.id;
                const bool connected = speaker.connected;
                std::string subtitle = connected ? tr(Text::Connected) : tr(Text::Paired);

                if (speaker.batteryLevel >= 0)
                {
                    subtitle += "  •  " + std::to_string(speaker.batteryLevel) + "%";
                }

                if (deviceRow(id.c_str(), Icon::Bluetooth, speaker.name.empty() ? speaker.address : speaker.name, subtitle, connected,
                        connected ? tr(Text::Disconnect) : tr(Text::Connect)))
                {
                    const auto bluetooth = bluetooth_.get();

                    bluetoothQueue_.post([this, bluetooth, id, connected]
                    {
                        const bool ok = connected ? bluetooth->disconnectAudioDevice(id) : bluetooth->connectAudioDevice(id);
                        const std::string error = ok ? std::string() : bluetooth->getLastErrorMessage();

                        main_.post([this, error]
                        {
                            showToast(error);
                            loadSpeakers();
                        });
                    });
                }
            }

            ImGui::Dummy(ImVec2(0, unit * 0.8f));
            ImGui::PushFont(fonts_.title);
            ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::NearbySpeakers));
            ImGui::PopFont();

            {
                const char* label = scanning_ ? tr(Text::Scanning) : tr(Text::Scan);
                const float buttonWidth = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                ImGui::SameLine(ImGui::GetCursorStartPos().x + width - buttonWidth);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - unit * 0.3f);

                if (pillButton(label, !scanning_))
                {
                    if (scanning_)
                    {
                        scanUntil_ = Clock::now();
                    }
                    else
                    {
                        startScan();
                    }
                }
            }

            if (scanning_)
            {
                const ImVec2 position = ImGui::GetCursorScreenPos();
                const float phase = std::fmod(static_cast<float>(ImGui::GetTime()) * 0.8f, 1.3f);
                auto* list = ImGui::GetWindowDrawList();
                const float barHeight = unit * 0.2f;
                list->AddRectFilled(position, ImVec2(position.x + width, position.y + barHeight), color(TEXT_PRIMARY, 0.1f), barHeight);
                const float start = position.x + width * std::clamp(phase - 0.3f, 0.0f, 1.0f);
                const float end = position.x + width * std::clamp(phase, 0.0f, 1.0f);
                list->AddRectFilled(ImVec2(start, position.y), ImVec2(end, position.y + barHeight), color(ACCENT), barHeight);
                ImGui::Dummy(ImVec2(width, barHeight + unit * 0.3f));
            }

            if (nearby_.empty())
            {
                wrappedText(scanned_ && !scanning_ ? tr(Text::NearbyNoneFound) : tr(Text::NearbyEmpty), TEXT_SECONDARY, width);
            }

            for (const auto& device : nearby_)
            {
                const std::string id = device.id;
                const std::string name = device.name.empty() ? device.address : device.name;
                std::string subtitle;

                if (device.connected)
                {
                    subtitle = tr(Text::Connected);
                }
                else
                {
                    subtitle = std::string(tr(proximityText(device.signalStrength))) + "  •  " + std::to_string(device.signalStrength) + " dBm  •  ";
                    subtitle += device.paired ? tr(Text::Paired) : tr(Text::Pair);
                }

                const ImVec2 rowStart = ImGui::GetCursorScreenPos();
                const bool veryClose = device.connected || device.signalStrength >= -55;

                if (deviceRow(("nearby_" + id).c_str(), Icon::Bluetooth, name, subtitle, veryClose,
                        device.connected ? nullptr : (device.paired ? tr(Text::Connect) : tr(Text::Pair))))
                {
                    showToast(format(tr(Text::ConnectingTo), name));
                    scanUntil_ = Clock::now();

                    const auto bluetooth = bluetooth_.get();

                    bluetoothQueue_.post([this, bluetooth, id]
                    {
                        const bool ok = bluetooth->connectAudioDevice(id);
                        const std::string error = ok ? std::string() : bluetooth->getLastErrorMessage();

                        main_.post([this, error]
                        {
                            showToast(error);
                            loadSpeakers();
                            loadNearby();
                        });
                    });
                }

                if (!device.connected)
                {
                    const int bars = signalBars(device.signalStrength);
                    auto* list = ImGui::GetWindowDrawList();
                    const float barWidth = unit * 0.2f;
                    const float baseX = rowStart.x + iconSize - barWidth * 4.0f;
                    const float baseY = rowStart.y + rowHeight - unit * 0.35f;

                    for (int bar = 0; bar < 4; ++bar)
                    {
                        const float barHeight = unit * 0.22f * static_cast<float>(bar + 1);
                        const float x = baseX + static_cast<float>(bar) * barWidth * 1.3f;
                        const bool lit = bar < bars;
                        list->AddRectFilled(ImVec2(x, baseY - barHeight), ImVec2(x + barWidth, baseY), color(lit ? ACCENT : TEXT_SECONDARY, lit ? 1.0f : 0.35f), barWidth * 0.3f);
                    }
                }
            }

            ImGui::Dummy(ImVec2(0, unit * 0.6f));

            if (beginCard("proximity", width))
            {
                const float lineEnd = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
                ImGui::PushFont(fonts_.title);
                ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::ProximityConnect));
                ImGui::PopFont();
                ImGui::SameLine(lineEnd - ImGui::GetFrameHeight() * 1.8f);

                if (toggle("proximity_toggle", proximityConnect_))
                {
                    setProximityConnect(!proximityConnect_);
                }

                ImGui::PushFont(fonts_.small);
                wrappedText(tr(Text::ProximityConnectDescription), TEXT_SECONDARY, ImGui::GetContentRegionAvail().x);
                ImGui::PopFont();
            }

            endCard();
        }
        else if (audioOutputsSupported())
        {
            ImGui::SameLine(ImGui::GetCursorStartPos().x + width - unit * 2.4f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - unit * 0.4f);

            if (iconButton("refresh_outputs", Icon::Refresh, unit * 2.4f, TEXT_SECONDARY, 0.55f))
            {
                loadOutputs();
            }

            bool any = false;

            for (const auto& device : outputs_)
            {
                if (!device.bluetooth)
                {
                    continue;
                }

                any = true;

                deviceRow(("output_" + device.id).c_str(), Icon::Bluetooth, device.name,
                    device.isDefault ? tr(Text::PlayingAudio) : tr(Text::Connected), device.isDefault, nullptr);
            }

            if (!any && outputsLoaded_)
            {
                wrappedText(tr(Text::NoConnectedSpeakers), TEXT_SECONDARY, width);
            }

            ImGui::Dummy(ImVec2(0, unit * 0.3f));

            if (pillButton(tr(Text::BluetoothOpenSettings), false))
            {
                openBluetoothSettings();
            }
        }
        else
        {
            wrappedText(tr(Text::BluetoothUnsupported), TEXT_SECONDARY, width);
            ImGui::Dummy(ImVec2(0, unit * 0.3f));

            if (pillButton(tr(Text::BluetoothOpenSettings), false))
            {
                openBluetoothSettings();
            }
        }

        ImGui::Dummy(ImVec2(0, unit * 0.8f));
        ImGui::PushFont(fonts_.title);
        ImGui::TextColored(TEXT_PRIMARY, "%s", tr(Text::SpotifyConnect));
        ImGui::PopFont();
        ImGui::SameLine(ImGui::GetCursorStartPos().x + width - unit * 2.4f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - unit * 0.4f);

        if (iconButton("refresh_devices", Icon::Refresh, unit * 2.4f, TEXT_SECONDARY, 0.55f))
        {
            loadDevices();
        }

        if (devices_.empty())
        {
            wrappedText(tr(Text::NoDevices), TEXT_SECONDARY, width);
        }

        for (const auto& device : devices_)
        {
            std::string subtitle = device.type;

            if (device.active)
            {
                subtitle += "  •  ";
                subtitle += tr(Text::Active);
            }

            subtitle += "  •  " + std::to_string(device.volume) + "%";

            const std::string id = device.id;

            if (deviceRow(("device_" + id).c_str(), deviceIcon(device.type), device.name, subtitle, device.active,
                    device.active || id.empty() ? nullptr : tr(Text::PlayHere)))
            {
                command([id](BlitzDJ& dj) { return dj.spotify().transferPlayback(id, true); });
                loadDevices();
            }
        }

        ImGui::Dummy(ImVec2(0, unit * 1.2f));

        if (pillButton(tr(Text::OpenSpotify), false, width, unit * 2.8f))
        {
            openUrl("https://open.spotify.com");
        }

        ImGui::Dummy(ImVec2(0, unit * 0.4f));

        if (pillButton(tr(Text::Logout), false, width, unit * 2.8f))
        {
            logout();
        }

        ImGui::Dummy(ImVec2(0, unit));
    }

    void App::drawMiniPlayer(float width)
    {
        const float unit = ImGui::GetFontSize();
        const float height = unit * 4.0f;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        auto* list = ImGui::GetWindowDrawList();

        list->AddRectFilled(start, ImVec2(start.x + width, start.y + height), color(SURFACE_HIGH), unit * 0.9f);

        const int duration = playback_.durationMs;
        const float fraction = duration > 0 ? static_cast<float>(currentProgress()) / static_cast<float>(duration) : 0.0f;
        list->AddRectFilled(
            ImVec2(start.x + unit * 0.6f, start.y + height - unit * 0.18f),
            ImVec2(start.x + unit * 0.6f + (width - unit * 1.2f) * fraction, start.y + height - unit * 0.06f),
            color(readableAccent(artColor_)),
            unit
        );

        ImGui::SetNextItemAllowOverlap();

        if (ImGui::InvisibleButton("mini_open", ImVec2(width, height)))
        {
            setPage(Page::Player);
        }

        const float art = unit * 2.8f;
        ImGui::SetCursorScreenPos(ImVec2(start.x + unit * 0.6f, start.y + (height - art) * 0.5f));
        drawArt(playback_.track.imageUrl, ImVec2(art, art), unit * 0.5f, true);

        const float buttonSize = unit * 2.6f;
        const float textX = start.x + unit * 0.6f + art + unit * 0.8f;
        const float textWidth = width - (textX - start.x) - buttonSize * 2.0f - unit;

        ImGui::SetCursorScreenPos(ImVec2(textX, start.y + height * 0.5f - unit * 1.1f));
        text(playback_.track.name, TEXT_PRIMARY, textWidth);
        ImGui::SetCursorScreenPos(ImVec2(textX, start.y + height * 0.5f + unit * 0.1f));
        ImGui::PushFont(fonts_.small);
        text(playback_.track.artist, TEXT_SECONDARY, textWidth);
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(start.x + width - buttonSize * 2.0f - unit * 0.4f, start.y + (height - buttonSize) * 0.5f));

        if (iconButton("mini_play", playback_.playing ? Icon::Pause : Icon::Play, buttonSize, TEXT_PRIMARY, 0.55f))
        {
            const bool pause = playback_.playing;
            playback_.progressMs = currentProgress();
            playback_.playing = !pause;
            playbackAt_ = Clock::now();
            command([pause](BlitzDJ& dj) { return pause ? dj.pause() : dj.play(); });
        }

        ImGui::SameLine(0, 0);

        if (iconButton("mini_next", Icon::Next, buttonSize, TEXT_PRIMARY, 0.55f))
        {
            command([](BlitzDJ& dj) { return dj.skip(); });
        }

        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + height));
    }

    void App::drawBottomNav()
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float unit = ImGui::GetFontSize();
        const float height = unit * 4.2f;
        const float top = display.y - height;

        auto* list = ImGui::GetWindowDrawList();
        list->AddRectFilled(ImVec2(0, top), ImVec2(display.x, display.y), color(SURFACE));
        list->AddLine(ImVec2(0, top), ImVec2(display.x, top), color(OUTLINE));

        struct Item
        {
            Page page;
            Icon icon;
            Text label;
        };

        const Item items[] = {
            {Page::Player, Icon::Music, Text::Player},
            {Page::AutoDJ, Icon::Bolt, Text::AutoDJ},
            {Page::Search, Icon::Search, Text::Search},
            {Page::Speakers, Icon::Speaker, Text::Speakers}
        };

        const float itemWidth = std::min(display.x, unit * 40.0f) / 4.0f;
        const float startX = (display.x - itemWidth * 4.0f) * 0.5f;

        for (int i = 0; i < 4; ++i)
        {
            const auto& item = items[i];
            const bool selected = page_ == item.page;
            const float x = startX + itemWidth * static_cast<float>(i);

            ImGui::SetCursorScreenPos(ImVec2(x, top));
            ImGui::PushID(i);

            if (ImGui::InvisibleButton("nav", ImVec2(itemWidth, height)))
            {
                setPage(item.page);
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::PopID();

            const ImVec2 iconCenter(x + itemWidth * 0.5f, top + height * 0.36f);

            if (selected)
            {
                list->AddRectFilled(
                    ImVec2(iconCenter.x - unit * 2.0f, iconCenter.y - unit * 1.0f),
                    ImVec2(iconCenter.x + unit * 2.0f, iconCenter.y + unit * 1.0f),
                    color(ImVec4(0.24f, 0.18f, 0.0f, 1.0f)),
                    unit
                );
            }

            drawIcon(list, item.icon, iconCenter, unit * 1.4f, color(selected ? ACCENT : TEXT_SECONDARY));

            ImGui::PushFont(fonts_.small);
            const char* label = tr(item.label);
            const ImVec2 labelSize = ImGui::CalcTextSize(label);
            list->AddText(ImVec2(x + (itemWidth - labelSize.x) * 0.5f, top + height * 0.66f), color(selected ? TEXT_PRIMARY : TEXT_SECONDARY), label);
            ImGui::PopFont();
        }
    }

    void App::drawToast()
    {
        if (toast_.empty() || Clock::now() > toastUntil_)
        {
            return;
        }

        const float remaining = std::chrono::duration<float>(toastUntil_ - Clock::now()).count();
        const float alpha = std::clamp(remaining * 3.0f, 0.0f, 1.0f);
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float unit = ImGui::GetFontSize();

        auto* list = ImGui::GetForegroundDrawList();
        const float maxWidth = std::min(display.x - unit * 2.0f, unit * 28.0f);
        const std::string message = fitText(toast_, maxWidth - unit * 2.0f);
        const ImVec2 size = ImGui::CalcTextSize(message.c_str());
        const ImVec2 box(size.x + unit * 2.0f, size.y + unit * 1.4f);
        const ImVec2 position((display.x - box.x) * 0.5f, display.y - unit * 10.0f - box.y);

        list->AddRectFilled(position, ImVec2(position.x + box.x, position.y + box.y), color(ImVec4(0.2f, 0.21f, 0.26f, 1.0f), alpha), box.y * 0.5f);
        list->AddText(ImVec2(position.x + unit, position.y + unit * 0.7f), color(TEXT_PRIMARY, alpha), message.c_str());
    }

    void App::draw()
    {
        drawBackground();

        const ImVec2 display = ImGui::GetIO().DisplaySize;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(display);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("BlitzDJ", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::PopStyleVar(2);

        if (!authenticated_)
        {
            drawLogin();
            ImGui::End();
            drawToast();
            return;
        }

        const float unit = ImGui::GetFontSize();
        const float navHeight = unit * 4.2f;
        const bool showMini = page_ != Page::Player && hasPlayback_;
        const float miniHeight = showMini ? unit * 4.6f : 0.0f;
        const float margin = unit * 1.2f;
        const float pageWidth = std::min(display.x - margin * 2.0f, MAX_CONTENT_WIDTH * scale_);
        const float pageX = (display.x - pageWidth) * 0.5f;

        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pageX, unit * 0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, unit * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

        ImGui::BeginChild("page", ImVec2(display.x, display.y - navHeight - miniHeight), ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleVar(2);

        switch (page_)
        {
            case Page::Player:
                drawPlayer(pageWidth);
                break;

            case Page::AutoDJ:
                drawAutoDJ(pageWidth);
                break;

            case Page::Search:
                drawSearch(pageWidth);
                break;

            case Page::Speakers:
                drawSpeakers(pageWidth);
                break;
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (showMini)
        {
            ImGui::SetCursorScreenPos(ImVec2(pageX, display.y - navHeight - miniHeight + unit * 0.2f));
            drawMiniPlayer(pageWidth);
        }

        drawBottomNav();

        ImGui::End();
        drawToast();
    }
}
