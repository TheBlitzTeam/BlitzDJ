#include "AudioOutputs.hpp"

#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <propidl.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#include <CoreAudio/CoreAudio.h>
#define BLITZDJ_CORE_AUDIO 1
#endif
#endif

namespace blitzdj
{
    namespace
    {
#if defined(_WIN32)
        std::string lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            return value;
        }

        template <typename T>
        class ComPtr
        {
        public:
            ComPtr() = default;

            ~ComPtr()
            {
                reset();
            }

            ComPtr(const ComPtr&) = delete;
            ComPtr& operator=(const ComPtr&) = delete;

            T** put()
            {
                reset();
                return &pointer_;
            }

            T* operator->() const
            {
                return pointer_;
            }

            T* get() const
            {
                return pointer_;
            }

            explicit operator bool() const
            {
                return pointer_ != nullptr;
            }

            void reset()
            {
                if (pointer_)
                {
                    pointer_->Release();
                    pointer_ = nullptr;
                }
            }

        private:
            T* pointer_ = nullptr;
        };

        const PROPERTYKEY friendlyNameKey = {
            {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14
        };

        const PROPERTYKEY enumeratorNameKey = {
            {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 24
        };

        std::string narrow(const wchar_t* value)
        {
            if (!value || !*value)
            {
                return {};
            }

            const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);

            if (size <= 1)
            {
                return {};
            }

            std::string output(static_cast<std::size_t>(size - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, value, -1, output.data(), size, nullptr, nullptr);
            return output;
        }

        std::string stringProperty(IPropertyStore* store, const PROPERTYKEY& key)
        {
            PROPVARIANT value;
            PropVariantInit(&value);

            std::string result;

            if (SUCCEEDED(store->GetValue(key, &value)) && value.vt == VT_LPWSTR)
            {
                result = narrow(value.pwszVal);
            }

            PropVariantClear(&value);
            return result;
        }

        bool topologyIsBluetooth(IMMDevice* device)
        {
            ComPtr<IDeviceTopology> topology;

            if (FAILED(device->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(topology.put()))))
            {
                return false;
            }

            ComPtr<IConnector> connector;

            if (FAILED(topology->GetConnector(0, connector.put())))
            {
                return false;
            }

            ComPtr<IConnector> connected;

            if (FAILED(connector->GetConnectedTo(connected.put())))
            {
                return false;
            }

            ComPtr<IPart> part;

            if (FAILED(connected->QueryInterface(__uuidof(IPart), reinterpret_cast<void**>(part.put()))))
            {
                return false;
            }

            ComPtr<IDeviceTopology> filter;

#if defined(__MINGW32__)
            if (FAILED(part->GetTopologyObjects(filter.put())))
#else
            if (FAILED(part->GetTopologyObject(filter.put())))
#endif
            {
                return false;
            }

            LPWSTR id = nullptr;

            if (FAILED(filter->GetDeviceId(&id)))
            {
                return false;
            }

            const std::string value = lower(narrow(id));
            CoTaskMemFree(id);

            return value.find("bth") != std::string::npos;
        }
#endif

#if defined(BLITZDJ_CORE_AUDIO)
        std::string fromCFString(CFStringRef value)
        {
            if (!value)
            {
                return {};
            }

            const CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8) + 1;
            std::string output(static_cast<std::size_t>(length), '\0');

            if (!CFStringGetCString(value, output.data(), length, kCFStringEncodingUTF8))
            {
                return {};
            }

            output.resize(std::char_traits<char>::length(output.c_str()));
            return output;
        }

