package com.theblitzteam.blitzdj

import android.Manifest
import android.app.PendingIntent
import android.content.Intent
import android.content.IntentFilter
import android.content.res.ColorStateList
import android.graphics.Bitmap
import android.graphics.RenderEffect
import android.graphics.Shader
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.nfc.NfcAdapter
import android.provider.Settings
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.View
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.graphics.ColorUtils
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.doOnLayout
import androidx.palette.graphics.Palette
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import coil.load
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.google.android.material.button.MaterialButton
import com.google.android.material.card.MaterialCardView
import com.google.android.material.chip.Chip
import com.google.android.material.chip.ChipGroup
import com.google.android.material.floatingactionbutton.FloatingActionButton
import com.google.android.material.imageview.ShapeableImageView
import com.google.android.material.materialswitch.MaterialSwitch
import com.google.android.material.progressindicator.LinearProgressIndicator
import com.google.android.material.slider.Slider
import com.google.android.material.textfield.TextInputEditText
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.min

class MainActivity : AppCompatActivity() {
    private val mainHandler = Handler(Looper.getMainLooper())

    private lateinit var root: View
    private lateinit var content: View
    private lateinit var backgroundArt: ImageView
    private lateinit var backgroundTint: View
    private lateinit var loginOverlay: View
    private lateinit var clientIdInput: TextInputEditText
    private lateinit var connectButton: MaterialButton
    private lateinit var bottomNav: BottomNavigationView

    private lateinit var pagePlayer: View
    private lateinit var pageAutoDj: View
    private lateinit var pageSearch: View
    private lateinit var pageSpeakers: View

    private lateinit var playingFrom: TextView
    private lateinit var deviceChip: MaterialButton
    private lateinit var albumArt: ShapeableImageView
    private lateinit var trackTitle: TextView
    private lateinit var trackArtist: TextView
    private lateinit var trackAlbum: TextView
    private lateinit var favoriteButton: MaterialButton
    private lateinit var progressSlider: Slider
    private lateinit var elapsedTime: TextView
    private lateinit var totalTime: TextView
    private lateinit var shuffleButton: MaterialButton
    private lateinit var previousButton: MaterialButton
    private lateinit var playPauseButton: FloatingActionButton
    private lateinit var nextButton: MaterialButton
    private lateinit var repeatButton: MaterialButton
    private lateinit var volumeSlider: Slider
    private lateinit var autoDjCard: MaterialCardView
    private lateinit var autoDjIcon: ImageView
    private lateinit var autoDjStatus: TextView
    private lateinit var playerAutoDjSwitch: MaterialSwitch
    private lateinit var queueTitle: TextView
    private lateinit var queueList: RecyclerView

    private lateinit var autoDjSwitch: MaterialSwitch
    private lateinit var autoDjQuery: TextInputEditText
    private lateinit var moodChips: ChipGroup
    private lateinit var queueButton: MaterialButton
    private lateinit var refreshSuggestions: MaterialButton
    private lateinit var suggestionProgress: LinearProgressIndicator
    private lateinit var suggestionEmpty: TextView
    private lateinit var suggestionList: RecyclerView

    private lateinit var searchInput: TextInputEditText
    private lateinit var searchProgress: LinearProgressIndicator
    private lateinit var searchEmpty: TextView
    private lateinit var searchResults: RecyclerView

    private lateinit var outputIcon: ShapeableImageView
    private lateinit var outputName: TextView
    private lateinit var outputSwitchButton: MaterialButton
    private lateinit var partySwitch: MaterialSwitch
    private lateinit var bluetoothSettingsButton: MaterialButton
    private lateinit var bluetoothEmpty: TextView
    private lateinit var bluetoothPermissionButton: MaterialButton
    private lateinit var bluetoothList: RecyclerView
    private lateinit var refreshDevices: MaterialButton
    private lateinit var connectEmpty: TextView
    private lateinit var connectList: RecyclerView
    private lateinit var scanButton: MaterialButton
    private lateinit var scanProgress: LinearProgressIndicator
    private lateinit var nearbyEmpty: TextView
    private lateinit var nearbyList: RecyclerView
    private lateinit var proximitySwitch: MaterialSwitch
    private lateinit var nfcCard: View
    private lateinit var nfcDescription: TextView
    private lateinit var nfcSettingsButton: MaterialButton
    private lateinit var openSpotifyButton: MaterialButton
    private lateinit var logoutButton: MaterialButton

    private lateinit var miniPlayer: MaterialCardView
    private lateinit var miniArt: ShapeableImageView
    private lateinit var miniTitle: TextView
    private lateinit var miniArtist: TextView
    private lateinit var miniPlayPause: MaterialButton
    private lateinit var miniNext: MaterialButton
    private lateinit var miniProgress: LinearProgressIndicator

    private lateinit var speakers: SpeakerManager

    private val queueAdapter = TrackAdapter(onPlay = { playUri(it.uri) }, onQueue = null)
    private val suggestionAdapter = TrackAdapter(onPlay = { playUri(it.uri) }, onQueue = { queueUri(it.uri) })
    private val searchAdapter = TrackAdapter(onPlay = { playUri(it.uri) }, onQueue = { queueUri(it.uri) })
    private val bluetoothAdapter = DeviceAdapter()
    private val connectAdapter = DeviceAdapter()
    private val nearbyAdapter = DeviceAdapter()

    private var nfcAdapter: NfcAdapter? = null
    private var proximityConnect = false
    private var scanRequested = false
    private var userScan = false

