#include "Platform.hpp"

#include <cstdlib>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace blitzdj::app
{
    namespace
    {
        void runDetached(const std::string& command)
        {
#ifndef _WIN32
            [[maybe_unused]] const int status = std::system((command + " >/dev/null 2>&1 &").c_str());
#else
            (void)command;
#endif
        }
    }

    std::filesystem::path configDirectory()
    {
#ifdef _WIN32
        if (const char* appData = std::getenv("APPDATA"))
        {
            return std::filesystem::path(appData) / "BlitzDJ";
        }
#elif defined(__APPLE__)
        if (const char* home = std::getenv("HOME"))
        {
            return std::filesystem::path(home) / "Library" / "Application Support" / "BlitzDJ";
        }
#else
        if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
        {
            return std::filesystem::path(xdg) / "BlitzDJ";
        }

        if (const char* home = std::getenv("HOME"))
        {
            return std::filesystem::path(home) / ".config" / "BlitzDJ";
        }
#endif
        return std::filesystem::current_path() / "config";
    }

    void openUrl(const std::string& url)
    {
#ifdef _WIN32
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
        runDetached("open '" + url + "'");
#else
        runDetached("xdg-open '" + url + "'");
#endif
    }

    void openBluetoothSettings()
    {
#ifdef _WIN32
        openUrl("ms-settings:bluetooth");
#elif defined(__APPLE__)
        openUrl("x-apple.systempreferences:com.apple.BluetoothSettings");
#else
        runDetached("gnome-control-center bluetooth || blueman-manager || kcmshell6 bluetooth || kcmshell5 bluetooth");
#endif
    }

    void openSoundSettings()
    {
#ifdef _WIN32
        openUrl("ms-settings:sound");
#elif defined(__APPLE__)
        openUrl("x-apple.systempreferences:com.apple.Sound-Settings.extension");
#else
        runDetached("gnome-control-center sound || pavucontrol || kcmshell6 kcm_pulseaudio || kcmshell5 kcm_pulseaudio");
#endif
    }
}
