#include "BlitzDJ.hpp"

#if BLITZDJ_HAS_BLUETOOTH
#include "Bluetooth.hpp"
#endif

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

using namespace blitzdj;

namespace
{
    constexpr const char* VERSION = "0.2.0";

    void clearScreen()
    {
        std::cout << "\033[2J\033[H" << std::flush;
    }

    void header(const std::string& title = "MAIN MENU")
    {
        clearScreen();

        std::cout
            << "============================================\n"
            << "                 BLITZ DJ                   \n"
            << "          Intelligent Music Player          \n"
            << "============================================\n"
            << " Version: " << VERSION << "\n"
            << " Section: " << title << "\n"
            << "--------------------------------------------\n\n";
    }

    std::string input(const std::string& prompt)
    {
        std::cout << prompt;

        std::string value;
        std::getline(std::cin, value);

        return value;
    }

    int number(const std::string& prompt)
    {
        const auto value = input(prompt);

        try
        {
            std::size_t pos = 0;
            int result = std::stoi(value, &pos);

            if (pos == value.size())
                return result;
        }
        catch (...)
        {
        }

        return -1;
    }

    void wait()
    {
        input("\nPress ENTER to continue...");
    }

    void result(bool success, BlitzDJ& dj)
    {
        if (success)
        {
            std::cout << "\nOperation completed.\n";
        }
        else
        {
            std::cout << "\nOperation failed.\n";

            const auto& error = dj.spotify().getLastError();

            if (!error.empty())
                std::cout << "Error: " << error << "\n";
        }
    }

    void printTracks(const std::vector<SpotifyTrack>& tracks)
    {
        if (tracks.empty())
        {
            std::cout << "No tracks available.\n";
            return;
        }

        for (std::size_t i = 0; i < tracks.size(); ++i)
        {
            const auto& track = tracks[i];

            std::cout
                << "[" << i + 1 << "] "
                << track.name
                << " - " << track.artist
                << "\n";
        }
    }

    void spotifyLogin(BlitzDJ& dj)
    {
        header("SPOTIFY AUTHENTICATION");

        if (dj.spotify().isAuthenticated())
        {
            std::cout << "Spotify is already connected.\n";
            wait();
            return;
        }

        const auto url = dj.getAuthorizationUrl();

        if (url.empty())
        {
            std::cout << "Unable to generate login URL.\n";
            wait();
            return;
        }

        std::cout
            << "Open this URL in your browser:\n\n"
            << url << "\n\n"
            << "After authorization, copy the code and state\n"
            << "from the redirected URL.\n\n";

        const auto code = input("Authorization code: ");
        const auto state = input("OAuth state: ");

        result(dj.authenticate(code, state), dj);
        wait();
    }

    void playerMenu(BlitzDJ& dj)
    {
        while (true)
        {
            header("MUSIC PLAYER");

            auto playback = dj.getPlayback();

            if (playback)
            {
                const auto& p = *playback;

                std::cout
                    << "Track: " << p.track.name << "\n"
                    << "Artist: " << p.track.artist << "\n"
                    << "Album: " << p.track.album << "\n"
                    << "Device: " << p.device.name << "\n"
                    << "Status: "
                    << (p.playing ? "Playing" : "Paused")
                    << "\n"
                    << "Progress: "
                    << p.progressMs / 1000
                    << " / "
                    << p.durationMs / 1000
                    << " seconds\n";
            }
            else
            {
                std::cout << "No active playback.\n";
            }

            std::cout
                << "\n[1] Play / Resume"
                << "\n[2] Pause"
                << "\n[3] Next Track"
                << "\n[4] Previous Track"
                << "\n[5] Set Volume"
                << "\n[6] Seek"
                << "\n[7] Toggle Shuffle"
                << "\n[8] Set Repeat"
                << "\n[0] Back\n\n";

            int choice = number("Select: ");

            if (choice == 0)
                return;

            bool success = false;

            switch (choice)
            {
                case 1:
                    success = dj.resume();
                    break;

                case 2:
                    success = dj.pause();
                    break;

                case 3:
                    success = dj.skip();
                    break;

                case 4:
                    success = dj.spotify().previousTrack();
                    break;

                case 5:
                {
                    int volume = number("Volume (0-100): ");

                    if (volume >= 0 && volume <= 100)
                        success = dj.spotify().setVolume(volume);

                    break;
                }

                case 6:
                {
                    int seconds = number("Position in seconds: ");

                    if (seconds >= 0 &&
                        seconds <= std::numeric_limits<int>::max() / 1000)
                    {
                        success = dj.spotify().seek(seconds * 1000);
                    }

                    break;
                }

                case 7:
                    if (playback)
                        success = dj.spotify().setShuffle(
                            !playback->shuffle
                        );
                    break;

                case 8:
                {
                    std::cout << "Modes: off, track, context\n";

                    auto mode = input("Repeat mode: ");

                    if (mode == "off" ||
                        mode == "track" ||
                        mode == "context")
                    {
                        success = dj.spotify().setRepeat(mode);
                    }

                    break;
                }

                default:
                    std::cout << "Invalid option.\n";
                    wait();
                    continue;
            }

            result(success, dj);
            wait();
        }
    }

