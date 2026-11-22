#include "BlitzDJ.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace blitzdj;

namespace
{
    void clearScreen()
    {
#ifdef _WIN32
        std::system("cls");
#else
        std::cout << "\033[2J\033[H";
#endif
    }

    void printHeader()
    {
        std::cout
            << "========================================\n"
            << "              BLITZ DJ                  \n"
            << "       Intelligent Spotify DJ           \n"
            << "========================================\n\n";
    }

    void waitForEnter()
    {
        std::cout << "\nPress ENTER to continue...";
        std::string input;
        std::getline(std::cin, input);
    }

    std::string readLine(const std::string& prompt)
    {
        std::cout << prompt;

        std::string value;
        std::getline(std::cin, value);

        return value;
    }

    int readNumber(const std::string& prompt)
    {
        while (true)
        {
            const std::string input = readLine(prompt);

            try
            {
                std::size_t processed = 0;
                int value = std::stoi(input, &processed);

                if (processed == input.size())
                {
                    return value;
                }
            }
            catch (...)
            {
            }

            if (!std::cin.good())
            {
                return -1;
            }

            std::cout << "Invalid number. Try again.\n";
        }
    }

    void showTracks(
        const std::vector<SpotifyTrack>& tracks
    )
    {
        if (tracks.empty())
        {
            std::cout << "No tracks found.\n";
            return;
        }

        for (std::size_t i = 0; i < tracks.size(); ++i)
        {
            const auto& track = tracks[i];

            std::cout
                << "[" << (i + 1) << "] "
                << track.name
                << " - " << track.artist
                << "\n";
        }
    }

    void showPlayback(BlitzDJ& dj)
    {
        auto playback = dj.getPlayback();

        if (!playback.has_value())
        {
            std::cout << "No active Spotify playback.\n";
            return;
        }

        const auto& player = *playback;

        std::cout
            << "Track: " << player.track.name << "\n"
            << "Artist: " << player.track.artist << "\n"
            << "Album: " << player.track.album << "\n"
            << "Device: " << player.device.name << "\n"
            << "Status: "
            << (player.playing ? "Playing" : "Paused")
            << "\n";

        const int currentSeconds = player.progressMs / 1000;
        const int totalSeconds = player.durationMs / 1000;

        std::cout
            << "Progress: "
            << currentSeconds / 60 << ":"
            << (currentSeconds % 60 < 10 ? "0" : "")
            << currentSeconds % 60
            << " / "
            << totalSeconds / 60 << ":"
            << (totalSeconds % 60 < 10 ? "0" : "")
            << totalSeconds % 60
            << "\n";
    }

    void authenticateMenu(BlitzDJ& dj)
    {
        clearScreen();
        printHeader();

        if (dj.spotify().isAuthenticated())
        {
            std::cout << "Spotify is already connected.\n";
            waitForEnter();
            return;
        }

        const std::string url = dj.getAuthorizationUrl();

        if (url.empty())
        {
            std::cout << "Failed to generate authorization URL.\n";
            waitForEnter();
            return;
        }

        std::cout
            << "Open the following URL in your browser:\n\n"
            << url
            << "\n\n";

        std::cout
            << "After authorization, copy the values from\n"
            << "the redirect URL.\n\n";

        const std::string code =
            readLine("Authorization code: ");

        const std::string state =
            readLine("Returned state: ");

        if (dj.authenticate(code, state))
        {
            std::cout << "\nSpotify connected successfully!\n";
        }
        else
        {
            std::cout
                << "\nAuthentication failed: "
                << dj.spotify().getLastError()
                << "\n";
        }

        waitForEnter();
    }

