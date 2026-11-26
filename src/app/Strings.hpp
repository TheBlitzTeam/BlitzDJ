#pragma once

namespace blitzdj::app
{
    enum class Text
    {
        Tagline,
        ConnectSpotify,
        WaitingBrowser,
        Cancel,
        LoginFailed,
        Player,
        AutoDJ,
        Search,
        Speakers,
        NowPlaying,
        PlayingOn,
        NothingPlaying,
        OpenSpotifyHint,
        UpNext,
        AutoDJOff,
        AutoDJOn,
        AutoDJEnable,
        AutoDJDescription,
        AutoDJQueryHint,
        QueueFive,
        Suggestions,
        SuggestionsEmpty,
        Refresh,
        SearchHint,
        SearchEmpty,
        NoResults,
        Queued,
        Done,
        FavoriteAdded,
        CurrentOutput,
        PartyMode,
        PartyModeDescription,
        PartyResumed,
        BluetoothSpeakers,
        BluetoothEmpty,
        BluetoothUnsupported,
        BluetoothOpenSettings,
        Connected,
        Paired,
        Connect,
        Disconnect,
        SpotifyConnect,
        NoDevices,
        PlayHere,
        Active,
        OpenSpotify,
        Logout,
        Volume,
        Scan,
        SpeakerReconnected,
        Count
    };

    void initializeLanguage();

    const char* tr(Text text);
}
