#pragma once

#include <filesystem>
#include <string>

namespace blitzdj::app
{
    std::filesystem::path configDirectory();

    void openUrl(const std::string& url);

    void openBluetoothSettings();

    void openSoundSettings();
}
