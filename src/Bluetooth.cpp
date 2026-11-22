#include "Bluetooth.hpp"

#include <algorithm>
#include <utility>

#ifdef __linux__
#include <gio/gio.h>
#endif

namespace blitzdj {
struct Bluetooth::Impl {
#ifdef __linux__
    GDBusConnection* bus = nullptr;
#endif
};

Bluetooth::Bluetooth(BluetoothConfig config) : impl_(new Impl), config_(std::move(config)) {}
Bluetooth::~Bluetooth() { shutdown(); }

void Bluetooth::setError(BluetoothError error, const std::string& message) {
    lastError_ = error;
    lastErrorMessage_ = message;
    if (errorCallback_) errorCallback_(error, message);
}

#ifdef __linux__
namespace {
using DeviceList = std::vector<BluetoothDevice>;
struct Snapshot { std::vector<BluetoothAdapter> adapters; DeviceList devices; };

GVariant* call(GDBusConnection* bus, const char* path, const char* iface,
               const char* method, GVariant* params, const GVariantType* resultType,
               GError** error) {
    return g_dbus_connection_call_sync(bus, "org.bluez", path, iface, method,
        params, resultType, G_DBUS_CALL_FLAGS_NONE, 15000, nullptr, error);
}

bool propertyBool(GVariant* props, const char* name, bool fallback = false) {
    GVariant* value = g_variant_lookup_value(props, name, G_VARIANT_TYPE_BOOLEAN);
    if (!value) return fallback;
    bool result = g_variant_get_boolean(value);
    g_variant_unref(value);
    return result;
}

std::string propertyString(GVariant* props, const char* name) {
    GVariant* value = g_variant_lookup_value(props, name, G_VARIANT_TYPE_STRING);
    if (!value) return {};
    std::string result = g_variant_get_string(value, nullptr);
    g_variant_unref(value);
    return result;
}

int propertyInt(GVariant* props, const char* name, int fallback) {
    GVariant* value = g_variant_lookup_value(props, name, nullptr);
    if (!value) return fallback;
    int result = fallback;
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT16)) result = g_variant_get_int16(value);
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_BYTE)) result = g_variant_get_byte(value);
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT16)) result = g_variant_get_uint16(value);
    g_variant_unref(value);
    return result;
}

Snapshot snapshot(GDBusConnection* bus, std::string& errorMessage) {
    Snapshot result;
    GError* error = nullptr;
    GVariant* reply = call(bus, "/", "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects", nullptr, G_VARIANT_TYPE("(a{oa{sa{sv}}})"), &error);
    if (!reply) {
        errorMessage = error ? error->message : "BlueZ unavailable";
        if (error) g_error_free(error);
        return result;
    }
    GVariant* objects = g_variant_get_child_value(reply, 0);
    GVariantIter iter;
    g_variant_iter_init(&iter, objects);
    const gchar* path = nullptr;
    GVariant* interfaces = nullptr;
    while (g_variant_iter_next(&iter, "{&o@a{sa{sv}}}", &path, &interfaces)) {
        GVariant* adapter = g_variant_lookup_value(interfaces, "org.bluez.Adapter1", G_VARIANT_TYPE("a{sv}"));
        if (adapter) {
            BluetoothAdapter a;
            a.id = path;
            a.name = propertyString(adapter, "Name");
            a.address = propertyString(adapter, "Address");
            a.available = true;
            a.powered = propertyBool(adapter, "Powered");
            a.discoverable = propertyBool(adapter, "Discoverable");
            a.scanning = propertyBool(adapter, "Discovering");
            result.adapters.push_back(std::move(a));
            g_variant_unref(adapter);
        }
        GVariant* device = g_variant_lookup_value(interfaces, "org.bluez.Device1", G_VARIANT_TYPE("a{sv}"));
        if (device) {
            BluetoothDevice d;
            d.id = path;
            d.name = propertyString(device, "Alias");
            if (d.name.empty()) d.name = propertyString(device, "Name");
            d.address = propertyString(device, "Address");
            d.paired = propertyBool(device, "Paired");
            d.trusted = propertyBool(device, "Trusted");
            d.connected = propertyBool(device, "Connected");
            d.connectionState = d.connected ? BluetoothConnectionState::Connected : BluetoothConnectionState::Disconnected;
            d.signalStrength = propertyInt(device, "RSSI", 0);
            GVariant* uuids = g_variant_lookup_value(device, "UUIDs", G_VARIANT_TYPE("as"));
            if (uuids) {
                GVariantIter uuidIter;
                g_variant_iter_init(&uuidIter, uuids);
                const gchar* uuid;
                while (g_variant_iter_next(&uuidIter, "&s", &uuid)) {
                    std::string s(uuid);
                    if (s.find("110b-") != std::string::npos || s.find("110a-") != std::string::npos ||
                        s.find("1108-") != std::string::npos || s.find("111e-") != std::string::npos) d.audioCapable = true;
                }
                g_variant_unref(uuids);
            }
            result.devices.push_back(std::move(d));
            g_variant_unref(device);
        }
        g_variant_unref(interfaces);
    }
    g_variant_unref(objects);
    g_variant_unref(reply);
    return result;
}

