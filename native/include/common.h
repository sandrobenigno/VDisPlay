#ifndef VDISPLAY_COMMON_H
#define VDISPLAY_COMMON_H

#include <windows.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Macros de exportação
// ---------------------------------------------------------------------------
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

// ---------------------------------------------------------------------------
// Utilitários de string
// ---------------------------------------------------------------------------

// Converte WCHAR (UTF-16) para UTF-8. Garante terminação nula.
static inline void wchar_to_utf8(const WCHAR* src, char* dst, int dst_len) {
    if (!src || !dst || dst_len <= 0) return;
    WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dst_len, NULL, NULL);
    dst[dst_len - 1] = '\0';
}

// Converte UTF-8 para WCHAR (UTF-16). Garante terminação nula.
static inline void utf8_to_wchar(const char* src, WCHAR* dst, int dst_len) {
    if (!src || !dst || dst_len <= 0) return;
    MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst_len);
    dst[dst_len - 1] = L'\0';
}

// ---------------------------------------------------------------------------
// GUIDs WASAPI/MMDevice — definidos aqui uma única vez para evitar duplicação
// e dependências de linker em múltiplos módulos.
// ---------------------------------------------------------------------------
static const CLSID COMMON_CLSID_MMDeviceEnumerator =
    {0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}};

static const IID COMMON_IID_IMMDeviceEnumerator =
    {0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}};

// ---------------------------------------------------------------------------
// Helper: abre um IMMDevice de captura pelo seu ID (string UTF-8).
// Retorna S_OK e preenche *ppDevice em caso de sucesso.
// O chamador é responsável por liberar *ppDevice com ->Release().
// ---------------------------------------------------------------------------
#include <mmdeviceapi.h>

static inline HRESULT common_get_capture_device_by_id(
        const char* device_id, IMMDevice** ppDevice) {

    if (!device_id || !ppDevice) return E_INVALIDARG;

    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(
        &COMMON_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
        &COMMON_IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) return hr;

    IMMDeviceCollection* pCollection = NULL;
    hr = pEnumerator->lpVtbl->EnumAudioEndpoints(
        pEnumerator, eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        pEnumerator->lpVtbl->Release(pEnumerator);
        return hr;
    }

    UINT32 count = 0;
    hr = pCollection->lpVtbl->GetCount(pCollection, &count);
    if (FAILED(hr)) {
        pCollection->lpVtbl->Release(pCollection);
        pEnumerator->lpVtbl->Release(pEnumerator);
        return hr;
    }

    IMMDevice* pFound = NULL;
    for (UINT32 i = 0; i < count && !pFound; i++) {
        IMMDevice* pDevice = NULL;
        if (FAILED(pCollection->lpVtbl->Item(pCollection, i, &pDevice))) continue;

        LPWSTR pwszID = NULL;
        if (SUCCEEDED(pDevice->lpVtbl->GetId(pDevice, &pwszID)) && pwszID) {
            char utf8_id[512];
            wchar_to_utf8(pwszID, utf8_id, sizeof(utf8_id));
            CoTaskMemFree(pwszID);
            if (strcmp(utf8_id, device_id) == 0) {
                pFound = pDevice;
                pFound->lpVtbl->AddRef(pFound);
            }
        }
        pDevice->lpVtbl->Release(pDevice);
    }

    pCollection->lpVtbl->Release(pCollection);
    pEnumerator->lpVtbl->Release(pEnumerator);

    if (!pFound) return E_FAIL;
    *ppDevice = pFound;
    return S_OK;
}

#endif /* VDISPLAY_COMMON_H */
