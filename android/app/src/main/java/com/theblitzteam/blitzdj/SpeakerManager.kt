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

class SpeakerManager(context: Context, private val onChanged: () -> Unit) {

    enum class OutputType { PHONE, BLUETOOTH, WIRED, USB }

    data class Output(val type: OutputType, val name: String?)

    data class Speaker(
        val name: String,
        val address: String,
        val connected: Boolean,
        val device: BluetoothDevice
    )

    private val appContext = context.applicationContext
    private val handler = Handler(Looper.getMainLooper())
    private val audioManager = appContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private val adapter: BluetoothAdapter? =
        (appContext.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter

    private var a2dp: BluetoothA2dp? = null
    private var started = false

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
            if (intent?.action == BluetoothAdapter.ACTION_STATE_CHANGED) {
                connectProfile()
            }

            onChanged()
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
        }

        ContextCompat.registerReceiver(appContext, receiver, filter, ContextCompat.RECEIVER_NOT_EXPORTED)
        connectProfile()
    }

    fun stop() {
        if (!started) {
            return
        }

        started = false
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
        Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
            ContextCompat.checkSelfPermission(appContext, Manifest.permission.BLUETOOTH_CONNECT) ==
            PackageManager.PERMISSION_GRANTED

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
            .filter(::isAudioDevice)
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

    fun connect(speaker: Speaker): Boolean = invokeHidden("connect", speaker.device)

    fun disconnect(speaker: Speaker): Boolean = invokeHidden("disconnect", speaker.device)

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
    private fun isAudioDevice(device: BluetoothDevice): Boolean {
        val bluetoothClass = runCatching { device.bluetoothClass }.getOrNull() ?: return true

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

    private companion object {
        val BLUETOOTH_TYPES = setOf(
            AudioDeviceInfo.TYPE_BLUETOOTH_A2DP,
            26,
            27,
            30
        )

        val WIRED_TYPES = setOf(
            AudioDeviceInfo.TYPE_WIRED_HEADPHONES,
            AudioDeviceInfo.TYPE_WIRED_HEADSET,
            AudioDeviceInfo.TYPE_LINE_ANALOG,
            AudioDeviceInfo.TYPE_AUX_LINE
        )

        val USB_TYPES = setOf(
            AudioDeviceInfo.TYPE_USB_DEVICE,
            AudioDeviceInfo.TYPE_USB_HEADSET,
            AudioDeviceInfo.TYPE_USB_ACCESSORY
        )
    }
}
