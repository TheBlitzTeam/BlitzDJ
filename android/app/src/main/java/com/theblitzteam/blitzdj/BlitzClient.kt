package com.theblitzteam.blitzdj

import android.content.Context
import java.io.File
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

object BlitzClient {
    const val REDIRECT_URI = "blitzdj://callback"

    private const val PREFERENCES = "blitzdj"
    private const val KEY_CLIENT_ID = "client_id"
    private const val KEY_QUERY = "auto_dj_query"
    private const val KEY_PARTY = "party_mode"
    private const val KEY_PROXIMITY = "proximity_connect"

    val executor: ExecutorService = Executors.newSingleThreadExecutor { runnable ->
        Thread(runnable, "BlitzDJ-Worker").apply { isDaemon = true }
    }

    @Volatile
    var autoLoginAttempted = false

    private var handle = 0L
    private var activeClientId = ""

    fun savedClientId(context: Context): String {
        val stored = preferences(context).getString(KEY_CLIENT_ID, null)
        return if (stored.isNullOrBlank()) BuildConfig.SPOTIFY_CLIENT_ID else stored
    }

    fun saveClientId(context: Context, clientId: String) {
        preferences(context).edit().putString(KEY_CLIENT_ID, clientId.trim()).apply()
    }

    fun savedQuery(context: Context): String =
        preferences(context).getString(KEY_QUERY, null) ?: "party hits"

    fun saveQuery(context: Context, query: String) {
        preferences(context).edit().putString(KEY_QUERY, query.trim()).apply()
    }

    fun partyMode(context: Context): Boolean =
        preferences(context).getBoolean(KEY_PARTY, false)

    fun savePartyMode(context: Context, enabled: Boolean) {
        preferences(context).edit().putBoolean(KEY_PARTY, enabled).apply()
    }

    fun proximityConnect(context: Context): Boolean =
        preferences(context).getBoolean(KEY_PROXIMITY, false)

    fun saveProximityConnect(context: Context, enabled: Boolean) {
        preferences(context).edit().putBoolean(KEY_PROXIMITY, enabled).apply()
    }

    @Synchronized
    fun handle(context: Context): Long {
        val clientId = savedClientId(context)

        if (clientId.isBlank()) {
            return 0L
        }

        if (handle != 0L && clientId == activeClientId) {
            return handle
        }

        if (handle != 0L) {
            NativeBridge.destroy(handle)
        }

        val session = File(context.applicationContext.filesDir, "session.json").absolutePath
        handle = NativeBridge.create(clientId, REDIRECT_URI, session)
        activeClientId = clientId

        if (handle != 0L) {
            NativeBridge.setSearchQuery(handle, savedQuery(context))
        }

        return handle
    }

    @Synchronized
    fun currentHandle(): Long = handle

    private fun preferences(context: Context) =
        context.applicationContext.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
}