    void searchMenu(BlitzDJ& dj)
    {
        header("MUSIC SEARCH");

        auto query = input("Search: ");

        if (query.empty())
            return;

        auto tracks = dj.search(query, 20);

        printTracks(tracks);

        if (tracks.empty())
        {
            wait();
            return;
        }

        std::cout
            << "\n[0] Back\n"
            << "Select a track number.\n";

        int selection = number("Track: ");

        if (selection <= 0 ||
            static_cast<std::size_t>(selection) > tracks.size())
            return;

        const auto& track = tracks[selection - 1];

        std::cout
            << "\n[1] Play"
            << "\n[2] Add to Queue"
            << "\n[3] Add to Favorites"
            << "\n[0] Cancel\n";

        int action = number("Action: ");

        switch (action)
        {
            case 1:
                result(dj.playTrack(track.uri), dj);
                break;

            case 2:
                result(dj.queueTrack(track.uri), dj);
                break;

            case 3:
                dj.ai().addFavoriteTrack(track.id);
                std::cout << "Track added to favorites.\n";
                break;

            default:
                return;
        }

        wait();
    }

    void recommendationsMenu(BlitzDJ& dj)
    {
        header("BLITZ AI RECOMMENDATIONS");

        auto query = input("Genre or artist: ");

        if (query.empty())
            return;

        auto tracks = dj.recommend(query, 10);

        printTracks(tracks);

        if (tracks.empty())
        {
            wait();
            return;
        }

        std::cout
            << "\n[1] Queue All"
            << "\n[2] Play First"
            << "\n[0] Back\n";

        int choice = number("Select: ");

        if (choice == 1)
        {
            int added = 0;

            for (const auto& track : tracks)
            {
                if (dj.queueTrack(track.uri))
                    ++added;
            }

            std::cout << added << " tracks queued.\n";
        }
        else if (choice == 2)
        {
            result(dj.playTrack(tracks.front().uri), dj);
        }

        wait();
    }

    void spotifyDevicesMenu(BlitzDJ& dj)
    {
        header("SPOTIFY DEVICES");

        auto devices = dj.spotify().getDevices();

        if (devices.empty())
        {
            std::cout << "No Spotify devices available.\n";
            wait();
            return;
        }

        for (std::size_t i = 0; i < devices.size(); ++i)
        {
            const auto& device = devices[i];

            std::cout
                << "[" << i + 1 << "] "
                << device.name
                << " (" << device.type << ")"
                << (device.active ? " [ACTIVE]" : "")
                << "\n";
        }

        int choice = number("\nSelect device (0 to cancel): ");

        if (choice <= 0 ||
            static_cast<std::size_t>(choice) > devices.size())
            return;

        const auto& device = devices[choice - 1];

        if (device.id.empty() || device.restricted)
        {
            std::cout << "Device is unavailable.\n";
        }
        else
        {
            bool success = dj.spotify().transferPlayback(device.id);

            if (success)
                dj.spotify().setDevice(device.id);

            result(success, dj);
        }

        wait();
    }

#if BLITZDJ_HAS_BLUETOOTH