bool method(GDBusConnection* bus, const std::string& path, const char* iface,
            const char* name, std::string& errorMessage) {
    GError* error = nullptr;
    GVariant* reply = call(bus, path.c_str(), iface, name, nullptr, nullptr, &error);
    if (!reply) {
        errorMessage = error ? error->message : "D-Bus operation failed";
        if (error) g_error_free(error);
        return false;
    }
    g_variant_unref(reply);
    return true;
}

bool setProperty(GDBusConnection* bus, const std::string& path, const char* iface,
                 const char* property, bool value, std::string& errorMessage) {
    GError* error = nullptr;
    GVariant* reply = call(bus, path.c_str(), "org.freedesktop.DBus.Properties", "Set",
        g_variant_new("(ssv)", iface, property, g_variant_new_boolean(value)), nullptr, &error);
    if (!reply) {
        errorMessage = error ? error->message : "Property update failed";
        if (error) g_error_free(error);
        return false;
    }
    g_variant_unref(reply);
    return true;
}
}
#endif

bool Bluetooth::initialize() {
    if (initialized_) return true;
#ifdef __linux__
    GError* error = nullptr;
    impl_->bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (!impl_->bus) {
        setError(BluetoothError::AdapterUnavailable, error ? error->message : "Cannot access system bus");
        if (error) g_error_free(error);
        return false;
    }
    std::string message;
    auto data = snapshot(impl_->bus, message);
    if (!message.empty() || data.adapters.empty()) {
        setError(BluetoothError::AdapterUnavailable, message.empty() ? "No BlueZ adapter found" : message);
        shutdown();
        return false;
    }
    initialized_ = true;
    clearError();
    return true;
#else
    setError(BluetoothError::NotSupported, "Bluetooth backend is not implemented on this platform");
    return false;
#endif
}

void Bluetooth::shutdown() {
#ifdef __linux__
    if (impl_->bus) { g_object_unref(impl_->bus); impl_->bus = nullptr; }
#endif
    initialized_ = false;
    scanning_ = false;
}
bool Bluetooth::isInitialized() const { return initialized_; }
bool Bluetooth::isSupported() const {
#ifdef __linux__
    return true;
#else
    return false;
#endif
}
void Bluetooth::setConfig(const BluetoothConfig& config) { config_ = config; }
const BluetoothConfig& Bluetooth::getConfig() const { return config_; }

