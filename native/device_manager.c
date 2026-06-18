#include "include/device_manager.h"
#include "include/common.h"      // R8: wchar_to_utf8, GUIDs WASAPI compartilhados
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>

#define MAX_DEVICES 32

#ifndef MF_E_ALREADY_INITIALIZED
#define MF_E_ALREADY_INITIALIZED ((HRESULT)0xC00D36B0)
#endif

typedef struct {
    char name[256];
    char id[512];
} DeviceInfo;

static DeviceInfo g_video_devices[MAX_DEVICES];
static int g_video_device_count = 0;

static DeviceInfo g_audio_devices[MAX_DEVICES];
static int g_audio_device_count = 0;

static int g_com_initialized = 0;
static int g_mf_initialized = 0;
static int g_backends_initialized = 0;

// R8: wchar_to_utf8 removida — usar a versão inline de include/common.h

extern void init_video_capture_backend();
extern void shutdown_video_capture_backend();
extern void init_audio_capture_backend();
extern void shutdown_audio_capture_backend();

EXPORT int init_devices_backend() {
    // Inicializa as critical sections incondicionalmente (apenas uma vez) para evitar crashes
    // por EnterCriticalSection em funções como set_brightness_offset.
    if (!g_backends_initialized) {
        init_video_capture_backend();
        init_audio_capture_backend();
        g_backends_initialized = 1;
    }

    // CoInitializeEx no main thread (que roda o Dart) deve suportar APARTMENTTHREADED,
    // já que o Flutter runner inicializa COM nesse modo. Também aceita RPC_E_CHANGED_MODE.
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
        g_com_initialized = 1;
    }

    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (SUCCEEDED(hr) || hr == MF_E_ALREADY_INITIALIZED) {
        g_mf_initialized = 1;
    }

    if (g_com_initialized && g_mf_initialized) {
        return 1;
    }
    return 0;
}

EXPORT void shutdown_devices_backend() {
    if (g_backends_initialized) {
        shutdown_video_capture_backend();
        shutdown_audio_capture_backend();
        g_backends_initialized = 0;
    }

    if (g_mf_initialized) {
        MFShutdown();
        g_mf_initialized = 0;
    }
    if (g_com_initialized) {
        CoUninitialize();
        g_com_initialized = 0;
    }
}

EXPORT int enum_video_devices() {
    g_video_device_count = 0;
    if (!g_mf_initialized) return 0;

    IMFAttributes* pAttributes = NULL;
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return 0;

    hr = pAttributes->lpVtbl->SetGUID(pAttributes, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        pAttributes->lpVtbl->Release(pAttributes);
        return 0;
    }

    IMFActivate** ppDevices = NULL;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    pAttributes->lpVtbl->Release(pAttributes);

    if (FAILED(hr) || count == 0) return 0;

    int loaded_count = 0;
    for (UINT32 i = 0; i < count && loaded_count < MAX_DEVICES; i++) {
        IMFActivate* pDevice = ppDevices[i];
        if (!pDevice) continue;

        WCHAR* name = NULL;
        UINT32 nameLen = 0;
        hr = pDevice->lpVtbl->GetAllocatedString(pDevice, &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &nameLen);
        if (SUCCEEDED(hr) && name) {
            wchar_to_utf8(name, g_video_devices[loaded_count].name, sizeof(g_video_devices[loaded_count].name));
            CoTaskMemFree(name);
        } else {
            snprintf(g_video_devices[loaded_count].name, sizeof(g_video_devices[loaded_count].name), "Dispositivo de Video %d", loaded_count + 1);
        }

        WCHAR* symLink = NULL;
        UINT32 symLinkLen = 0;
        hr = pDevice->lpVtbl->GetAllocatedString(pDevice, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &symLink, &symLinkLen);
        if (SUCCEEDED(hr) && symLink) {
            wchar_to_utf8(symLink, g_video_devices[loaded_count].id, sizeof(g_video_devices[loaded_count].id));
            CoTaskMemFree(symLink);
        } else {
            snprintf(g_video_devices[loaded_count].id, sizeof(g_video_devices[loaded_count].id), "vidcap_%d", loaded_count);
        }

        pDevice->lpVtbl->Release(pDevice);
        loaded_count++;
    }

    CoTaskMemFree(ppDevices);
    g_video_device_count = loaded_count;
    return g_video_device_count;
}

