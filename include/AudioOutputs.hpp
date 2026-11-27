#pragma once

#include <string>
#include <vector>

namespace blitzdj
{
    struct AudioOutputDevice
    {
        std::string id;
        std::string name;

        bool bluetooth = false;
        bool isDefault = false;
    };

    bool audioOutputsSupported();

    std::vector<AudioOutputDevice> getAudioOutputDevices();
}
