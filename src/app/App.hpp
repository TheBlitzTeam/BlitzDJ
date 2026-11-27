#pragma once

#include "BlitzDJ.hpp"
#include "Bluetooth.hpp"
#include "ImageCache.hpp"
#include "TaskQueue.hpp"

#include <imgui.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace blitzdj::app
{
    struct Fonts
    {
        ImFont* regular = nullptr;
        ImFont* small = nullptr;
        ImFont* title = nullptr;
        ImFont* headline = nullptr;
        ImFont* display = nullptr;
    };

    class App
    {
    public:
        App();
        ~App();

        App(const App&) = delete;
        App& operator=(const App&) = delete;

        bool initialize();

        void setFonts(const Fonts& fonts, float scale);

        void update();

        void draw();

        bool wantsFastRefresh() const;

    private:
        using Clock = std::chrono::steady_clock;

        enum class Page
        {
            Player,
            AutoDJ,
            Search,
            Speakers
        };

        Fonts fonts_;
        float scale_ = 1.0f;

        std::filesystem::path settingsPath_;
        Json settings_ = Json::object();

        std::unique_ptr<BlitzDJ> dj_;
        std::unique_ptr<Bluetooth> bluetooth_;
        bool bluetoothReady_ = false;

        MainThread main_;

        Page page_ = Page::Player;

        bool authenticated_ = false;
        bool loggingIn_ = false;
        std::string loginError_;
        std::shared_ptr<std::atomic<bool>> loginCancel_;

        bool hasPlayback_ = false;
        SpotifyPlayback playback_;
        Clock::time_point playbackAt_ = Clock::now();
        Clock::time_point lastPoll_ = Clock::time_point{};
        bool pollInFlight_ = false;
        std::string lastTrackId_;

        bool autoDJ_ = false;
        bool partyMode_ = false;

        float seekFraction_ = 0.0f;
        bool seeking_ = false;
        float volumeFraction_ = 0.5f;
        bool changingVolume_ = false;

        std::vector<SpotifyTrack> queue_;
        std::vector<SpotifyTrack> suggestions_;
        std::vector<SpotifyTrack> results_;
        std::vector<SpotifyDevice> devices_;
        std::vector<BluetoothDevice> speakers_;
        std::vector<BluetoothDevice> nearby_;

        bool loadingSuggestions_ = false;
        bool searching_ = false;
        bool searched_ = false;
        bool loadingSpeakers_ = false;
        bool scanning_ = false;
        bool scanned_ = false;
        bool loadingNearby_ = false;
        bool proximityConnect_ = false;
        bool proximityBusy_ = false;
        Clock::time_point scanUntil_ = Clock::time_point{};
        Clock::time_point lastNearbyRefresh_ = Clock::time_point{};
        Clock::time_point lastProximityCheck_ = Clock::time_point{};
        std::shared_ptr<std::atomic<bool>> stopping_ = std::make_shared<std::atomic<bool>>(false);

        char searchBuffer_[256] = {};
        char queryBuffer_[128] = {};

        std::string toast_;
        Clock::time_point toastUntil_ = Clock::time_point{};

        std::vector<std::string> connectedSpeakers_;
        std::string lostSpeaker_;
        bool resumeAfterReconnect_ = false;
        Clock::time_point lastSpeakerCheck_ = Clock::time_point{};
        Clock::time_point lastReconnectAttempt_ = Clock::time_point{};

        ImVec4 artColor_ = ImVec4(0.1f, 0.1f, 0.12f, 1.0f);
        ImVec4 targetArtColor_ = ImVec4(0.1f, 0.1f, 0.12f, 1.0f);

        ImageCache images_{imageLoader_};
        TaskQueue imageLoader_{3};
        TaskQueue bluetoothQueue_{1};
        TaskQueue network_{1};

        void loadSettings();
        void saveSettings();

        void startLogin();
        void cancelLogin();
        void logout();

        void refreshPlayback();
        void loadQueue();
        void loadSuggestions();
        void search();
        void loadDevices();
        void loadSpeakers();
        void checkSpeakers();
        void startScan();
        void updateScan();
        void loadNearby();
        void checkProximity();
        void setProximityConnect(bool enabled);

        void setAutoDJ(bool enabled);
        void setPartyMode(bool enabled);
        void applyQuery(const std::string& query);

        void command(std::function<bool(BlitzDJ&)> action, const char* success = nullptr, bool refresh = true);

        void showToast(const std::string& message);

        int currentProgress() const;

        void drawBackground();
        void drawLogin();
        void drawPlayer(float width);
        void drawAutoDJ(float width);
        void drawSearch(float width);
        void drawSpeakers(float width);
        void drawMiniPlayer(float width);
        void drawBottomNav();
        void drawToast();
        void drawTrackList(const std::vector<SpotifyTrack>& tracks, const char* id, bool queueButton, bool playButton);
        void drawArt(const std::string& url, ImVec2 size, float rounding, bool thumbnail);

        void setPage(Page page);
    };
}