    void bluetoothMenu(Bluetooth& bluetooth)
    {
        while (true)
        {
            header("BLUETOOTH MANAGER");

            std::cout
                << "Status: "
                << (bluetooth.isInitialized()
                    ? "Available" : "Unavailable")
                << "\n\n"
                << "[1] Scan Devices\n"
                << "[2] Paired Devices\n"
                << "[3] Connected Devices\n"
                << "[4] Connect Device\n"
                << "[5] Disconnect Device\n"
                << "[6] Pair Device\n"
                << "[7] Unpair Device\n"
                << "[8] Bluetooth Adapters\n"
                << "[0] Back\n\n";

            int choice = number("Select: ");

            if (choice == 0)
                return;

            if (!bluetooth.isInitialized())
            {
                std::cout << "Bluetooth is not initialized.\n";
                wait();
                continue;
            }

            if (choice == 1)
            {
                if (bluetooth.startScan())
                {
                    std::cout << "Scanning for devices...\n";

                    std::this_thread::sleep_for(
                        std::chrono::seconds(5)
                    );

                    bluetooth.stopScan();

                    auto devices = bluetooth.getDevices();

                    for (const auto& device : devices)
                    {
                        std::cout
                            << device.name
                            << " | " << device.address
                            << (device.connected ? " [CONNECTED]" : "")
                            << "\n";
                    }

                    if (devices.empty())
                        std::cout << "No devices found.\n";
                }
                else
                {
                    std::cout
                        << bluetooth.getLastErrorMessage()
                        << "\n";
                }
            }
            else if (choice == 2 || choice == 3)
            {
                auto devices = choice == 2
                    ? bluetooth.getPairedDevices()
                    : bluetooth.getConnectedDevices();

                for (const auto& device : devices)
                {
                    std::cout
                        << device.name
                        << " | " << device.address
                        << " | ID: " << device.id
                        << "\n";
                }

                if (devices.empty())
                    std::cout << "No devices found.\n";
            }
            else if (choice >= 4 && choice <= 7)
            {
                auto id = input("Device ID or address: ");

                if (id.empty())
                    continue;

                auto device = bluetooth.findDevice(id);

                if (!device)
                    device = bluetooth.findDeviceByAddress(id);

                if (!device)
                {
                    std::cout << "Device not found.\n";
                    wait();
                    continue;
                }

                bool success = false;

                switch (choice)
                {
                    case 4:
                        success = bluetooth.connectAudioDevice(
                            device->id
                        );
                        break;

                    case 5:
                        success = bluetooth.disconnectDevice(
                            device->id
                        );
                        break;

                    case 6:
                        success = bluetooth.pairDevice(
                            device->id
                        );
                        break;

                    case 7:
                        success = bluetooth.unpairDevice(
                            device->id
                        );
                        break;
                }

                std::cout << (
                    success
                        ? "Bluetooth operation completed.\n"
                        : bluetooth.getLastErrorMessage() + "\n"
                );
            }
            else if (choice == 8)
            {
                auto adapters = bluetooth.getAdapters();

                for (const auto& adapter : adapters)
                {
                    std::cout
                        << adapter.name
                        << " | " << adapter.address
                        << " | Powered: "
                        << (adapter.powered ? "Yes" : "No")
                        << "\n";
                }

                if (adapters.empty())
                    std::cout << "No adapters found.\n";
            }

            wait();
        }
    }

#endif

    void autoDJMenu(BlitzDJ& dj)
    {
        while (true)
        {
            header("AUTO DJ");

            std::cout
                << "AutoDJ: "
                << (dj.isAutoDJEnabled() ? "ON" : "OFF")
                << "\n"
                << "Worker: "
                << (dj.isRunning() ? "RUNNING" : "STOPPED")
                << "\n\n"
                << "[1] Start AutoDJ\n"
                << "[2] Stop AutoDJ\n"
                << "[3] Enable / Disable AutoDJ\n"
                << "[4] Change Music Search\n"
                << "[5] Queue Recommendations\n"
                << "[6] Add Favorite Artist\n"
                << "[7] Clear AI History\n"
                << "[0] Back\n\n";

            int choice = number("Select: ");

            if (choice == 0)
                return;

            switch (choice)
            {
                case 1:
                    dj.setAutoDJ(true);
                    result(dj.start(), dj);
                    break;

                case 2:
                    dj.stop();
                    std::cout << "AutoDJ stopped.\n";
                    break;

                case 3:
                    dj.setAutoDJ(!dj.isAutoDJEnabled());
                    std::cout << "AutoDJ setting updated.\n";
                    break;

                case 4:
                {
                    auto query = input("New search query: ");

                    if (!query.empty())
                    {
                        dj.stop();
                        dj.setSearchQuery(query);
                        std::cout
                            << "Search updated. Restart AutoDJ "
                            << "to continue.\n";
                    }

                    break;
                }

                case 5:
                {
                    int count = number("Number of tracks (1-20): ");

                    if (count >= 1 && count <= 20)
                        result(
                            dj.queueRecommendedTracks(
                                static_cast<std::size_t>(count)
                            ),
                            dj
                        );

                    break;
                }

                case 6:
                {
                    dj.stop();

                    auto artist = input("Favorite artist: ");

                    if (!artist.empty())
                        dj.ai().addFavoriteArtist(artist);

                    break;
                }

                case 7:
                    dj.stop();
                    dj.ai().clearHistory();
                    std::cout << "AI history cleared.\n";
                    break;

                default:
                    std::cout << "Invalid option.\n";
                    break;
            }

            wait();
        }
    }

