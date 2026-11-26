package com.theblitzteam.blitzdj

object NativeBridge {
    init {
        System.loadLibrary("blitzdj")
    }

    external fun create(clientId: String, redirectUri: String, sessionPath: String): Long
    external fun destroy(handle: Long)
    external fun version(): String

    external fun authorizationUrl(handle: Long): String
    external fun handleRedirect(handle: Long, url: String): Boolean
    external fun isAuthenticated(handle: Long): Boolean
    external fun logout(handle: Long)

    external fun play(handle: Long): Boolean
    external fun pause(handle: Long): Boolean
    external fun next(handle: Long): Boolean
    external fun previous(handle: Long): Boolean
    external fun setVolume(handle: Long, volume: Int): Boolean
    external fun seek(handle: Long, positionMs: Int): Boolean
    external fun setShuffle(handle: Long, enabled: Boolean): Boolean
    external fun setRepeat(handle: Long, mode: String): Boolean

    external fun playbackJson(handle: Long): String
    external fun devicesJson(handle: Long): String
    external fun queueJson(handle: Long, limit: Int): String
    external fun searchJson(handle: Long, query: String, limit: Int): String
    external fun recommendJson(handle: Long, query: String, count: Int): String

    external fun playTrack(handle: Long, uri: String): Boolean
    external fun queueTrack(handle: Long, uri: String): Boolean
    external fun transferPlayback(handle: Long, deviceId: String): Boolean

    external fun startAutoDJ(handle: Long): Boolean
    external fun stopAutoDJ(handle: Long)
    external fun isAutoDJRunning(handle: Long): Boolean
    external fun setSearchQuery(handle: Long, query: String)
    external fun searchQuery(handle: Long): String
    external fun queueRecommendations(handle: Long, count: Int): Boolean
    external fun addFavoriteArtist(handle: Long, artist: String)
    external fun addFavoriteTrack(handle: Long, trackId: String)

    external fun lastError(handle: Long): String
}
