package com.theblitzteam.blitzdj

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothA2dp
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothClass
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import androidx.core.content.ContextCompat
import androidx.core.content.IntentCompat

class SpeakerManager(context: Context, private val onChanged: () -> Unit) {

    enum class OutputType { PHONE, BLUETOOTH, WIRED, USB }

    enum class Proximity { VERY_CLOSE, NEAR, IN_RANGE, FAR }

    data class Output(val type: OutputType, val name: String?)

    data class Speaker(
        val name: String,
        val address: String,
        val connected: Boolean,
        val device: BluetoothDevice
    )

    data class Nearby(
        val name: String,
        val address: String,
        val rssi: Int,
        val bonded: Boolean,
        val device: BluetoothDevice
    ) {
        val proximity: Proximity
            get() = proximityOf(rssi)
    }

    var onNearbyChanged: (() -> Unit)? = null
    var onConnecting: ((name: String, success: Boolean) -> Unit)? = null

    var autoConnect = false

    var isDiscovering = false
        private set

    private val appContext = context.applicationContext
    private val handler = Handler(Looper.getMainLooper())
    private val audioManager = appContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private val adapter: BluetoothAdapter? =
        (appContext.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter

    private var a2dp: BluetoothA2dp? = null
    private var started = false
    private var pendingConnect: String? = null
    private val nearby = LinkedHashMap<String, Nearby>()

    private val deviceCallback = object : AudioDeviceCallback() {
        override fun onAudioDevicesAdded(addedDevices: Array<out AudioDeviceInfo>?) {
            onChanged()
        }

        override fun onAudioDevicesRemoved(removedDevices: Array<out AudioDeviceInfo>?) {
            onChanged()
        }
    }

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                BluetoothAdapter.ACTION_STATE_CHANGED -> {
                    connectProfile()
                    onChanged()
                }

                BluetoothDevice.ACTION_FOUND -> handleFound(intent)

                BluetoothAdapter.ACTION_DISCOVERY_STARTED -> {
                    isDiscovering = true
                    onNearbyChanged?.invoke()
                }

                BluetoothAdapter.ACTION_DISCOVERY_FINISHED -> {
                    isDiscovering = false
                    onNearbyChanged?.invoke()
                }

                BluetoothDevice.ACTION_BOND_STATE_CHANGED -> {
                    handleBondChanged(intent)
                    onChanged()
                }

                else -> onChanged()
            }
        }
    }

    private val profileListener = object : BluetoothProfile.ServiceListener {
        override fun onServiceConnected(profile: Int, proxy: BluetoothProfile?) {
            if (profile == BluetoothProfile.A2DP) {
                a2dp = proxy as? BluetoothA2dp
                onChanged()
            }
        }

        override fun onServiceDisconnected(profile: Int) {
            if (profile == BluetoothProfile.A2DP) {
                a2dp = null
            }
        }
    }

    val isBluetoothAvailable: Boolean
        get() = adapter != null

    fun start() {
        if (started) {
            return
        }

        started = true
        audioManager.registerAudioDeviceCallback(deviceCallback, handler)

        val filter = IntentFilter().apply {
            addAction(BluetoothAdapter.ACTION_STATE_CHANGED)
            addAction(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED)
            addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
            addAction(BluetoothDevice.ACTION_FOUND)
            addAction(BluetoothAdapter.ACTION_DISCOVERY_STARTED)
            addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED)
        }

        ContextCompat.registerReceiver(appContext, receiver, filter, ContextCompat.RECEIVER_NOT_EXPORTED)
        connectProfile()
    }

    fun stop() {
        if (!started) {
            return
        }

        started = false
        stopDiscovery()
        audioManager.unregisterAudioDeviceCallback(deviceCallback)
        runCatching { appContext.unregisterReceiver(receiver) }

        a2dp?.let { proxy -> runCatching { adapter?.closeProfileProxy(BluetoothProfile.A2DP, proxy) } }
        a2dp = null
    }

    fun connectProfile() {
        if (a2dp != null || !hasPermission() || !isBluetoothEnabled()) {
            return
        }

        runCatching { adapter?.getProfileProxy(appContext, profileListener, BluetoothProfile.A2DP) }
    }

    fun hasPermission(): Boolean =
        Build.VERSION.SDK_INT < Build.VERSION_CODES.S || granted(Manifest.permission.BLUETOOTH_CONNECT)

    fun hasScanPermission(): Boolean =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            granted(Manifest.permission.BLUETOOTH_SCAN) && granted(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            granted(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    fun requiredPermissions(): Array<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.BLUETOOTH_SCAN)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    @SuppressLint("MissingPermission")
    fun isBluetoothEnabled(): Boolean =
        runCatching { adapter?.isEnabled == true }.getOrDefault(false)

    fun currentOutput(): Output {
        val outputs = audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)

        outputs.firstOrNull { it.type in BLUETOOTH_TYPES }?.let {
            return Output(OutputType.BLUETOOTH, it.productName?.toString())
        }

        outputs.firstOrNull { it.type in WIRED_TYPES }?.let {
            return Output(OutputType.WIRED, null)
        }

        outputs.firstOrNull { it.type in USB_TYPES }?.let {
            return Output(OutputType.USB, it.productName?.toString())
        }

        return Output(OutputType.PHONE, null)
    }

    @SuppressLint("MissingPermission")
    fun speakers(): List<Speaker> {
        if (!hasPermission() || !isBluetoothEnabled()) {
            return emptyList()
        }

        val bonded = runCatching { adapter?.bondedDevices.orEmpty() }.getOrDefault(emptySet())
        val connected = runCatching { a2dp?.connectedDevices.orEmpty() }
            .getOrDefault(emptyList())
            .map { it.address }
            .toSet()

        return bonded
            .filter { device -> isAudioClass(runCatching { device.bluetoothClass }.getOrNull()) }
            .map { device ->
                Speaker(
                    name = runCatching { device.name }.getOrNull() ?: device.address,
                    address = device.address,
                    connected = device.address in connected,
                    device = device
                )
            }
            .sortedWith(compareByDescending<Speaker> { it.connected }.thenBy { it.name.lowercase() })
    }

    fun nearbySpeakers(): List<Nearby> =
        nearby.values.sortedByDescending { it.rssi }

    @SuppressLint("MissingPermission")
    fun startDiscovery(clear: Boolean = true): Boolean {
        if (!hasScanPermission() || !isBluetoothEnabled()) {
            return false
        }

        if (clear) {
            nearby.clear()
            onNearbyChanged?.invoke()
        }

        return runCatching {
            if (adapter?.isDiscovering == true) {
                true
            } else {
                adapter?.startDiscovery() == true
            }
        }.getOrDefault(false)
    }

    @SuppressLint("MissingPermission")
    fun stopDiscovery() {
        if (hasScanPermission()) {
            runCatching { adapter?.cancelDiscovery() }
        }

        isDiscovering = false
    }

    fun connect(speaker: Speaker): Boolean = invokeHidden("connect", speaker.device)

    fun disconnect(speaker: Speaker): Boolean = invokeHidden("disconnect", speaker.device)

    @SuppressLint("MissingPermission")
    fun pairAndConnect(address: String): Boolean {
        if (!hasPermission()) {
            return false
        }

        val device = runCatching { adapter?.getRemoteDevice(address.uppercase()) }.getOrNull() ?: return false
        stopDiscovery()

        val bonded = runCatching { device.bondState == BluetoothDevice.BOND_BONDED }.getOrDefault(false)

        if (bonded) {
            return invokeHidden("connect", device)
        }

        pendingConnect = device.address
        return runCatching { device.createBond() }.getOrDefault(false)
    }

    @SuppressLint("MissingPermission")
    fun deviceName(address: String): String? =
        runCatching { adapter?.getRemoteDevice(address.uppercase())?.name }.getOrNull()

    fun openBluetoothSettings(activity: Activity) {
        runCatching { activity.startActivity(Intent(Settings.ACTION_BLUETOOTH_SETTINGS)) }
    }

    fun openOutputPicker(activity: Activity) {
        val intent = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            Intent(Settings.Panel.ACTION_VOLUME)
        } else {
            Intent(Settings.ACTION_BLUETOOTH_SETTINGS)
        }

        runCatching { activity.startActivity(intent) }
            .onFailure { openBluetoothSettings(activity) }
    }

    @SuppressLint("MissingPermission")
    private fun handleFound(intent: Intent) {
        val device = IntentCompat.getParcelableExtra(intent, BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java) ?: return
        val rssi = intent.getShortExtra(BluetoothDevice.EXTRA_RSSI, Short.MIN_VALUE).toInt()

        if (rssi == Short.MIN_VALUE.toInt()) {
            return
        }

        val bluetoothClass = IntentCompat.getParcelableExtra(intent, BluetoothDevice.EXTRA_CLASS, BluetoothClass::class.java)
            ?: runCatching { device.bluetoothClass }.getOrNull()

        if (!isAudioClass(bluetoothClass)) {
            return
        }

        val name = intent.getStringExtra(BluetoothDevice.EXTRA_NAME)
            ?: runCatching { device.name }.getOrNull()
            ?: return

        val bonded = runCatching { device.bondState == BluetoothDevice.BOND_BONDED }.getOrDefault(false)

        nearby[device.address] = Nearby(name, device.address, rssi, bonded, device)
        onNearbyChanged?.invoke()

        if (autoConnect && bonded && rssi >= AUTO_CONNECT_RSSI && currentOutput().type != OutputType.BLUETOOTH) {
            stopDiscovery()
            val success = invokeHidden("connect", device)
            onConnecting?.invoke(name, success)
        }
    }

    @SuppressLint("MissingPermission")
    private fun handleBondChanged(intent: Intent) {
        val device = IntentCompat.getParcelableExtra(intent, BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java) ?: return
        val state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.BOND_NONE)

        nearby[device.address]?.let { nearby[device.address] = it.copy(bonded = state == BluetoothDevice.BOND_BONDED) }

        if (device.address != pendingConnect) {
            return
        }

        val name = runCatching { device.name }.getOrNull() ?: device.address

        when (state) {
            BluetoothDevice.BOND_BONDED -> {
                pendingConnect = null
                handler.postDelayed({
                    val success = invokeHidden("connect", device) || currentOutput().type == OutputType.BLUETOOTH
                    onConnecting?.invoke(name, success)
                }, BOND_CONNECT_DELAY_MS)
            }

            BluetoothDevice.BOND_NONE -> {
                pendingConnect = null
                onConnecting?.invoke(name, false)
            }
        }
    }

    private fun granted(permission: String): Boolean =
        ContextCompat.checkSelfPermission(appContext, permission) == PackageManager.PERMISSION_GRANTED

    private fun isAudioClass(bluetoothClass: BluetoothClass?): Boolean {
        if (bluetoothClass == null) {
            return true
        }

        return bluetoothClass.majorDeviceClass == BluetoothClass.Device.Major.AUDIO_VIDEO ||
            bluetoothClass.hasService(BluetoothClass.Service.AUDIO) ||
            bluetoothClass.hasService(BluetoothClass.Service.RENDER)
    }

    private fun invokeHidden(name: String, device: BluetoothDevice): Boolean {
        val proxy = a2dp ?: return false

        return runCatching {
            val method = BluetoothA2dp::class.java.getMethod(name, BluetoothDevice::class.java)
            method.invoke(proxy, device) as? Boolean ?: false
        }.getOrDefault(false)
    }

    companion object {
        const val AUTO_CONNECT_RSSI = -70

        private const val BOND_CONNECT_DELAY_MS = 1200L

        fun proximityOf(rssi: Int): Proximity = when {
            rssi >= -55 -> Proximity.VERY_CLOSE
            rssi >= -70 -> Proximity.NEAR
            rssi >= -85 -> Proximity.IN_RANGE
            else -> Proximity.FAR
        }

        private val BLUETOOTH_TYPES = setOf(
            AudioDeviceInfo.TYPE_BLUETOOTH_A2DP,
            26,
            27,
            30
        )

        private val WIRED_TYPES = setOf(
            AudioDeviceInfo.TYPE_WIRED_HEADPHONES,
            AudioDeviceInfo.TYPE_WIRED_HEADSET,
            AudioDeviceInfo.TYPE_LINE_ANALOG,
            AudioDeviceInfo.TYPE_AUX_LINE
        )

        private val USB_TYPES = setOf(
            AudioDeviceInfo.TYPE_USB_DEVICE,
            AudioDeviceInfo.TYPE_USB_HEADSET,
            AudioDeviceInfo.TYPE_USB_ACCESSORY
        )
    }
}