        std::string stringProperty(AudioObjectID object, AudioObjectPropertySelector selector)
        {
            AudioObjectPropertyAddress address = {selector, kAudioObjectPropertyScopeGlobal, 0};
            CFStringRef value = nullptr;
            UInt32 size = sizeof(value);

            if (AudioObjectGetPropertyData(object, &address, 0, nullptr, &size, &value) != noErr || !value)
            {
                return {};
            }

            std::string result = fromCFString(value);
            CFRelease(value);
            return result;
        }
#endif
    }

    bool audioOutputsSupported()
    {
#if defined(_WIN32) || defined(BLITZDJ_CORE_AUDIO)
        return true;
#else
        return false;
#endif
    }

    std::vector<AudioOutputDevice> getAudioOutputDevices()
    {
        std::vector<AudioOutputDevice> devices;

#if defined(_WIN32)
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        {
            ComPtr<IMMDeviceEnumerator> enumerator;

            if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                    __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.put()))))
            {
                std::string defaultId;
                ComPtr<IMMDevice> defaultDevice;

                if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, defaultDevice.put())))
                {
                    LPWSTR id = nullptr;

                    if (SUCCEEDED(defaultDevice->GetId(&id)))
                    {
                        defaultId = narrow(id);
                        CoTaskMemFree(id);
                    }
                }

                ComPtr<IMMDeviceCollection> collection;

                if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection.put())))
                {
                    UINT count = 0;
                    collection->GetCount(&count);

                    for (UINT index = 0; index < count; ++index)
                    {
                        ComPtr<IMMDevice> device;

                        if (FAILED(collection->Item(index, device.put())))
                        {
                            continue;
                        }

                        AudioOutputDevice output;
                        LPWSTR id = nullptr;

                        if (SUCCEEDED(device->GetId(&id)))
                        {
                            output.id = narrow(id);
                            CoTaskMemFree(id);
                        }

                        output.isDefault = !defaultId.empty() && output.id == defaultId;

                        ComPtr<IPropertyStore> store;

                        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, store.put())))
                        {
                            output.name = stringProperty(store.get(), friendlyNameKey);
                            output.bluetooth = lower(stringProperty(store.get(), enumeratorNameKey)).find("bth") != std::string::npos;
                        }

                        if (!output.bluetooth)
                        {
                            output.bluetooth = topologyIsBluetooth(device.get());
                        }

                        devices.push_back(std::move(output));
                    }
                }
            }
        }

        if (SUCCEEDED(initialized))
        {
            CoUninitialize();
        }
#elif defined(BLITZDJ_CORE_AUDIO)
        AudioObjectPropertyAddress address = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, 0};
        UInt32 size = 0;

        if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr || size == 0)
        {
            return devices;
        }

        std::vector<AudioObjectID> ids(size / sizeof(AudioObjectID));

        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, ids.data()) != noErr)
        {
            return devices;
        }

        AudioObjectID defaultOutput = kAudioObjectUnknown;
        AudioObjectPropertyAddress defaultAddress = {kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, 0};
        UInt32 defaultSize = sizeof(defaultOutput);
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultAddress, 0, nullptr, &defaultSize, &defaultOutput);

        for (const AudioObjectID id : ids)
        {
            AudioObjectPropertyAddress streams = {kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput, 0};
            UInt32 streamSize = 0;

            if (AudioObjectGetPropertyDataSize(id, &streams, 0, nullptr, &streamSize) != noErr || streamSize == 0)
            {
                continue;
            }

            UInt32 transport = 0;
            UInt32 transportSize = sizeof(transport);
            AudioObjectPropertyAddress transportAddress = {kAudioDevicePropertyTransportType, kAudioObjectPropertyScopeGlobal, 0};
            AudioObjectGetPropertyData(id, &transportAddress, 0, nullptr, &transportSize, &transport);

            AudioOutputDevice output;
            output.id = stringProperty(id, kAudioDevicePropertyDeviceUID);
            output.name = stringProperty(id, kAudioObjectPropertyName);
            output.bluetooth = transport == kAudioDeviceTransportTypeBluetooth || transport == kAudioDeviceTransportTypeBluetoothLE;
            output.isDefault = id == defaultOutput;

            devices.push_back(std::move(output));
        }
#endif

        return devices;
    }
}