    void searchMenu(BlitzDJ& dj)
    {
        clearScreen();
        printHeader();

        const std::string query =
            readLine("Search for a song or artist: ");

        if (query.empty())
        {
            return;
        }

        const auto tracks = dj.search(query, 20);

        showTracks(tracks);

        if (tracks.empty())
        {
            waitForEnter();
            return;
        }

        std::cout
            << "\n[0] Back\n"
            << "Select a track to play.\n\n";

        const int selection =
            readNumber("Track number: ");

        if (selection <= 0 ||
            static_cast<std::size_t>(selection) > tracks.size())
        {
            return;
        }

        const auto& track = tracks[selection - 1];

        if (dj.playTrack(track.uri))
        {
            std::cout << "Now playing: " << track.name << "\n";
        }
        else
        {
            std::cout
                << "Playback failed: "
                << dj.spotify().getLastError()
                << "\n";
        }

        waitForEnter();
    }

    void recommendationsMenu(BlitzDJ& dj)
    {
        clearScreen();
        printHeader();

        const std::string query =
            readLine("Enter a music genre or artist: ");

        if (query.empty())
        {
            return;
        }

        auto tracks = dj.recommend(query, 10);

        std::cout << "\nRecommended tracks:\n\n";
        showTracks(tracks);

        if (tracks.empty())
        {
            waitForEnter();
            return;
        }

        std::cout
            << "\n[1] Add recommendations to queue\n"
            << "[2] Play first recommendation\n"
            << "[0] Back\n\n";

        const int option = readNumber("Choose: ");

        if (option == 1)
        {
            int added = 0;

            for (const auto& track : tracks)
            {
                if (dj.queueTrack(track.uri))
                {
                    ++added;
                }
            }

            std::cout
                << added << " tracks added to the queue.\n";
        }
        else if (option == 2)
        {
            if (dj.playTrack(tracks.front().uri))
            {
                std::cout << "Playback started.\n";
            }
            else
            {
                std::cout << "Failed to start playback.\n";
            }
        }

        waitForEnter();
    }

    void devicesMenu(BlitzDJ& dj)
    {
        clearScreen();
        printHeader();

        const auto devices = dj.spotify().getDevices();

        if (devices.empty())
        {
            std::cout
                << "No Spotify devices available.\n"
                << "Open Spotify on a supported device.\n";

            waitForEnter();
            return;
        }

        std::cout << "Available devices:\n\n";

        for (std::size_t i = 0; i < devices.size(); ++i)
        {
            std::cout
                << "[" << (i + 1) << "] "
                << devices[i].name
                << " (" << devices[i].type << ")"
                << (devices[i].active ? " [ACTIVE]" : "")
                << "\n";
        }

        std::cout << "\n[0] Back\n";

        const int selection =
            readNumber("Select device: ");

        if (selection <= 0 ||
            static_cast<std::size_t>(selection) > devices.size())
        {
            return;
        }

        const auto& device = devices[selection - 1];

        if (device.id.empty() || device.restricted)
        {
            std::cout << "This device cannot be selected.\n";
        }
        else if (dj.spotify().transferPlayback(device.id))
        {
            dj.spotify().setDevice(device.id);
            std::cout << "Playback transferred successfully.\n";
        }
        else
        {
            std::cout << "Failed to transfer playback.\n";
        }

        waitForEnter();
    }

    void settingsMenu(BlitzDJ& dj)
    {
        while (true)
        {
            clearScreen();
            printHeader();

            std::cout
                << "============= SETTINGS =============\n\n"
                << "[1] Toggle AutoDJ\n"
                << "[2] Change AutoDJ search query\n"
                << "[3] Add favorite artist\n"
                << "[4] Add favorite track\n"
                << "[5] Clear playback history\n"
                << "[6] Change volume\n"
                << "[7] Toggle shuffle\n"
                << "[0] Back\n\n";

            std::cout
                << "AutoDJ: "
                << (dj.isAutoDJEnabled() ? "ON" : "OFF")
                << "\n";

            const int option = readNumber("\nChoose: ");

            if (option == 0)
            {
                return;
            }

            switch (option)
            {
                case 1:
                {
                    dj.setAutoDJ(!dj.isAutoDJEnabled());

                    std::cout << "AutoDJ updated.\n";
                    break;
                }

                case 2:
                {
                    const std::string query =
                        readLine("New search query: ");

                    dj.setSearchQuery(query);
                    break;
                }

                case 3:
                {
                    const std::string artist =
                        readLine("Favorite artist: ");

                    dj.ai().addFavoriteArtist(artist);
                    break;
                }

                case 4:
                {
                    const std::string id =
                        readLine("Spotify track ID: ");

                    dj.ai().addFavoriteTrack(id);
                    break;
                }

                case 5:
                {
                    dj.ai().clearHistory();
                    std::cout << "History cleared.\n";
                    break;
                }

                case 6:
                {
                    const int volume =
                        readNumber("Volume (0-100): ");

                    if (volume >= 0 && volume <= 100)
                    {
                        if (!dj.spotify().setVolume(volume))
                        {
                            std::cout << "Volume change failed.\n";
                        }
                    }
                    break;
                }

                case 7:
                {
                    auto playback = dj.getPlayback();

                    if (playback.has_value())
                    {
                        dj.spotify().setShuffle(
                            !playback->shuffle
                        );
                    }
                    break;
                }

                default:
                    std::cout << "Unknown option.\n";
                    break;
            }

            waitForEnter();
        }
    }

