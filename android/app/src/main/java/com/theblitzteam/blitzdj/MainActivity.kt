package com.theblitzteam.blitzdj

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.SeekBar
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import org.json.JSONArray
import org.json.JSONObject

class MainActivity : Activity() {
    private val mainHandler = Handler(Looper.getMainLooper())

    private lateinit var statusText: TextView
    private lateinit var loginPanel: View
    private lateinit var clientIdInput: EditText
    private lateinit var connectButton: Button
    private lateinit var playerPanel: View
    private lateinit var trackName: TextView
    private lateinit var trackArtist: TextView
    private lateinit var trackProgress: ProgressBar
    private lateinit var deviceText: TextView
    private lateinit var previousButton: Button
    private lateinit var playPauseButton: Button
    private lateinit var nextButton: Button
    private lateinit var volumeBar: SeekBar
    private lateinit var openSpotifyButton: Button
    private lateinit var devicesButton: Button
    private lateinit var autoDjPanel: View
    private lateinit var autoDjSwitch: Switch
    private lateinit var autoDjQuery: EditText
    private lateinit var queueButton: Button
    private lateinit var searchPanel: View
    private lateinit var searchInput: EditText
    private lateinit var searchButton: Button
    private lateinit var resultsContainer: LinearLayout

    private var authenticated = false
    private var playing = false
    private var polling = false
    private var pollInFlight = false
    private var updatingControls = false
    private var draggingVolume = false

    private val pollRunnable = object : Runnable {
        override fun run() {
            if (!polling) {
                return
            }

            refreshPlayback()
            mainHandler.postDelayed(this, POLL_INTERVAL_MS)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bindViews()
        bindActions()

        clientIdInput.setText(BlitzClient.savedClientId(this))
        autoDjQuery.setText(BlitzClient.savedQuery(this))

        updateAccountUi()
        handleRedirect(intent)
        refreshAccount()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleRedirect(intent)
    }

    override fun onResume() {
        super.onResume()
        polling = true
        mainHandler.post(pollRunnable)
    }

    override fun onPause() {
        polling = false
        mainHandler.removeCallbacks(pollRunnable)
        super.onPause()
    }

    private fun bindViews() {
        statusText = findViewById(R.id.statusText)
        loginPanel = findViewById(R.id.loginPanel)
        clientIdInput = findViewById(R.id.clientIdInput)
        connectButton = findViewById(R.id.connectButton)
        playerPanel = findViewById(R.id.playerPanel)
        trackName = findViewById(R.id.trackName)
        trackArtist = findViewById(R.id.trackArtist)
        trackProgress = findViewById(R.id.trackProgress)
        deviceText = findViewById(R.id.deviceText)
        previousButton = findViewById(R.id.previousButton)
        playPauseButton = findViewById(R.id.playPauseButton)
        nextButton = findViewById(R.id.nextButton)
        volumeBar = findViewById(R.id.volumeBar)
        openSpotifyButton = findViewById(R.id.openSpotifyButton)
        devicesButton = findViewById(R.id.devicesButton)
        autoDjPanel = findViewById(R.id.autoDjPanel)
        autoDjSwitch = findViewById(R.id.autoDjSwitch)
        autoDjQuery = findViewById(R.id.autoDjQuery)
        queueButton = findViewById(R.id.queueButton)
        searchPanel = findViewById(R.id.searchPanel)
        searchInput = findViewById(R.id.searchInput)
        searchButton = findViewById(R.id.searchButton)
        resultsContainer = findViewById(R.id.resultsContainer)
    }

    private fun bindActions() {
        connectButton.setOnClickListener { onConnectClicked() }

        previousButton.setOnClickListener { command { NativeBridge.previous(it) } }
        nextButton.setOnClickListener { command { NativeBridge.next(it) } }
        playPauseButton.setOnClickListener {
            val pause = playing
            command { if (pause) NativeBridge.pause(it) else NativeBridge.play(it) }
        }

        volumeBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(bar: SeekBar, progress: Int, fromUser: Boolean) = Unit

            override fun onStartTrackingTouch(bar: SeekBar) {
                draggingVolume = true
            }

            override fun onStopTrackingTouch(bar: SeekBar) {
                draggingVolume = false
                val volume = bar.progress
                command(refresh = false) { NativeBridge.setVolume(it, volume) }
            }
        })

        openSpotifyButton.setOnClickListener { openSpotify() }
        devicesButton.setOnClickListener { showDevices() }

        autoDjSwitch.setOnCheckedChangeListener { _, checked ->
            if (!updatingControls) {
                setAutoDJ(checked)
            }
        }