    void showMainMenu(BlitzDJ& dj)
    {
        header();

        std::cout
            << "Spotify: "
            << (dj.spotify().isAuthenticated()
                ? "CONNECTED" : "DISCONNECTED")
            << "\n"
            << "AutoDJ: "
            << (dj.isRunning()
                ? "RUNNING" : "STOPPED")
            << "\n"
            << "Bluetooth: "
#if BLITZDJ_HAS_BLUETOOTH
            << "SUPPORTED\n"
#else
            << "UNAVAILABLE\n"
#endif
            << "\n"
            << "[1] Connect Spotify\n"
            << "[2] Music Player\n"
            << "[3] Search Music\n"
            << "[4] AI Recommendations\n"
            << "[5] Spotify Devices\n"
            << "[6] Bluetooth Manager\n"
            << "[7] AutoDJ Manager\n"
            << "[8] About BlitzDJ\n"
            << "[0] Exit\n\n";
    }

    void aboutMenu()
    {
        header("ABOUT");

        std::cout
            << "BlitzDJ " << VERSION << "\n"
            << "Developed by TheBlitzTeam\n\n"
            << "A cross-platform intelligent music\n"
            << "controller powered by C++17.\n\n"
            << "Features:\n"
            << " - Spotify Web API\n"
            << " - Automatic music selection\n"
            << " - Music recommendations\n"
            << " - Spotify Connect controls\n"
            << " - Bluetooth device management\n\n"
            << "https://github.com/TheBlitzTeam/BlitzDJ\n";

        wait();
    }
}

int main()
{
    std::string clientId;

    if (const char* value = std::getenv("SPOTIFY_CLIENT_ID"))
        clientId = value;

    if (clientId.empty())
    {
        header("INITIAL SETUP");
        clientId = input("Spotify Client ID: ");
    }

    if (clientId.empty())
    {
        std::cerr << "Spotify Client ID is required.\n";
        return 1;
    }

    BlitzDJConfig config;

    config.clientId = clientId;
    config.autoDJ = true;
    config.autoPlay = false;
    config.searchQuery = "electronic music";
    config.updateIntervalMs = 5000;
    config.queueThresholdMs = 30000;

    BlitzDJ dj(config);

    if (!dj.initialize())
    {
        std::cerr << "Failed to initialize BlitzDJ.\n";
        return 1;
    }

#if BLITZDJ_HAS_BLUETOOTH
    Bluetooth bluetooth;

    if (!bluetooth.initialize())
    {
        std::cerr
            << "Bluetooth initialization failed: "
            << bluetooth.getLastErrorMessage()
            << "\n";
    }
#endif

    bool running = true;

    while (running && std::cin.good())
    {
        showMainMenu(dj);

        int choice = number("Select: ");

        switch (choice)
        {
            case 1:
                spotifyLogin(dj);
                break;

            case 2:
                playerMenu(dj);
                break;

            case 3:
                searchMenu(dj);
                break;

            case 4:
                recommendationsMenu(dj);
                break;

            case 5:
                spotifyDevicesMenu(dj);
                break;

            case 6:
#if BLITZDJ_HAS_BLUETOOTH
                bluetoothMenu(bluetooth);
#else
                std::cout
                    << "Bluetooth is unavailable on this build.\n";
                wait();
#endif
                break;

            case 7:
                autoDJMenu(dj);
                break;

            case 8:
                aboutMenu();
                break;

            case 0:
                running = false;
                break;

            default:
                std::cout << "Invalid option.\n";
                wait();
                break;
        }
    }

    dj.stop();

#if BLITZDJ_HAS_BLUETOOTH
    bluetooth.shutdown();
#endif

    std::cout << "\nBlitzDJ closed successfully.\n";

    return 0;
}