    private var authenticated = false
    private var hasPlayback = false
    private var playing = false
    private var shuffle = false
    private var repeatMode = "off"
    private var durationMs = 0
    private var progressMs = 0
    private var progressUpdatedAt = 0L
    private var currentTrackId = ""
    private var currentImageUrl = ""
    private var currentArtist = ""
    private var autoDjRunning = false
    private var draggingProgress = false
    private var draggingVolume = false
    private var updatingControls = false
    private var pollInFlight = false
    private var polling = false
    private var currentPage = R.id.nav_player
    private var lastOutputType: SpeakerManager.OutputType? = null
    private var resumeOnReconnect = false
    private var partyMode = false
    private var scanButtonUsed = false

    private val bluetoothPermission = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        speakers.connectProfile()
        refreshSpeakers()

        if (scanRequested) {
            scanRequested = false
            startScan()
        }

        if (proximityConnect && !speakers.hasScanPermission()) {
            setProximityConnect(false)
        }
    }

    private val proximityRunnable = object : Runnable {
        override fun run() {
            if (!polling || !proximityConnect) {
                return
            }

            if (speakers.currentOutput().type != SpeakerManager.OutputType.BLUETOOTH && !speakers.isDiscovering) {
                speakers.startDiscovery(clear = false)
            }

            mainHandler.postDelayed(this, PROXIMITY_INTERVAL_MS)
        }
    }

    private val notificationPermission = registerForActivityResult(ActivityResultContracts.RequestPermission()) { }

    private val pollRunnable = object : Runnable {
        override fun run() {
            if (!polling) {
                return
            }

            refreshPlayback()
            mainHandler.postDelayed(this, POLL_INTERVAL_MS)
        }
    }

    private val tickRunnable = object : Runnable {
        override fun run() {
            if (!polling) {
                return
            }

            renderProgress()
            mainHandler.postDelayed(this, TICK_INTERVAL_MS)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        setContentView(R.layout.activity_main)

        bindViews()
        setupInsets()
        setupLists()
        setupPlayer()
        setupAutoDj()
        setupSearch()
        setupSpeakers()
        setupLogin()

        speakers = SpeakerManager(this) { mainHandler.post { onSpeakersChanged() } }
        speakers.onNearbyChanged = { mainHandler.post { renderNearby() } }
        speakers.onConnecting = { name, success ->
            mainHandler.post {
                toast(getString(if (success) R.string.connecting_to else R.string.bluetooth_connect_fallback, name))

                if (!success) {
                    speakers.openBluetoothSettings(this)
                }
            }
        }

        nfcAdapter = NfcAdapter.getDefaultAdapter(this)
        proximityConnect = BlitzClient.proximityConnect(this) && speakers.hasScanPermission()
        speakers.autoConnect = proximityConnect
        updatingControls = true
        proximitySwitch.isChecked = proximityConnect
        updatingControls = false

        partyMode = BlitzClient.partyMode(this)
        updatingControls = true
        partySwitch.isChecked = partyMode
        updatingControls = false
        applyPartyMode()

        showPage(R.id.nav_player)

        if (isRedirect(intent)) {
            BlitzClient.autoLoginAttempted = true
            handleRedirect(intent)
        } else {
            refreshAccount(autoLogin = true)
        }

        if (NfcHandover.isHandover(intent)) {
            handleNfc(intent)
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)

        if (NfcHandover.isHandover(intent)) {
            handleNfc(intent)
        } else {
            handleRedirect(intent)
        }
    }

    override fun onResume() {
        super.onResume()
        polling = true
        speakers.start()
        onSpeakersChanged()
        mainHandler.post(pollRunnable)
        mainHandler.post(tickRunnable)
        enableNfcDispatch()
        renderNfc()

        if (proximityConnect) {
            mainHandler.postDelayed(proximityRunnable, 1500)
        }
    }

    override fun onPause() {
        polling = false
        mainHandler.removeCallbacks(pollRunnable)
        mainHandler.removeCallbacks(tickRunnable)
        mainHandler.removeCallbacks(proximityRunnable)
        runCatching { nfcAdapter?.disableForegroundDispatch(this) }

        if (!partyMode) {
            speakers.stopDiscovery()
        }

        if (!partyMode) {
            speakers.stop()
        }

        super.onPause()
    }

    override fun onDestroy() {
        speakers.stop()
        super.onDestroy()
    }

    private fun bindViews() {
        root = findViewById(R.id.root)
        content = findViewById(R.id.content)
        backgroundArt = findViewById(R.id.backgroundArt)
        backgroundTint = findViewById(R.id.backgroundTint)
        loginOverlay = findViewById(R.id.loginOverlay)
        clientIdInput = findViewById(R.id.clientIdInput)
        connectButton = findViewById(R.id.connectButton)
        bottomNav = findViewById(R.id.bottomNav)

        pagePlayer = findViewById(R.id.pagePlayer)
        pageAutoDj = findViewById(R.id.pageAutoDj)
        pageSearch = findViewById(R.id.pageSearch)
        pageSpeakers = findViewById(R.id.pageSpeakers)

        playingFrom = findViewById(R.id.playingFrom)
        deviceChip = findViewById(R.id.deviceChip)
        albumArt = findViewById(R.id.albumArt)
        trackTitle = findViewById(R.id.trackTitle)
        trackArtist = findViewById(R.id.trackArtist)
        trackAlbum = findViewById(R.id.trackAlbum)
        favoriteButton = findViewById(R.id.favoriteButton)
        progressSlider = findViewById(R.id.progressSlider)
        elapsedTime = findViewById(R.id.elapsedTime)
        totalTime = findViewById(R.id.totalTime)
        shuffleButton = findViewById(R.id.shuffleButton)
        previousButton = findViewById(R.id.previousButton)
        playPauseButton = findViewById(R.id.playPauseButton)
        nextButton = findViewById(R.id.nextButton)
        repeatButton = findViewById(R.id.repeatButton)
        volumeSlider = findViewById(R.id.volumeSlider)
        autoDjCard = findViewById(R.id.autoDjCard)
        autoDjIcon = findViewById(R.id.autoDjIcon)
        autoDjStatus = findViewById(R.id.autoDjStatus)
        playerAutoDjSwitch = findViewById(R.id.playerAutoDjSwitch)
        queueTitle = findViewById(R.id.queueTitle)
        queueList = findViewById(R.id.queueList)

        autoDjSwitch = findViewById(R.id.autoDjSwitch)
        autoDjQuery = findViewById(R.id.autoDjQuery)
        moodChips = findViewById(R.id.moodChips)
        queueButton = findViewById(R.id.queueButton)
        refreshSuggestions = findViewById(R.id.refreshSuggestions)
        suggestionProgress = findViewById(R.id.suggestionProgress)
        suggestionEmpty = findViewById(R.id.suggestionEmpty)
        suggestionList = findViewById(R.id.suggestionList)

        searchInput = findViewById(R.id.searchInput)
        searchProgress = findViewById(R.id.searchProgress)
        searchEmpty = findViewById(R.id.searchEmpty)
        searchResults = findViewById(R.id.searchResults)

        outputIcon = findViewById(R.id.outputIcon)
        outputName = findViewById(R.id.outputName)
        outputSwitchButton = findViewById(R.id.outputSwitchButton)
        partySwitch = findViewById(R.id.partySwitch)
        bluetoothSettingsButton = findViewById(R.id.bluetoothSettingsButton)
        bluetoothEmpty = findViewById(R.id.bluetoothEmpty)
        bluetoothPermissionButton = findViewById(R.id.bluetoothPermissionButton)
        bluetoothList = findViewById(R.id.bluetoothList)
        refreshDevices = findViewById(R.id.refreshDevices)
        connectEmpty = findViewById(R.id.connectEmpty)
        connectList = findViewById(R.id.connectList)
        scanButton = findViewById(R.id.scanButton)
        scanProgress = findViewById(R.id.scanProgress)
        nearbyEmpty = findViewById(R.id.nearbyEmpty)
        nearbyList = findViewById(R.id.nearbyList)
        proximitySwitch = findViewById(R.id.proximitySwitch)
        nfcCard = findViewById(R.id.nfcCard)
        nfcDescription = findViewById(R.id.nfcDescription)
        nfcSettingsButton = findViewById(R.id.nfcSettingsButton)
        openSpotifyButton = findViewById(R.id.openSpotifyButton)
        logoutButton = findViewById(R.id.logoutButton)

        miniPlayer = findViewById(R.id.miniPlayer)
        miniArt = findViewById(R.id.miniArt)
        miniTitle = findViewById(R.id.miniTitle)
        miniArtist = findViewById(R.id.miniArtist)
        miniPlayPause = findViewById(R.id.miniPlayPause)
        miniNext = findViewById(R.id.miniNext)
        miniProgress = findViewById(R.id.miniProgress)
    }

    private fun setupInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(root) { _, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            content.setPadding(bars.left, bars.top, bars.right, 0)
            loginOverlay.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            insets
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            backgroundArt.setRenderEffect(RenderEffect.createBlurEffect(90f, 90f, Shader.TileMode.CLAMP))
        } else {
            backgroundArt.alpha = 0.25f
        }

        albumArt.doOnLayout { view ->
            val screenHeight = resources.displayMetrics.heightPixels
            val size = min(view.width, (screenHeight * 0.42f).toInt())
            val params = view.layoutParams
            params.width = size
            params.height = size
            view.layoutParams = params
        }
    }

    private fun setupLists() {
        listOf(
            queueList to queueAdapter,
            suggestionList to suggestionAdapter,
            searchResults to searchAdapter,
            bluetoothList to bluetoothAdapter,
            nearbyList to nearbyAdapter,
            connectList to connectAdapter
        ).forEach { (list, adapter) ->
            list.layoutManager = LinearLayoutManager(this)
            list.adapter = adapter
            list.itemAnimator = null
        }
    }

    private fun setupPlayer() {
        bottomNav.setOnItemSelectedListener { item ->
            showPage(item.itemId)
            true
        }

        bottomNav.setOnItemReselectedListener { }

        deviceChip.setOnClickListener { bottomNav.selectedItemId = R.id.nav_speakers }
        miniPlayer.setOnClickListener { bottomNav.selectedItemId = R.id.nav_player }
        autoDjCard.setOnClickListener { bottomNav.selectedItemId = R.id.nav_auto_dj }

        playPauseButton.setOnClickListener { togglePlayback() }
        miniPlayPause.setOnClickListener { togglePlayback() }
        previousButton.setOnClickListener { command { NativeBridge.previous(it) } }
        nextButton.setOnClickListener { command { NativeBridge.next(it) } }
        miniNext.setOnClickListener { command { NativeBridge.next(it) } }

        shuffleButton.setOnClickListener {
            val enabled = !shuffle
            shuffle = enabled
            renderModes()
            command { NativeBridge.setShuffle(it, enabled) }
        }

        repeatButton.setOnClickListener {
            val next = when (repeatMode) {
                "off" -> "context"
                "context" -> "track"
                else -> "off"
            }

            repeatMode = next
            renderModes()
            command { NativeBridge.setRepeat(it, next) }
        }

        favoriteButton.setOnClickListener {
            val artist = currentArtist.substringBefore(",").trim()

            if (artist.isNotEmpty()) {
                background({ NativeBridge.addFavoriteArtist(it, artist); true }) {
                    toast(getString(R.string.favorite_added, artist))
                }
            }
        }

        progressSlider.addOnSliderTouchListener(object : Slider.OnSliderTouchListener {
            override fun onStartTrackingTouch(slider: Slider) {
                draggingProgress = true
            }

            override fun onStopTrackingTouch(slider: Slider) {
                draggingProgress = false

                if (durationMs <= 0) {
                    return
                }

                val position = (slider.value / PROGRESS_SCALE * durationMs).toInt()
                progressMs = position
                progressUpdatedAt = SystemClock.elapsedRealtime()
                command { NativeBridge.seek(it, position) }
            }
        })

        progressSlider.addOnChangeListener { _, value, fromUser ->
            if (fromUser && durationMs > 0) {
                elapsedTime.text = formatTime((value / PROGRESS_SCALE * durationMs).toInt())
            }
        }

        volumeSlider.setLabelFormatter { value -> "${value.toInt()}%" }
        volumeSlider.addOnSliderTouchListener(object : Slider.OnSliderTouchListener {
            override fun onStartTrackingTouch(slider: Slider) {
                draggingVolume = true
            }

            override fun onStopTrackingTouch(slider: Slider) {
                draggingVolume = false
                val volume = slider.value.toInt()
                command(refresh = false) { NativeBridge.setVolume(it, volume) }
            }
        })

        playerAutoDjSwitch.setOnCheckedChangeListener { _, checked ->
            if (!updatingControls) {
                setAutoDJ(checked)
            }
        }
    }

    private fun setupAutoDj() {
        autoDjQuery.setText(BlitzClient.savedQuery(this))

        resources.getStringArray(R.array.moods).forEach { mood ->
            val chip = Chip(this).apply {
                text = mood
                isCheckable = true
                isCheckedIconVisible = false
                setOnClickListener {
                    autoDjQuery.setText(mood)
                    applyQuery()
                    loadSuggestions()
                }
            }

            moodChips.addView(chip)
        }

        autoDjSwitch.setOnCheckedChangeListener { _, checked ->
            if (!updatingControls) {
                setAutoDJ(checked)
            }
        }

        autoDjQuery.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_DONE) {
                applyQuery()
                hideKeyboard()
                loadSuggestions()
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

            mainHandler.postDelayed({ loadQueue() }, 1200)
        }

        refreshSuggestions.setOnClickListener { loadSuggestions() }
    }

    private fun setupSearch() {
        searchInput.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_SEARCH) {
                search()
                true
            } else {
                false
            }
        }
    }

    private fun setupSpeakers() {
        outputSwitchButton.setOnClickListener { speakers.openOutputPicker(this) }
        bluetoothSettingsButton.setOnClickListener { speakers.openBluetoothSettings(this) }
        refreshDevices.setOnClickListener { loadConnectDevices() }
        openSpotifyButton.setOnClickListener { openSpotify() }
        logoutButton.setOnClickListener { logout() }

        bluetoothPermissionButton.setOnClickListener {
            bluetoothPermission.launch(speakers.requiredPermissions())
        }

        scanButton.setOnClickListener {
            if (speakers.isDiscovering) {
                speakers.stopDiscovery()
                renderNearby()
            } else {
                startScan()
            }
        }

        proximitySwitch.setOnCheckedChangeListener { _, checked ->
            if (!updatingControls) {
                setProximityConnect(checked)
            }
        }

        nfcSettingsButton.setOnClickListener {
            runCatching { startActivity(Intent(Settings.ACTION_NFC_SETTINGS)) }
                .onFailure { runCatching { startActivity(Intent(Settings.ACTION_WIRELESS_SETTINGS)) } }
        }

        partySwitch.setOnCheckedChangeListener { _, checked ->
            if (!updatingControls) {
                partyMode = checked
                BlitzClient.savePartyMode(this, checked)
                applyPartyMode()

                if (checked && !autoDjRunning) {
                    setAutoDJ(true)
                }
            }
        }
    }

    private fun setupLogin() {
        clientIdInput.setText(BlitzClient.savedClientId(this))

        connectButton.setOnClickListener {
            val clientId = clientIdInput.text?.toString()?.trim().orEmpty()

            if (clientId.isEmpty()) {
                toast(getString(R.string.client_id_required))
                return@setOnClickListener
            }

            BlitzClient.saveClientId(this, clientId)
            startLogin()
        }
    }

    private fun showPage(id: Int) {
        currentPage = id

        pagePlayer.visibility = if (id == R.id.nav_player) View.VISIBLE else View.GONE
        pageAutoDj.visibility = if (id == R.id.nav_auto_dj) View.VISIBLE else View.GONE
        pageSearch.visibility = if (id == R.id.nav_search) View.VISIBLE else View.GONE
        pageSpeakers.visibility = if (id == R.id.nav_speakers) View.VISIBLE else View.GONE

        updateMiniPlayer()

        when (id) {
            R.id.nav_player -> loadQueue()
            R.id.nav_auto_dj -> if (suggestionAdapter.isEmpty()) loadSuggestions()
            R.id.nav_speakers -> {
                refreshSpeakers()
                loadConnectDevices()
                renderNearby()
                renderNfc()
            }
        }
    }

    private fun showLogin(visible: Boolean) {
        loginOverlay.visibility = if (visible) View.VISIBLE else View.GONE
        bottomNav.visibility = if (visible) View.GONE else View.VISIBLE
        updateMiniPlayer()
    }

    private fun updateMiniPlayer() {
        val visible = authenticated && hasPlayback && currentPage != R.id.nav_player
        miniPlayer.visibility = if (visible) View.VISIBLE else View.GONE
    }

    private fun startLogin() {
        BlitzClient.autoLoginAttempted = true

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

    private fun isRedirect(intent: Intent?): Boolean =
        intent?.data?.scheme == "blitzdj"

    private fun handleRedirect(intent: Intent?) {
        if (intent == null || !isRedirect(intent)) {
            return
        }

        val url = intent.data.toString()
        intent.data = null

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

    private fun refreshAccount(autoLogin: Boolean = false) {
        BlitzClient.executor.execute {
            val handle = BlitzClient.handle(this)
            val isAuthenticated = handle != 0L && NativeBridge.isAuthenticated(handle)

            mainHandler.post {
                authenticated = isAuthenticated
                showLogin(!isAuthenticated)

                if (isAuthenticated) {
                    refreshPlayback()
                    loadQueue()
                } else if (autoLogin && !BlitzClient.autoLoginAttempted && handle != 0L) {
                    startLogin()
                }
            }
        }
    }

    private fun logout() {
        AutoDJService.stop(this)

        background({ NativeBridge.logout(it); true }) {
            authenticated = false
            hasPlayback = false
            currentTrackId = ""
            currentImageUrl = ""
            showLogin(true)
        }
    }

    private fun refreshPlayback() {
        if (!authenticated || pollInFlight) {
            return
        }

        pollInFlight = true

        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()
            val json = if (handle != 0L) NativeBridge.playbackJson(handle) else "{}"

            mainHandler.post {
                pollInFlight = false
                renderPlayback(json)
            }
        }
    }

    private fun renderPlayback(json: String) {
        val playback = runCatching { JSONObject(json) }.getOrNull() ?: JSONObject()

        hasPlayback = playback.optBoolean("active")
        autoDjRunning = playback.optBoolean("auto_dj", autoDjRunning)
        renderAutoDj()

        if (!hasPlayback) {
            playing = false
            durationMs = 0
            progressMs = 0
            currentTrackId = ""
            trackTitle.setText(R.string.nothing_playing)
            trackArtist.setText(R.string.open_spotify_hint)
            trackAlbum.text = ""
            playingFrom.setText(R.string.app_name)
            deviceChip.setText(R.string.devices)
            loadArt("")
            renderPlayState()
            renderProgress()
            updateMiniPlayer()
            return
        }

        val track = playback.optJSONObject("track") ?: JSONObject()
        val device = playback.optJSONObject("device") ?: JSONObject()

        playing = playback.optBoolean("playing")
        shuffle = playback.optBoolean("shuffle")
        repeatMode = playback.optString("repeat", "off")
        durationMs = playback.optInt("duration_ms")
        progressMs = playback.optInt("progress_ms")
        progressUpdatedAt = SystemClock.elapsedRealtime()

        val trackId = track.optString("id")
        val title = track.optString("name").ifEmpty { getString(R.string.nothing_playing) }
        currentArtist = track.optString("artist")

        trackTitle.text = title
        trackTitle.isSelected = true
        trackArtist.text = currentArtist
        trackAlbum.text = track.optString("album")
        miniTitle.text = title
        miniArtist.text = currentArtist

        val deviceName = device.optString("name")
        deviceChip.text = deviceName.ifEmpty { getString(R.string.devices) }
        deviceChip.setIconResource(deviceIcon(device.optString("type")))
        playingFrom.text = if (deviceName.isNotEmpty()) getString(R.string.playing_on, deviceName) else getString(R.string.app_name)

        if (!draggingVolume && device.has("volume")) {
            volumeSlider.value = device.optInt("volume").coerceIn(0, 100).toFloat()
        }

        loadArt(track.optString("image_url"))

        if (trackId != currentTrackId) {
            currentTrackId = trackId
            loadQueue()
        }

        renderPlayState()
        renderModes()
        renderProgress()
        updateMiniPlayer()
    }

    private fun renderPlayState() {
        val icon = if (playing) R.drawable.ic_pause else R.drawable.ic_play
        playPauseButton.setImageResource(icon)
        playPauseButton.contentDescription = getString(if (playing) R.string.pause else R.string.play)
        miniPlayPause.setIconResource(icon)
    }

    private fun renderModes() {
        val active = ContextCompat.getColor(this, R.color.accent)
        val inactive = ContextCompat.getColor(this, R.color.text_secondary)

        shuffleButton.iconTint = ColorStateList.valueOf(if (shuffle) active else inactive)
        repeatButton.iconTint = ColorStateList.valueOf(if (repeatMode == "off") inactive else active)
        repeatButton.setIconResource(if (repeatMode == "track") R.drawable.ic_repeat_one else R.drawable.ic_repeat)
    }

    private fun renderProgress() {
        if (!hasPlayback || durationMs <= 0) {
            if (!draggingProgress) {
                progressSlider.value = 0f
            }

            elapsedTime.setText(R.string.zero_time)
            totalTime.setText(R.string.zero_time)
            miniProgress.progress = 0
            return
        }

        val elapsed = if (playing) SystemClock.elapsedRealtime() - progressUpdatedAt else 0L
        val position = (progressMs + elapsed).coerceIn(0L, durationMs.toLong()).toInt()
        val fraction = position.toFloat() / durationMs

        if (!draggingProgress) {
            progressSlider.value = (fraction * PROGRESS_SCALE).coerceIn(0f, PROGRESS_SCALE)
            elapsedTime.text = formatTime(position)
        }

        totalTime.text = formatTime(durationMs)
        miniProgress.max = PROGRESS_SCALE.toInt()
        miniProgress.progress = (fraction * PROGRESS_SCALE).toInt()

        if (playing && position >= durationMs) {
            refreshPlayback()
        }
    }

    private fun renderAutoDj() {
        updatingControls = true
        autoDjSwitch.isChecked = autoDjRunning
        playerAutoDjSwitch.isChecked = autoDjRunning
        updatingControls = false

        val query = BlitzClient.savedQuery(this)
        autoDjStatus.text = if (autoDjRunning) getString(R.string.auto_dj_on, query) else getString(R.string.auto_dj_off)
        autoDjIcon.imageTintList = ColorStateList.valueOf(
            ContextCompat.getColor(this, if (autoDjRunning) R.color.accent else R.color.text_secondary)
        )
    }

    private fun loadArt(url: String) {
        if (url == currentImageUrl) {
            return
        }

        currentImageUrl = url

        if (url.isEmpty()) {
            albumArt.setImageDrawable(null)
            miniArt.setImageDrawable(null)
            backgroundArt.setImageDrawable(null)
            applyColors(ContextCompat.getColor(this, R.color.accent), ContextCompat.getColor(this, R.color.background))
            return
        }

        albumArt.load(url) {
            crossfade(300)
            allowHardware(false)
            listener(onSuccess = { _, result ->
                (result.drawable as? BitmapDrawable)?.bitmap?.let(::extractColors)
            })
        }

        miniArt.load(url) { crossfade(true) }
        backgroundArt.load(url) { crossfade(600) }
    }

    private fun extractColors(bitmap: Bitmap) {
        Palette.from(bitmap).maximumColorCount(16).generate { palette ->
            val fallbackAccent = ContextCompat.getColor(this, R.color.accent)
            val fallbackDark = ContextCompat.getColor(this, R.color.background)

            if (palette == null) {
                applyColors(fallbackAccent, fallbackDark)
                return@generate
            }

            val vibrant = palette.getVibrantColor(palette.getLightVibrantColor(fallbackAccent))
            val dark = palette.getDarkMutedColor(palette.getDarkVibrantColor(fallbackDark))

            applyColors(vibrant, dark)
        }
    }

    private fun applyColors(accent: Int, dark: Int) {
        val background = ContextCompat.getColor(this, R.color.background)
        val top = ColorUtils.setAlphaComponent(ColorUtils.blendARGB(dark, background, 0.2f), 0x99)
        val middle = ColorUtils.setAlphaComponent(ColorUtils.blendARGB(dark, background, 0.6f), 0xDD)

        backgroundTint.background = GradientDrawable(
            GradientDrawable.Orientation.TOP_BOTTOM,
            intArrayOf(top, middle, background)
        )

        val readable = if (ColorUtils.calculateLuminance(accent) < 0.25) {
            ColorUtils.blendARGB(accent, 0xFFFFFFFF.toInt(), 0.45f)
        } else {
            accent
        }

        val accentList = ColorStateList.valueOf(readable)
        progressSlider.trackActiveTintList = accentList
        progressSlider.thumbTintList = accentList
        miniProgress.setIndicatorColor(readable)
    }

    private fun togglePlayback() {
        val pause = playing
        playing = !pause
        progressUpdatedAt = SystemClock.elapsedRealtime()
        renderPlayState()
        command { if (pause) NativeBridge.pause(it) else NativeBridge.play(it) }
    }

    private fun playUri(uri: String) {
        if (uri.isNotEmpty()) {
            command { NativeBridge.playTrack(it, uri) }
        }
    }

    private fun queueUri(uri: String) {
        if (uri.isEmpty()) {
            return
        }

        command(message = getString(R.string.queued)) { NativeBridge.queueTrack(it, uri) }
        mainHandler.postDelayed({ loadQueue() }, 800)
    }

    private fun loadQueue() {
        if (!authenticated) {
            return
        }

        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()
            val json = if (handle != 0L) NativeBridge.queueJson(handle, 10) else "[]"
            val tracks = TrackItem.list(json)

            mainHandler.post {
                queueAdapter.submit(tracks)
                queueTitle.visibility = if (tracks.isEmpty()) View.GONE else View.VISIBLE
            }
        }
    }

    private fun setAutoDJ(enabled: Boolean) {
        autoDjRunning = enabled
        renderAutoDj()

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
        val query = autoDjQuery.text?.toString()?.trim().orEmpty().ifEmpty { "party hits" }
        BlitzClient.saveQuery(this, query)
        renderAutoDj()

        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()

            if (handle != 0L) {
                NativeBridge.setSearchQuery(handle, query)
            }
        }

        return query
    }

    private fun loadSuggestions() {
        if (!authenticated) {
            return
        }

        val query = applyQuery()
        suggestionProgress.visibility = View.VISIBLE

        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()
            val json = if (handle != 0L) NativeBridge.recommendJson(handle, query, 8) else "[]"
            val tracks = TrackItem.list(json)

            mainHandler.post {
                suggestionProgress.visibility = View.GONE
                suggestionAdapter.submit(tracks)
                suggestionEmpty.visibility = if (tracks.isEmpty()) View.VISIBLE else View.GONE
            }
        }
    }

    private fun search() {
        val query = searchInput.text?.toString()?.trim().orEmpty()

        if (query.isEmpty()) {
            return
        }

        hideKeyboard()
        searchProgress.visibility = View.VISIBLE

        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()
            val json = if (handle != 0L) NativeBridge.searchJson(handle, query, 10) else "[]"
            val tracks = TrackItem.list(json)

            mainHandler.post {
                searchProgress.visibility = View.GONE
                searchAdapter.submit(tracks)
                searchEmpty.visibility = if (tracks.isEmpty()) View.VISIBLE else View.GONE
                searchEmpty.setText(if (tracks.isEmpty()) R.string.no_results else R.string.search_empty)
            }
        }
    }

    private fun onSpeakersChanged() {
        val output = speakers.currentOutput()
        val previous = lastOutputType
        lastOutputType = output.type

        if (previous == SpeakerManager.OutputType.BLUETOOTH && output.type != SpeakerManager.OutputType.BLUETOOTH) {
            resumeOnReconnect = partyMode && playing
        } else if (previous != null &&
            previous != SpeakerManager.OutputType.BLUETOOTH &&
            output.type == SpeakerManager.OutputType.BLUETOOTH &&
            resumeOnReconnect
        ) {
            resumeOnReconnect = false
            mainHandler.postDelayed({
                command { NativeBridge.play(it) }
                toast(getString(R.string.party_resumed))
            }, RECONNECT_RESUME_DELAY_MS)
        }

        renderOutput(output)

        if (currentPage == R.id.nav_speakers) {
            refreshSpeakers()
        }
    }

    private fun renderOutput(output: SpeakerManager.Output) {
        val (icon, name) = when (output.type) {
            SpeakerManager.OutputType.BLUETOOTH -> R.drawable.ic_bluetooth to (output.name ?: getString(R.string.bluetooth_speakers))
            SpeakerManager.OutputType.WIRED -> R.drawable.ic_volume to getString(R.string.wired_output)
            SpeakerManager.OutputType.USB -> R.drawable.ic_volume to (output.name ?: getString(R.string.wired_output))
            SpeakerManager.OutputType.PHONE -> R.drawable.ic_phone to getString(R.string.phone_speaker)
        }

        outputIcon.setImageResource(icon)
        outputName.text = name
    }

    private fun refreshSpeakers() {
        if (!::speakers.isInitialized) {
            return
        }

        val output = speakers.currentOutput()
        renderOutput(output)

        bluetoothPermissionButton.visibility = View.GONE

        when {
            !speakers.isBluetoothAvailable -> {
                bluetoothAdapter.submit(emptyList())
                bluetoothEmpty.visibility = View.GONE
                bluetoothSettingsButton.visibility = View.GONE
                return
            }

            !speakers.hasPermission() -> {
                bluetoothAdapter.submit(emptyList())
                bluetoothEmpty.visibility = View.VISIBLE
                bluetoothEmpty.setText(R.string.bluetooth_permission)
                bluetoothPermissionButton.visibility = View.VISIBLE
                return
            }

            !speakers.isBluetoothEnabled() -> {
                bluetoothAdapter.submit(emptyList())
                bluetoothEmpty.visibility = View.VISIBLE
                bluetoothEmpty.setText(R.string.bluetooth_off)
                return
            }
        }

        speakers.connectProfile()

        val list = speakers.speakers()
        val activeName = if (output.type == SpeakerManager.OutputType.BLUETOOTH) output.name else null

        bluetoothEmpty.visibility = if (list.isEmpty()) View.VISIBLE else View.GONE
        bluetoothEmpty.setText(R.string.bluetooth_empty)

        bluetoothAdapter.submit(list.map { speaker ->
            val active = speaker.connected && (activeName == null || activeName == speaker.name)

            DeviceItem(
                key = speaker.address,
                name = speaker.name,
                subtitle = getString(
                    when {
                        active -> R.string.playing_here
                        speaker.connected -> R.string.connected
                        else -> R.string.paired
                    }
                ),
                icon = R.drawable.ic_bluetooth,
                highlighted = speaker.connected,
                actionLabel = getString(if (speaker.connected) R.string.disconnect_device else R.string.connect_device),
                onAction = {
                    val success = if (speaker.connected) speakers.disconnect(speaker) else speakers.connect(speaker)

                    if (!success) {
                        toast(getString(R.string.bluetooth_connect_fallback))
                        speakers.openBluetoothSettings(this)
                    }

                    mainHandler.postDelayed({ refreshSpeakers() }, 1500)
                }
            )
        })
    }

    private fun loadConnectDevices() {
        if (!authenticated) {
            return
        }

        BlitzClient.executor.execute {
            val handle = BlitzClient.currentHandle()
            val json = if (handle != 0L) NativeBridge.devicesJson(handle) else "[]"
            val devices = runCatching { JSONArray(json) }.getOrNull() ?: JSONArray()

            mainHandler.post {
                val items = (0 until devices.length()).mapNotNull { index ->
                    val device = devices.optJSONObject(index) ?: return@mapNotNull null
                    val id = device.optString("id")
                    val active = device.optBoolean("active")
                    val type = device.optString("type")
                    val details = buildString {
                        append(type)

                        if (active) {
                            append(" • ")
                            append(getString(R.string.active_device))
                        }

                        if (device.has("volume")) {
                            append(" • ")
                            append(getString(R.string.volume_percent, device.optInt("volume")))
                        }
                    }

                    DeviceItem(
                        key = id,
                        name = device.optString("name"),
                        subtitle = details,
                        icon = deviceIcon(type),
                        highlighted = active,
                        actionLabel = if (active || id.isEmpty()) null else getString(R.string.use_device),
                        onAction = {
                            command { NativeBridge.transferPlayback(it, id) }
                            mainHandler.postDelayed({ loadConnectDevices() }, 1200)
                        }
                    )
                }

                connectAdapter.submit(items)
                connectEmpty.visibility = if (items.isEmpty()) View.VISIBLE else View.GONE
            }
        }
    }

    private fun startScan() {
        if (!speakers.hasScanPermission()) {
            scanRequested = true
            bluetoothPermission.launch(speakers.requiredPermissions())
            return
        }

        if (!speakers.isBluetoothEnabled()) {
            toast(getString(R.string.bluetooth_off))
            speakers.openBluetoothSettings(this)
            return
        }

        userScan = true
        speakers.startDiscovery(clear = true)
        renderNearby()
    }

    private fun setProximityConnect(enabled: Boolean) {
        if (enabled && !speakers.hasScanPermission()) {
            bluetoothPermission.launch(speakers.requiredPermissions())
        }

        proximityConnect = enabled
        speakers.autoConnect = enabled
        BlitzClient.saveProximityConnect(this, enabled)

        updatingControls = true
        proximitySwitch.isChecked = enabled
        updatingControls = false

        mainHandler.removeCallbacks(proximityRunnable)

        if (enabled && polling) {
            mainHandler.post(proximityRunnable)
        }
    }

    private fun renderNearby() {
        val scanning = speakers.isDiscovering
        val list = speakers.nearbySpeakers()

        scanProgress.visibility = if (scanning && userScan) View.VISIBLE else View.GONE
        scanButton.setText(if (scanning && userScan) R.string.scanning else R.string.scan)

        if (!scanning) {
            userScan = false
        }

        nearbyEmpty.visibility = if (list.isEmpty()) View.VISIBLE else View.GONE
        nearbyEmpty.setText(
            when {
                !speakers.hasScanPermission() -> R.string.scan_permission
                list.isEmpty() && !scanning && speakers.isBluetoothEnabled() && scanButtonUsed -> R.string.nearby_none_found
                else -> R.string.nearby_empty
            }
        )

        if (scanning) {
            scanButtonUsed = true
        }

        nearbyAdapter.submit(list.map { speaker ->
            val proximity = getString(
                when (speaker.proximity) {
                    SpeakerManager.Proximity.VERY_CLOSE -> R.string.proximity_very_close
                    SpeakerManager.Proximity.NEAR -> R.string.proximity_near
                    SpeakerManager.Proximity.IN_RANGE -> R.string.proximity_in_range
                    SpeakerManager.Proximity.FAR -> R.string.proximity_far
                }
            )

            val status = getString(if (speaker.bonded) R.string.paired else R.string.pair_and_connect)

            DeviceItem(
                key = speaker.address,
                name = speaker.name,
                subtitle = "$proximity • ${speaker.rssi} dBm • $status",
                icon = R.drawable.ic_bluetooth,
                highlighted = speaker.proximity == SpeakerManager.Proximity.VERY_CLOSE,
                actionLabel = getString(if (speaker.bonded) R.string.connect_device else R.string.pair_and_connect),
                onAction = { connectNearby(speaker.address, speaker.name) }
            )
        })
    }

    private fun connectNearby(address: String, name: String) {
        if (!speakers.hasPermission()) {
            bluetoothPermission.launch(speakers.requiredPermissions())
            return
        }

        if (speakers.pairAndConnect(address)) {
            toast(getString(R.string.connecting_to, name))
        } else {
            toast(getString(R.string.bluetooth_connect_fallback))
            speakers.openBluetoothSettings(this)
        }

        mainHandler.postDelayed({ refreshSpeakers() }, 2500)
    }

    private fun enableNfcDispatch() {
        val adapter = nfcAdapter ?: return

        val flags = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) PendingIntent.FLAG_MUTABLE else 0
        val pending = PendingIntent.getActivity(
            this,
            NFC_REQUEST,
            Intent(this, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            flags
        )

        val filters = listOf(NfcHandover.CLASSIC_MIME, NfcHandover.LOW_ENERGY_MIME).mapNotNull { mime ->
            runCatching { IntentFilter(NfcAdapter.ACTION_NDEF_DISCOVERED).apply { addDataType(mime) } }.getOrNull()
        }

        runCatching { adapter.enableForegroundDispatch(this, pending, filters.toTypedArray(), null) }
    }

    private fun renderNfc() {
        val adapter = nfcAdapter

        if (adapter == null) {
            nfcCard.visibility = View.GONE
            return
        }

        val enabled = runCatching { adapter.isEnabled }.getOrDefault(false)
        nfcCard.visibility = View.VISIBLE
        nfcDescription.setText(if (enabled) R.string.nfc_description else R.string.nfc_disabled)
        nfcSettingsButton.visibility = if (enabled) View.GONE else View.VISIBLE
    }

    private fun handleNfc(intent: Intent) {
        val target = NfcHandover.parse(intent)
        intent.action = null

        if (target == null) {
            toast(getString(R.string.nfc_unknown_tag))
            return
        }

        if (!speakers.hasPermission()) {
            bluetoothPermission.launch(speakers.requiredPermissions())
            return
        }

        val name = target.name ?: speakers.deviceName(target.address) ?: target.address

        if (authenticated) {
            bottomNav.selectedItemId = R.id.nav_speakers
        }

        connectNearby(target.address, name)
    }

    private fun applyPartyMode() {
        if (partyMode) {
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
            resumeOnReconnect = false
        }
    }

    private fun deviceIcon(type: String): Int = when (type.lowercase()) {
        "computer" -> R.drawable.ic_computer
        "smartphone", "tablet" -> R.drawable.ic_phone
        else -> R.drawable.ic_speaker
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
                message?.let(::toast)
            }

            if (refresh) {
                mainHandler.postDelayed({ refreshPlayback() }, 500)
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
            ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) !=
            android.content.pm.PackageManager.PERMISSION_GRANTED
        ) {
            notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
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

    private fun formatTime(milliseconds: Int): String {
        val totalSeconds = milliseconds / 1000
        return "%d:%02d".format(totalSeconds / 60, totalSeconds % 60)
    }

    companion object {
        private const val POLL_INTERVAL_MS = 3000L
        private const val TICK_INTERVAL_MS = 500L
        private const val RECONNECT_RESUME_DELAY_MS = 1500L
        private const val PROGRESS_SCALE = 1000f
        private const val SPOTIFY_PACKAGE = "com.spotify.music"
        private const val PROXIMITY_INTERVAL_MS = 30000L
        private const val NFC_REQUEST = 20
    }
}