    void playerMenu(BlitzDJ& dj)
    {
        while (true)
        {
            clearScreen();
            printHeader();

            std::cout << "============== PLAYER ==============\n\n";

            showPlayback(dj);

            std::cout
                << "\n[1] Play / Resume\n"
                << "[2] Pause\n"
                << "[3] Next track\n"
                << "[4] Previous track\n"
                << "[5] Add recommended tracks\n"
                << "[0] Back\n\n";

            const int option = readNumber("Choose: ");

            if (option == 0)
            {
                return;
            }

            bool success = false;

            switch (option)
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
                    success = dj.queueRecommendedTracks(3);
                    break;

                default:
                    std::cout << "Unknown option.\n";
                    waitForEnter();
                    continue;
            }

            std::cout
                << (success ? "Command completed.\n"
                            : "Command failed.\n");

            waitForEnter();
        }
    }

    void printMainMenu(BlitzDJ& dj)
    {
        clearScreen();
        printHeader();

        std::cout
            << "Spotify: "
            << (dj.spotify().isAuthenticated()
                ? "CONNECTED" : "DISCONNECTED")
            << "\n";

        std::cout
            << "AutoDJ: "
            << (dj.isAutoDJEnabled() ? "ENABLED" : "DISABLED")
            << "\n\n";

        std::cout
            << "============== MAIN MENU ==============\n\n"
            << "[1] Connect Spotify\n"
            << "[2] Music Player\n"
            << "[3] Search Music\n"
            << "[4] AI Recommendations\n"
            << "[5] Spotify Devices\n"
            << "[6] Settings\n"
            << "[7] Start AutoDJ\n"
            << "[8] Stop AutoDJ\n"
            << "[0] Exit\n\n";
    }
}

int main()
{
    std::string clientId;

    if (const char* env = std::getenv("SPOTIFY_CLIENT_ID"))
    {
        clientId = env;
    }

    clearScreen();
    printHeader();

    if (clientId.empty())
    {
        clientId = readLine("Enter Spotify Client ID: ");
    }

    if (clientId.empty())
    {
        std::cerr << "A Spotify Client ID is required.\n";
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

    bool running = true;

    while (running)
    {
        printMainMenu(dj);

        const int option = readNumber("Choose an option: ");

        switch (option)
        {
            case 1:
                authenticateMenu(dj);
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
                devicesMenu(dj);
                break;

            case 6:
                settingsMenu(dj);
                break;

            case 7:
            {
                if (dj.start())
                {
                    std::cout << "AutoDJ started successfully.\n";
                }
                else
                {
                    std::cout
                        << "Unable to start AutoDJ.\n"
                        << "Connect your Spotify account first.\n";
                }

                waitForEnter();
                break;
            }

            case 8:
                dj.stop();
                std::cout << "AutoDJ stopped.\n";
                waitForEnter();
                break;

            case 0:
                running = false;
                break;

            default:
                std::cout << "Invalid option.\n";
                waitForEnter();
                break;
        }
    }

    dj.stop();

    std::cout << "\nThank you for using BlitzDJ!\n";

    return 0;
}