std::vector<BluetoothAdapter> Bluetooth::getAdapters() {
#ifdef __linux__
    if (!initialized_) { setError(BluetoothError::NotInitialized, "Bluetooth is not initialized"); return {}; }
    std::string error;
    auto result = snapshot(impl_->bus, error);
    if (!error.empty()) setError(BluetoothError::Unknown, error);
    return result.adapters;
#else
    setError(BluetoothError::NotSupported, "Unsupported platform"); return {};
#endif
}
std::optional<BluetoothAdapter> Bluetooth::getDefaultAdapter() {
    auto adapters = getAdapters();
    if (adapters.empty()) return std::nullopt;
    return adapters.front();
}
bool Bluetooth::setAdapterPowered(bool enabled) {
#ifdef __linux__
    auto adapter = getDefaultAdapter();
    if (!adapter) return false;
    std::string error;
    if (setProperty(impl_->bus, adapter->id, "org.bluez.Adapter1", "Powered", enabled, error)) return true;
    setError(BluetoothError::PermissionDenied, error); return false;
#else
    (void)enabled; setError(BluetoothError::NotSupported, "Unsupported platform"); return false;
#endif
}
bool Bluetooth::setDiscoverable(bool enabled) {
#ifdef __linux__
    auto adapter = getDefaultAdapter();
    if (!adapter) return false;
    std::string error;
    if (setProperty(impl_->bus, adapter->id, "org.bluez.Adapter1", "Discoverable", enabled, error)) return true;
    setError(BluetoothError::PermissionDenied, error); return false;
#else
    (void)enabled; setError(BluetoothError::NotSupported, "Unsupported platform"); return false;
#endif
}
bool Bluetooth::isAdapterPowered() { auto adapter = getDefaultAdapter(); return adapter && adapter->powered; }