        autoDjQuery.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_DONE) {
                applyQuery()
                hideKeyboard()
                true
            } else {
                false
            }
        }

        queueButton.setOnClickListener {
            val query = applyQuery()
            command(message = getString(R.string.done)) {
                NativeBridge.setSearchQuery(it, query)
                NativeBridge.queueRecommendations(it, 5)
            }
        }

        searchButton.setOnClickListener { search() }
        searchInput.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_SEARCH) {
                search()
                true
            } else {
                false
            }
        }
    }

    private fun onConnectClicked() {
        if (authenticated) {
            AutoDJService.stop(this)
            background({ NativeBridge.logout(it); true }) { refreshAccount() }
            return
        }

        val clientId = clientIdInput.text.toString().trim()

        if (clientId.isEmpty()) {
            toast(getString(R.string.client_id_required))
            return
        }

        BlitzClient.saveClientId(this, clientId)

        BlitzClient.executor.execute {
            val handle = BlitzClient.handle(this)
            val url = if (handle != 0L) NativeBridge.authorizationUrl(handle) else ""
            val error = if (handle != 0L) NativeBridge.lastError(handle) else ""

            mainHandler.post {
                if (url.isEmpty()) {
                    toast(error.ifEmpty { getString(R.string.client_id_required) })
                } else {
                    startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
                }
            }
        }
    }

    private fun handleRedirect(intent: Intent?) {
        if (intent == null) {
            return
        }

        val data = intent.data ?: return

        if (data.scheme != "blitzdj") {
            return
        }

        intent.data = null
        val url = data.toString()

        BlitzClient.executor.execute {
            val handle = BlitzClient.handle(this)
            val success = handle != 0L && NativeBridge.handleRedirect(handle, url)
            val error = if (handle != 0L) NativeBridge.lastError(handle) else ""

            mainHandler.post {
                toast(if (success) getString(R.string.login_success) else error)
                refreshAccount()
            }
        }
    }

    private fun refreshAccount() {
        BlitzClient.executor.execute {
            val handle = BlitzClient.handle(this)
            val isAuthenticated = handle != 0L && NativeBridge.isAuthenticated(handle)
            val autoDjRunning = handle != 0L && NativeBridge.isAutoDJRunning(handle)

            mainHandler.post {
                authenticated = isAuthenticated
                updatingControls = true
                autoDjSwitch.isChecked = autoDjRunning
                updatingControls = false
                updateAccountUi()

                if (isAuthenticated) {
                    refreshPlayback()
                }
            }
        }
    }

    private fun updateAccountUi() {
        val connectedVisibility = if (authenticated) View.VISIBLE else View.GONE

        loginPanel.visibility = if (authenticated) View.GONE else View.VISIBLE
        playerPanel.visibility = connectedVisibility
        autoDjPanel.visibility = connectedVisibility
        searchPanel.visibility = connectedVisibility

        connectButton.setText(if (authenticated) R.string.logout else R.string.connect)
        statusText.setText(if (authenticated) R.string.connected else R.string.disconnected)
    }

    private fun refreshPlayback() {
        if (!authenticated || pollInFlight) {
            return
        }

        pollInFlight = true

        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()
            val json = if (handle != 0L) NativeBridge.playbackJson(handle) else "{}"
            val running = handle != 0L && NativeBridge.isAutoDJRunning(handle)

            mainHandler.post {
                pollInFlight = false
                renderPlayback(json, running)
            }
        }
    }

    private fun renderPlayback(json: String, autoDjRunning: Boolean) {
        val playback = runCatching { JSONObject(json) }.getOrNull() ?: JSONObject()

        updatingControls = true
        autoDjSwitch.isChecked = autoDjRunning
        updatingControls = false

        if (!playback.optBoolean("active")) {
            playing = false
            trackName.setText(R.string.nothing_playing)
            trackArtist.setText(R.string.open_spotify_hint)
            deviceText.text = ""
            trackProgress.progress = 0
            playPauseButton.setText(R.string.play)
            return
        }

        val track = playback.optJSONObject("track") ?: JSONObject()
        val device = playback.optJSONObject("device") ?: JSONObject()

        playing = playback.optBoolean("playing")

        trackName.text = track.optString("name").ifEmpty { getString(R.string.nothing_playing) }
        trackArtist.text = track.optString("artist")
        deviceText.text = device.optString("name")
        playPauseButton.setText(if (playing) R.string.pause else R.string.play)

        val duration = playback.optInt("duration_ms")
        val progress = playback.optInt("progress_ms")
        trackProgress.progress = if (duration > 0) (progress.toLong() * 1000 / duration).toInt() else 0

        if (!draggingVolume) {
            volumeBar.progress = device.optInt("volume", volumeBar.progress)
        }
    }

    private fun setAutoDJ(enabled: Boolean) {
        if (enabled) {
            val query = applyQuery()
            requestNotificationPermission()

            BlitzClient.executor.execute {
                val handle = BlitzClient.currentHandle()

                if (handle != 0L) {
                    NativeBridge.setSearchQuery(handle, query)
                }

                mainHandler.post { AutoDJService.start(this) }
            }
        } else {
            AutoDJService.stop(this)
        }

        mainHandler.postDelayed({ refreshPlayback() }, 1500)
    }

    private fun applyQuery(): String {
        val query = autoDjQuery.text.toString().trim().ifEmpty { "electronic music" }
        BlitzClient.saveQuery(this, query)

        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()

            if (handle != 0L) {
                NativeBridge.setSearchQuery(handle, query)
            }
        }

        return query
    }

    private fun search() {
        val query = searchInput.text.toString().trim()

        if (query.isEmpty()) {
            return
        }

        hideKeyboard()

        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()
            val json = if (handle != 0L) NativeBridge.searchJson(handle, query, 10) else "[]"

            mainHandler.post { renderTracks(json) }
        }
    }

    private fun renderTracks(json: String) {
        resultsContainer.removeAllViews()

        val tracks = runCatching { JSONArray(json) }.getOrNull() ?: JSONArray()

        if (tracks.length() == 0) {
            resultsContainer.addView(infoText(getString(R.string.no_results)))
            return
        }

        for (index in 0 until tracks.length()) {
            val track = tracks.optJSONObject(index) ?: continue
            val uri = track.optString("uri")

            resultsContainer.addView(
                row(
                    title = track.optString("name"),
                    subtitle = track.optString("artist"),
                    primary = getString(R.string.play_track) to { command { NativeBridge.playTrack(it, uri) } },
                    secondary = getString(R.string.queue) to {
                        command(message = getString(R.string.done)) { NativeBridge.queueTrack(it, uri) }
                    }
                )
            )
        }
    }

    private fun showDevices() {
        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()
            val json = if (handle != 0L) NativeBridge.devicesJson(handle) else "[]"

            mainHandler.post {
                resultsContainer.removeAllViews()

                val devices = runCatching { JSONArray(json) }.getOrNull() ?: JSONArray()

                if (devices.length() == 0) {
                    resultsContainer.addView(infoText(getString(R.string.no_devices)))
                    return@post
                }

                for (index in 0 until devices.length()) {
                    val device = devices.optJSONObject(index) ?: continue
                    val id = device.optString("id")
                    val active = if (device.optBoolean("active")) " ●" else ""

                    resultsContainer.addView(
                        row(
                            title = device.optString("name") + active,
                            subtitle = device.optString("type"),
                            primary = getString(R.string.use_device) to {
                                command { NativeBridge.transferPlayback(it, id) }
                            },
                            secondary = null
                        )
                    )
                }

                findViewById<View>(R.id.rootScroll).post {
                    findViewById<android.widget.ScrollView>(R.id.rootScroll)
                        .smoothScrollTo(0, searchPanel.top)
                }
            }
        }
    }

    private fun row(
        title: String,
        subtitle: String,
        primary: Pair<String, () -> Unit>,
        secondary: Pair<String, () -> Unit>?
    ): View {
        val padding = (8 * resources.displayMetrics.density).toInt()

        val texts = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)

            addView(TextView(context).apply {
                text = title
                maxLines = 1
                ellipsize = android.text.TextUtils.TruncateAt.END
                setTextColor(getColor(R.color.text_primary))
                textSize = 15f
            })

            addView(TextView(context).apply {
                text = subtitle
                maxLines = 1
                ellipsize = android.text.TextUtils.TruncateAt.END
                setTextColor(getColor(R.color.text_secondary))
                textSize = 13f
            })
        }

        return LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, padding, 0, padding)
            gravity = android.view.Gravity.CENTER_VERTICAL

            addView(texts)

            addView(Button(context).apply {
                text = primary.first
                setOnClickListener { primary.second() }
            })

            if (secondary != null) {
                addView(Button(context).apply {
                    text = secondary.first
                    setOnClickListener { secondary.second() }
                })
            }
        }
    }

    private fun infoText(message: String) = TextView(this).apply {
        text = message
        setTextColor(getColor(R.color.text_secondary))
        setPadding(0, 16, 0, 16)
    }

    private fun openSpotify() {
        val launch = packageManager.getLaunchIntentForPackage(SPOTIFY_PACKAGE)

        if (launch != null) {
            startActivity(launch)
        } else {
            startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("https://open.spotify.com")))
        }
    }

    private fun command(
        refresh: Boolean = true,
        message: String? = null,
        action: (Long) -> Boolean
    ) {
        background(action) { success ->
            if (success) {
                message?.let { toast(it) }
            }

            if (refresh) {
                mainHandler.postDelayed({ refreshPlayback() }, 600)
            }
        }
    }

    private fun background(action: (Long) -> Boolean, done: (Boolean) -> Unit) {
        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()
            val success = handle != 0L && action(handle)
            val error = if (!success && handle != 0L) NativeBridge.lastError(handle) else ""

            mainHandler.post {
                if (!success && error.isNotEmpty()) {
                    toast(error)
                }

                done(success)
            }
        }
    }

    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
        ) {
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), NOTIFICATION_REQUEST)
        }
    }

    private fun hideKeyboard() {
        val manager = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
        currentFocus?.let { manager.hideSoftInputFromWindow(it.windowToken, 0) }
    }

    private fun toast(message: String) {
        if (message.isNotEmpty()) {
            Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
        }
    }

    companion object {
        private const val POLL_INTERVAL_MS = 3000L
        private const val NOTIFICATION_REQUEST = 10
        private const val SPOTIFY_PACKAGE = "com.spotify.music"
    }
}