// R8: GUIDs locais removidos — usar COMMON_CLSID_MMDeviceEnumerator / COMMON_IID_IMMDeviceEnumerator de common.h

EXPORT int enum_audio_devices() {
    g_audio_device_count = 0;
    if (!g_com_initialized) return 0;

    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(&COMMON_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &COMMON_IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) return 0;

    IMMDeviceCollection* pCollection = NULL;
    hr = pEnumerator->lpVtbl->EnumAudioEndpoints(pEnumerator, eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        pEnumerator->lpVtbl->Release(pEnumerator);
        return 0;
    }

    UINT32 count = 0;
    hr = pCollection->lpVtbl->GetCount(pCollection, &count);
    if (FAILED(hr)) {
        pCollection->lpVtbl->Release(pCollection);
        pEnumerator->lpVtbl->Release(pEnumerator);
        return 0;
    }

    int loaded_count = 0;
    for (UINT32 i = 0; i < count && loaded_count < MAX_DEVICES; i++) {
        IMMDevice* pDevice = NULL;
        hr = pCollection->lpVtbl->Item(pCollection, i, &pDevice);
        if (FAILED(hr) || !pDevice) continue;

        LPWSTR pwszID = NULL;
        hr = pDevice->lpVtbl->GetId(pDevice, &pwszID);
        if (SUCCEEDED(hr) && pwszID) {
            wchar_to_utf8(pwszID, g_audio_devices[loaded_count].id, sizeof(g_audio_devices[loaded_count].id));
            CoTaskMemFree(pwszID);
        } else {
            snprintf(g_audio_devices[loaded_count].id, sizeof(g_audio_devices[loaded_count].id), "audiocap_%d", loaded_count);
        }

        IPropertyStore* pProps = NULL;
        hr = pDevice->lpVtbl->OpenPropertyStore(pDevice, STGM_READ, &pProps);
        if (SUCCEEDED(hr)) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            hr = pProps->lpVtbl->GetValue(pProps, &PKEY_Device_FriendlyName, &varName);
            if (SUCCEEDED(hr) && varName.pwszVal) {
                wchar_to_utf8(varName.pwszVal, g_audio_devices[loaded_count].name, sizeof(g_audio_devices[loaded_count].name));
                PropVariantClear(&varName);
            } else {
                snprintf(g_audio_devices[loaded_count].name, sizeof(g_audio_devices[loaded_count].name), "Dispositivo de Audio %d", loaded_count + 1);
            }
            pProps->lpVtbl->Release(pProps);
        } else {
            snprintf(g_audio_devices[loaded_count].name, sizeof(g_audio_devices[loaded_count].name), "Dispositivo de Audio %d", loaded_count + 1);
        }

        pDevice->lpVtbl->Release(pDevice);
        loaded_count++;
    }

    pCollection->lpVtbl->Release(pCollection);
    pEnumerator->lpVtbl->Release(pEnumerator);

    g_audio_device_count = loaded_count;
    return g_audio_device_count;
}

EXPORT const char* get_video_device_name(int index) {
    if (index < 0 || index >= g_video_device_count) return "";
    return g_video_devices[index].name;
}

EXPORT const char* get_video_device_id(int index) {
    if (index < 0 || index >= g_video_device_count) return "";
    return g_video_devices[index].id;
}

EXPORT const char* get_audio_device_name(int index) {
    if (index < 0 || index >= g_audio_device_count) return "";
    return g_audio_devices[index].name;
}

EXPORT const char* get_audio_device_id(int index) {
    if (index < 0 || index >= g_audio_device_count) return "";
    return g_audio_devices[index].id;
}
