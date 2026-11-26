#pragma once

#include <imgui.h>

namespace blitzdj::app
{
    enum class Icon
    {
        Play,
        Pause,
        Next,
        Previous,
        Shuffle,
        Repeat,
        RepeatOne,
        Volume,
        Search,
        Bolt,
        Music,
        Speaker,
        Bluetooth,
        Heart,
        QueueAdd,
        Refresh,
        Logout,
        Computer,
        Phone
    };

    void drawIcon(ImDrawList* drawList, Icon icon, ImVec2 center, float size, ImU32 color);
}