bool Bluetooth::startScan() {
#ifdef __linux__
    auto adapter = getDefaultAdapter();
    if (!adapter) return false;
    std::string error;
    if (!method(impl_->bus, adapter->id, "org.bluez.Adapter1", "StartDiscovery", error)) {
        setError(BluetoothError::Unknown, error); return false;
    }
    scanning_ = true;
    return true;
#else
    setError(BluetoothError::NotSupported, "Unsupported platform"); return false;
#endif
}
void Bluetooth::stopScan() {
#ifdef __linux__
    if (initialized_ && scanning_) {
        auto adapter = getDefaultAdapter();
        if (adapter) {
            std::string error;
            if (!method(impl_->bus, adapter->id, "org.bluez.Adapter1", "StopDiscovery", error))
                setError(BluetoothError::Unknown, error);
        }
    }
#endif
    scanning_ = false;
}
bool Bluetooth::isScanning() const { return scanning_; }
std::vector<BluetoothDevice> Bluetooth::getDevices() {
#ifdef __linux__
    if (!initialized_) { setError(BluetoothError::NotInitialized, "Bluetooth is not initialized"); return {}; }
    std::string error;
    auto data = snapshot(impl_->bus, error);
    if (!error.empty()) setError(BluetoothError::Unknown, error);
    for (const auto& device : data.devices) if (deviceCallback_) deviceCallback_(device);
    return data.devices;
#else
    setError(BluetoothError::NotSupported, "Unsupported platform"); return {};
#endif
}
std::vector<BluetoothDevice> Bluetooth::getPairedDevices() {
    auto devices = getDevices();
    devices.erase(std::remove_if(devices.begin(), devices.end(), [](const auto& d) { return !d.paired; }), devices.end());
    return devices;
}
std::vector<BluetoothDevice> Bluetooth::getAudioDevices() {
    auto devices = getDevices();
    devices.erase(std::remove_if(devices.begin(), devices.end(), [](const auto& d) { return !d.audioCapable; }), devices.end());
    return devices;
}
std::optional<BluetoothDevice> Bluetooth::findDevice(const std::string& id) {
    for (auto& device : getDevices()) if (device.id == id) return device;
    setError(BluetoothError::DeviceNotFound, "Device not found"); return std::nullopt;
}
std::optional<BluetoothDevice> Bluetooth::findDeviceByAddress(const std::string& address) {
    for (auto& device : getDevices()) if (device.address == address) return device;
    setError(BluetoothError::DeviceNotFound, "Device not found"); return std::nullopt;
}
bool Bluetooth::pairDevice(const std::string& id) {
#ifdef __linux__
    if (!findDevice(id)) return false;
    std::string error;
    if (method(impl_->bus, id, "org.bluez.Device1", "Pair", error)) return true;
    setError(BluetoothError::ConnectionFailed, error); return false;
#else
    (void)id; setError(BluetoothError::NotSupported, "Unsupported platform"); return false;
#endif
}
bool Bluetooth::unpairDevice(const std::string& id) {
#ifdef __linux__
    if (!findDevice(id)) return false;
    auto adapter = getDefaultAdapter();
    if (!adapter) return false;
    GError* error = nullptr;
    GVariant* reply = call(impl_->bus, adapter->id.c_str(), "org.bluez.Adapter1", "RemoveDevice",
        g_variant_new("(o)", id.c_str()), nullptr, &error);
    if (!reply) {
        setError(BluetoothError::ConnectionFailed, error ? error->message : "RemoveDevice failed");
        if (error) g_error_free(error);
        return false;
    }
    g_variant_unref(reply); return true;
#else
    (void)id; setError(BluetoothError::NotSupported, "Unsupported platform"); return false;
#endif
}
bool Bluetooth::trustDevice(const std::string& id, bool trusted) {
#ifdef __linux__
    if (!findDevice(id)) return false;
    std::string error;
    if (setProperty(impl_->bus, id, "org.bluez.Device1", "Trusted", trusted, error)) return true;
    setError(BluetoothError::PermissionDenied, error); return false;
#else
    (void)id; (void)trusted; setError(BluetoothError::NotSupported, "Unsupported platform"); return false;
#endif
}
bool Bluetooth::connectDevice(const std::string& id) {
#ifdef __linux__
    auto device = findDevice(id);
    if (!device) return false;
    if (device->connected) return true;
    if (connectionCallback_) connectionCallback_(*device, BluetoothConnectionState::Connecting);
    std::string error;
    if (method(impl_->bus, id, "org.bluez.Device1", "Connect", error)) {
        device->connected = true;
        if (connectionCallback_) connectionCallback_(*device, BluetoothConnectionState::Connected);
        return true;
    }
    setError(BluetoothError::ConnectionFailed, error);
    if (connectionCallback_) connectionCallback_(*device, BluetoothConnectionState::Failed);
    return false;
#else
    (void)id; setError(BluetoothError::NotSupported, "Unsupported platform"); return false;
#endif
}
bool Bluetooth::disconnectDevice(const std::string& id) {
#ifdef __linux__
    auto device = findDevice(id);
    if (!device) return false;
    if (!device->connected) return true;
    if (connectionCallback_) connectionCallback_(*device, BluetoothConnectionState::Disconnecting);
    std::string error;
    if (method(impl_->bus, id, "org.bluez.Device1", "Disconnect", error)) {
        device->connected = false;
        if (connectionCallback_) connectionCallback_(*device, BluetoothConnectionState::Disconnected);
        return true;
    }
    setError(BluetoothError::ConnectionFailed, error); return false;
#else
    (void)id; setError(BluetoothError::NotSupported, "Unsupported platform"); return false;
#endif
}
bool Bluetooth::reconnectDevice(const std::string& id) { return connectDevice(id); }
bool Bluetooth::isDeviceConnected(const std::string& id) { auto device = findDevice(id); return device && device->connected; }
std::vector<BluetoothDevice> Bluetooth::getConnectedDevices() {
    auto devices = getDevices();
    devices.erase(std::remove_if(devices.begin(), devices.end(), [](const auto& d) { return !d.connected; }), devices.end());
    return devices;
}
bool Bluetooth::connectAudioDevice(const std::string& id) {
    auto device = findDevice(id);
    if (!device) return false;
    if (!device->audioCapable) { setError(BluetoothError::NotSupported, "Device does not advertise a supported audio profile"); return false; }
    return connectDevice(id);
}
bool Bluetooth::disconnectAudioDevice(const std::string& id) { return disconnectDevice(id); }
bool Bluetooth::setDefaultAudioDevice(const std::string& id) {
    (void)id;
    setError(BluetoothError::NotSupported, "Audio routing requires a PipeWire or PulseAudio backend");
    return false;
}
std::optional<BluetoothDevice> Bluetooth::getDefaultAudioDevice() {
    setError(BluetoothError::NotSupported, "Audio routing requires a PipeWire or PulseAudio backend");
    return std::nullopt;
}
int Bluetooth::getSignalStrength(const std::string& id) { auto d = findDevice(id); return d ? d->signalStrength : 0; }
int Bluetooth::getBatteryLevel(const std::string& id) {
    auto d = findDevice(id);
    if (!d) return -1;
#ifdef __linux__
    std::string error;
    auto data = snapshot(impl_->bus, error);
    (void)data;
    GError* dbusError = nullptr;
    GVariant* reply = call(impl_->bus, id.c_str(), "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", "org.bluez.Battery1", "Percentage"), G_VARIANT_TYPE("(v)"), &dbusError);
    if (!reply) { if (dbusError) g_error_free(dbusError); return -1; }
    GVariant* value = g_variant_get_child_value(reply, 0);
    GVariant* inner = g_variant_get_variant(value);
    int percentage = g_variant_is_of_type(inner, G_VARIANT_TYPE_BYTE) ? g_variant_get_byte(inner) : -1;
    g_variant_unref(inner); g_variant_unref(value); g_variant_unref(reply);
    return percentage;
