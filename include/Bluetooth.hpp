#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace blitzdj
{
    enum class BluetoothDeviceType
    {
        Unknown,
        Headphones,
        Headset,
        Speaker,
        Computer,
        Smartphone,
        Tablet,
        Other
    };

    enum class BluetoothConnectionState
    {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
        Failed
    };

    enum class BluetoothError
    {
        None,
        NotInitialized,
        NotSupported,
        AdapterUnavailable,
        PermissionDenied,
        DeviceNotFound,
        ConnectionFailed,
        Timeout,
        Unknown
    };

    struct BluetoothDevice
    {
        std::string id;
        std::string name;
        std::string address;

        BluetoothDeviceType type =
            BluetoothDeviceType::Unknown;

        BluetoothConnectionState connectionState =
            BluetoothConnectionState::Disconnected;

        bool paired = false;
        bool trusted = false;
        bool connected = false;
        bool audioCapable = false;

        int signalStrength = 0;
        int batteryLevel = -1;
    };

    struct BluetoothAdapter
    {
        std::string id;
        std::string name;
        std::string address;

        bool available = false;
        bool powered = false;
        bool discoverable = false;
        bool scanning = false;
    };

    struct BluetoothConfig
    {
        int scanTimeoutSeconds = 10;
        int connectionTimeoutSeconds = 15;

        bool scanPairedDevices = true;
        bool scanNearbyDevices = true;
        bool audioDevicesOnly = false;
        bool autoReconnect = false;
    };

    class Bluetooth
    {
    public:
        using DeviceCallback =
            std::function<void(const BluetoothDevice&)>;

        using ConnectionCallback =
            std::function<void(
                const BluetoothDevice&,
                BluetoothConnectionState
            )>;

        using ErrorCallback =
            std::function<void(
                BluetoothError,
                const std::string&
            )>;

        explicit Bluetooth(
            BluetoothConfig config = BluetoothConfig{}
        );

        ~Bluetooth();

        Bluetooth(const Bluetooth&) = delete;
        Bluetooth& operator=(const Bluetooth&) = delete;

        bool initialize();
        void shutdown();

        bool isInitialized() const;
        bool isSupported() const;

        void setConfig(const BluetoothConfig& config);
        const BluetoothConfig& getConfig() const;

        // Adapter management
        std::vector<BluetoothAdapter> getAdapters();

        std::optional<BluetoothAdapter> getDefaultAdapter();

        bool setAdapterPowered(bool enabled);
        bool setDiscoverable(bool enabled);

        bool isAdapterPowered();

        // Device discovery
        bool startScan();
        void stopScan();

        bool isScanning() const;

        std::vector<BluetoothDevice> getDevices();
        std::vector<BluetoothDevice> getPairedDevices();

        std::vector<BluetoothDevice> getAudioDevices();

        std::optional<BluetoothDevice> findDevice(
            const std::string& deviceId
        );

        std::optional<BluetoothDevice> findDeviceByAddress(
            const std::string& address
        );

        // Pairing
        bool pairDevice(const std::string& deviceId);
        bool unpairDevice(const std::string& deviceId);

        bool trustDevice(
            const std::string& deviceId,
            bool trusted = true
        );

        // Connection
        bool connectDevice(const std::string& deviceId);
        bool disconnectDevice(const std::string& deviceId);

        bool reconnectDevice(const std::string& deviceId);

        bool isDeviceConnected(
            const std::string& deviceId
        );

        std::vector<BluetoothDevice> getConnectedDevices();

        // Audio devices
        bool connectAudioDevice(
            const std::string& deviceId
        );

        bool disconnectAudioDevice(
            const std::string& deviceId
        );

        bool setDefaultAudioDevice(
            const std::string& deviceId
        );

        std::optional<BluetoothDevice> getDefaultAudioDevice();

        // Device information
        int getSignalStrength(const std::string& deviceId);

        int getBatteryLevel(const std::string& deviceId);

        // Callbacks
        void setDeviceDiscoveredCallback(
            DeviceCallback callback
        );

        void setConnectionCallback(
            ConnectionCallback callback
        );

        void setErrorCallback(
            ErrorCallback callback
        );

        // Error handling
        BluetoothError getLastError() const;

        const std::string& getLastErrorMessage() const;

        void clearError();

        // Utilities
        static std::string deviceTypeToString(
            BluetoothDeviceType type
        );

        static std::string connectionStateToString(
            BluetoothConnectionState state
        );

        static std::string errorToString(
            BluetoothError error
        );

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;

        BluetoothConfig config_;

        bool initialized_ = false;
        bool scanning_ = false;

        BluetoothError lastError_ =
            BluetoothError::None;

        std::string lastErrorMessage_;

        DeviceCallback deviceCallback_;
        ConnectionCallback connectionCallback_;
        ErrorCallback errorCallback_;

        void setError(
            BluetoothError error,
            const std::string& message
        );
    };
}