#else
    return -1;
#endif
}
void Bluetooth::setDeviceDiscoveredCallback(DeviceCallback cb) { deviceCallback_ = std::move(cb); }
void Bluetooth::setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
void Bluetooth::setErrorCallback(ErrorCallback cb) { errorCallback_ = std::move(cb); }
BluetoothError Bluetooth::getLastError() const { return lastError_; }
const std::string& Bluetooth::getLastErrorMessage() const { return lastErrorMessage_; }
void Bluetooth::clearError() { lastError_ = BluetoothError::None; lastErrorMessage_.clear(); }
std::string Bluetooth::deviceTypeToString(BluetoothDeviceType type) {
    switch (type) {
        case BluetoothDeviceType::Headphones: return "Headphones";
        case BluetoothDeviceType::Headset: return "Headset";
        case BluetoothDeviceType::Speaker: return "Speaker";
        case BluetoothDeviceType::Computer: return "Computer";
        case BluetoothDeviceType::Smartphone: return "Smartphone";
        case BluetoothDeviceType::Tablet: return "Tablet";
        case BluetoothDeviceType::Other: return "Other";
        default: return "Unknown";
    }
}
std::string Bluetooth::connectionStateToString(BluetoothConnectionState state) {
    switch (state) {
        case BluetoothConnectionState::Disconnected: return "Disconnected";
        case BluetoothConnectionState::Connecting: return "Connecting";
        case BluetoothConnectionState::Connected: return "Connected";
        case BluetoothConnectionState::Disconnecting: return "Disconnecting";
        case BluetoothConnectionState::Failed: return "Failed";
    }
    return "Unknown";
}
std::string Bluetooth::errorToString(BluetoothError error) {
    switch (error) {
        case BluetoothError::None: return "None";
        case BluetoothError::NotInitialized: return "Not initialized";
        case BluetoothError::NotSupported: return "Not supported";
        case BluetoothError::AdapterUnavailable: return "Adapter unavailable";
        case BluetoothError::PermissionDenied: return "Permission denied";
        case BluetoothError::DeviceNotFound: return "Device not found";
        case BluetoothError::ConnectionFailed: return "Connection failed";
        case BluetoothError::Timeout: return "Timeout";
        default: return "Unknown";
    }
}
}
