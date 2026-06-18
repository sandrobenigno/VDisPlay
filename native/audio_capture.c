#include "include/audio_capture.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

static HANDLE g_audio_thread = NULL;
static int g_is_audio_capturing = 0;
static int g_enable_loopback = 0;
static float g_peak_volume = 0.0f;
static int g_audio_deinterleave = 0;

static IMMDevice* g_capture_device = NULL;
static IAudioClient* g_capture_client = NULL;
static IAudioCaptureClient* g_capture_client_in = NULL;

static IMMDevice* g_render_device = NULL;
static IAudioClient* g_render_client = NULL;
static IAudioRenderClient* g_render_client_out = NULL;

static CRITICAL_SECTION g_audio_cs;


static WORD g_in_format_tag = 0;
static WORD g_in_bits_per_sample = 0;
static WORD g_in_channels = 0;
static DWORD g_in_sample_rate = 0;
static GUID g_in_subformat = {0};

static WORD g_out_format_tag = 0;
static WORD g_out_bits_per_sample = 0;
static WORD g_out_channels = 0;
static DWORD g_out_sample_rate = 0;
static GUID g_out_subformat = {0};

// IIDs WASAPI locais
static const CLSID local_CLSID_MMDeviceEnumerator = {0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}};
static const IID local_IID_IMMDeviceEnumerator = {0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}};
static const IID local_IID_IAudioClient = {0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}};
static const IID local_IID_IAudioClient2 = {0x726778cd, 0xf60a, 0x4eda, {0x82, 0xde, 0xe4, 0x76, 0x10, 0xcd, 0x78, 0xaa}};
static const IID local_IID_IAudioCaptureClient = {0xc8adbd64, 0xe71e, 0x48a0, {0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17}};
static const IID local_IID_IAudioRenderClient = {0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}};

static const GUID local_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID local_KSDATAFORMAT_SUBTYPE_PCM = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

static float read_sample_as_float(const BYTE* pData, int index, WORD formatTag, WORD bitsPerSample, GUID subFormat) {
    if (formatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return ((const float*)pData)[index];
    } else if (formatTag == WAVE_FORMAT_PCM) {
        if (bitsPerSample == 16) {
            return ((const int16_t*)pData)[index] / 32768.0f;
        } else if (bitsPerSample == 24) {
            const uint8_t* p = pData + index * 3;
            int32_t val = (p[0] << 8) | (p[1] << 16) | (p[2] << 24);
            val >>= 8;
            return val / 8388608.0f;
        } else if (bitsPerSample == 32) {
            return ((const int32_t*)pData)[index] / 2147483648.0f;
        }
    } else if (formatTag == WAVE_FORMAT_EXTENSIBLE) {
        if (IsEqualGUID(&subFormat, &local_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            return ((const float*)pData)[index];
        } else if (IsEqualGUID(&subFormat, &local_KSDATAFORMAT_SUBTYPE_PCM)) {
            if (bitsPerSample == 16) {
                return ((const int16_t*)pData)[index] / 32768.0f;
            } else if (bitsPerSample == 24) {
                const uint8_t* p = pData + index * 3;
                int32_t val = (p[0] << 8) | (p[1] << 16) | (p[2] << 24);
                val >>= 8;
                return val / 8388608.0f;
            } else if (bitsPerSample == 32) {
                return ((const int32_t*)pData)[index] / 2147483648.0f;
            }
        }
    }
    return 0.0f;
}

static void write_sample_from_float(BYTE* pData, int index, float sample, WORD formatTag, WORD bitsPerSample, GUID subFormat) {
    if (sample < -1.0f) sample = -1.0f;
    if (sample > 1.0f) sample = 1.0f;

    if (formatTag == WAVE_FORMAT_IEEE_FLOAT) {
        ((float*)pData)[index] = sample;
    } else if (formatTag == WAVE_FORMAT_PCM) {
        if (bitsPerSample == 16) {
            ((int16_t*)pData)[index] = (int16_t)(sample * 32767.0f);
        } else if (bitsPerSample == 32) {
            ((int32_t*)pData)[index] = (int32_t)(sample * 2147483647.0f);
        }
    } else if (formatTag == WAVE_FORMAT_EXTENSIBLE) {
        if (IsEqualGUID(&subFormat, &local_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            ((float*)pData)[index] = sample;
        } else if (IsEqualGUID(&subFormat, &local_KSDATAFORMAT_SUBTYPE_PCM)) {
            if (bitsPerSample == 16) {
                ((int16_t*)pData)[index] = (int16_t)(sample * 32767.0f);
            } else if (bitsPerSample == 32) {
                ((int32_t*)pData)[index] = (int32_t)(sample * 2147483647.0f);
            }
        }
    }
}

static HRESULT get_capture_device_by_id(const char* device_id, IMMDevice** ppDevice) {
    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(&local_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &local_IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) return hr;

    IMMDeviceCollection* pCollection = NULL;
    hr = pEnumerator->lpVtbl->EnumAudioEndpoints(pEnumerator, eCapture, DEVICE_STATE_ACTIVE, &pCollection);
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

    IMMDevice* pFoundDevice = NULL;
    for (UINT32 i = 0; i < count; i++) {
        IMMDevice* pDevice = NULL;
        hr = pCollection->lpVtbl->Item(pCollection, i, &pDevice);
        if (FAILED(hr)) continue;

        LPWSTR pwszID = NULL;
        hr = pDevice->lpVtbl->GetId(pDevice, &pwszID);
        if (SUCCEEDED(hr) && pwszID) {
            char utf8_id[512];
            WideCharToMultiByte(CP_UTF8, 0, pwszID, -1, utf8_id, sizeof(utf8_id), NULL, NULL);
            CoTaskMemFree(pwszID);

            if (strcmp(utf8_id, device_id) == 0) {
                pFoundDevice = pDevice;
                pFoundDevice->lpVtbl->AddRef(pFoundDevice);
            }
        }
        pDevice->lpVtbl->Release(pDevice);
    }

    pCollection->lpVtbl->Release(pCollection);
    pEnumerator->lpVtbl->Release(pEnumerator);

    if (!pFoundDevice) return E_FAIL;
    *ppDevice = pFoundDevice;
    return S_OK;
}

static DWORD WINAPI audio_thread_proc(LPVOID lpParam) {
    HRESULT hr;

    WORD inTag = g_in_format_tag;
    WORD inBits = g_in_bits_per_sample;
    WORD inChannels = g_in_channels;
    DWORD inRate = g_in_sample_rate;
    GUID inSub = g_in_subformat;

    WORD outTag = g_out_format_tag;
    WORD outBits = g_out_bits_per_sample;
    WORD outChannels = g_out_channels;
    DWORD outRate = g_out_sample_rate;
    GUID outSub = g_out_subformat;

    hr = g_capture_client->lpVtbl->Start(g_capture_client);
    if (FAILED(hr)) {
        return 0;
    }

    if (g_render_client) {
        hr = g_render_client->lpVtbl->Start(g_render_client);
        if (FAILED(hr)) {
            g_capture_client->lpVtbl->Stop(g_capture_client);
            return 0;
        }
    }

    while (g_is_audio_capturing) {
        UINT32 packetSize = 0;
        hr = g_capture_client_in->lpVtbl->GetNextPacketSize(g_capture_client_in, &packetSize);
        if (FAILED(hr)) {
            Sleep(5);
            continue;
        }

        if (packetSize > 0) {
            BYTE* pCaptureData = NULL;
            UINT32 numFramesRead = 0;
            DWORD flags = 0;

            hr = g_capture_client_in->lpVtbl->GetBuffer(
                g_capture_client_in,
                &pCaptureData,
                &numFramesRead,
                &flags,
                NULL,
                NULL
            );

            if (SUCCEEDED(hr) && numFramesRead > 0) {
                // 1. Calcula volume do pico para o VU meter
                float peak = 0.0f;
                for (UINT32 i = 0; i < numFramesRead * inChannels; i++) {
                    float val = read_sample_as_float(pCaptureData, i, inTag, inBits, inSub);
                    float abs_val = val < 0.0f ? -val : val;
                    if (abs_val > peak) peak = abs_val;
                }

                // Aplica decaimento suave
                EnterCriticalSection(&g_audio_cs);
                if (peak > g_peak_volume) {
                    g_peak_volume = peak;
                } else {
                    g_peak_volume = g_peak_volume * 0.95f + peak * 0.05f;
                }
                int local_loopback = g_enable_loopback;
                int local_deinterleave = g_audio_deinterleave;
                LeaveCriticalSection(&g_audio_cs);

                // 2. Toca o áudio nos alto-falantes se habilitado
                if (local_loopback && g_render_client_out) {
                    int effChannels = inChannels;
                    DWORD effRate = inRate;
                    UINT32 effFrames = numFramesRead;

                    if (local_deinterleave && inChannels == 1) {
                        effChannels = 2;
                        effRate = inRate / 2;
                        effFrames = numFramesRead / 2;
                    }

                    // Calcula número proporcional de frames de saída (resampling linear)
                    UINT32 numFramesWrite = (UINT32)(effFrames * (double)outRate / effRate);
                    
                    // Verifica quanto espaço há no buffer de render
                    UINT32 padding = 0;
                    hr = g_render_client->lpVtbl->GetCurrentPadding(g_render_client, &padding);
                    UINT32 bufferFrameCount = 0;
                    g_render_client->lpVtbl->GetBufferSize(g_render_client, &bufferFrameCount);
                    UINT32 availableSpace = bufferFrameCount - padding;

                    if (numFramesWrite <= availableSpace) {
                        BYTE* pRenderData = NULL;
                        hr = g_render_client_out->lpVtbl->GetBuffer(g_render_client_out, numFramesWrite, &pRenderData);
                        if (SUCCEEDED(hr)) {
                            // Resampling linear com mapeamento de canais e formatos.
                            // Apenas envia áudio para os canais frontal esquerdo (0) e frontal direito (1).
                            // Zera canais adicionais (como centro, subwoofer e surround) para evitar
                            // cancelamento de fase ou distorção no downmix de sistemas surround (5.1 / 7.1).
                            for (UINT32 j = 0; j < numFramesWrite; j++) {
                                double x = j * (double)effFrames / numFramesWrite;
                                UINT32 k = (UINT32)x;
                                double t = x - k;

                                for (WORD ch = 0; ch < outChannels; ch++) {
                                    float interpolated = 0.0f;
                                    if (ch == 0 || ch == 1) {
                                        WORD in_ch = ch;
                                        if (effChannels == 1) {
                                            in_ch = 0; // Mono -> Stereo
                                        } else if (ch >= effChannels) {
                                            in_ch = effChannels - 1; // Proteção
                                        }

                                        float sample1 = read_sample_as_float(pCaptureData, k * effChannels + in_ch, inTag, inBits, inSub);
                                        float sample2 = (k + 1 < effFrames) ? read_sample_as_float(pCaptureData, (k + 1) * effChannels + in_ch, inTag, inBits, inSub) : sample1;

                                        interpolated = (float)((1.0 - t) * sample1 + t * sample2);
                                    }
                                    write_sample_from_float(pRenderData, j * outChannels + ch, interpolated, outTag, outBits, outSub);
                                }
                            }
                            g_render_client_out->lpVtbl->ReleaseBuffer(g_render_client_out, numFramesWrite, 0);
                        }
                    }
                }

                g_capture_client_in->lpVtbl->ReleaseBuffer(g_capture_client_in, numFramesRead);
            }
        } else {
            Sleep(1);
        }
    }

    g_capture_client->lpVtbl->Stop(g_capture_client);
    if (g_render_client) {
        g_render_client->lpVtbl->Stop(g_render_client);
    }

    return 0;
}

EXPORT int start_audio_capture(const char* device_id, int enable_loopback) {
    if (g_is_audio_capturing) {
        stop_audio_capture();
    }

    g_enable_loopback = enable_loopback;

    HRESULT hr = get_capture_device_by_id(device_id, &g_capture_device);
    if (FAILED(hr)) return 0;

    hr = g_capture_device->lpVtbl->Activate(g_capture_device, &local_IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&g_capture_client);
    if (FAILED(hr)) {
        g_capture_device->lpVtbl->Release(g_capture_device);
        g_capture_device = NULL;
        return 0;
    }

    // Configura a stream para modo RAW (ignora filtros do Windows como cancelamento de ruído e corte de graves/passa-alta)
    IAudioClient2* pAudioClient2 = NULL;
    if (SUCCEEDED(g_capture_client->lpVtbl->QueryInterface(g_capture_client, &local_IID_IAudioClient2, (void**)&pAudioClient2)) && pAudioClient2) {
        AudioClientProperties props;
        memset(&props, 0, sizeof(props));
        props.cbSize = sizeof(props);
        props.bIsOffload = FALSE;
        props.eCategory = 0; // AudioCategory_Other
        props.Options = 1;   // AUDCLNT_STREAMOPTIONS_RAW
        pAudioClient2->lpVtbl->SetClientProperties(pAudioClient2, &props);
        pAudioClient2->lpVtbl->Release(pAudioClient2);
    }

    WAVEFORMATEX* pFormatIn = NULL;
    hr = g_capture_client->lpVtbl->GetMixFormat(g_capture_client, &pFormatIn);
    if (FAILED(hr)) {
        g_capture_client->lpVtbl->Release(g_capture_client);
        g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device);
        g_capture_device = NULL;
        return 0;
    }

    // Tenta negociar estéreo (2 canais) caso o formato padrão mix format retornado seja mono (1 canal).
    // Isso é comum com placas de captura USB onde o Windows seleciona mono por padrão na primeira conexão.
    if (pFormatIn->nChannels == 1) {
        if (pFormatIn->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            WAVEFORMATEXTENSIBLE* pExt = (WAVEFORMATEXTENSIBLE*)pFormatIn;
            pExt->Format.nChannels = 2;
            pExt->Format.nBlockAlign = (pExt->Format.wBitsPerSample / 8) * 2;
            pExt->Format.nAvgBytesPerSec = pExt->Format.nSamplesPerSec * pExt->Format.nBlockAlign;
            pExt->dwChannelMask = 3; // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT
        } else {
            pFormatIn->nChannels = 2;
            pFormatIn->nBlockAlign = (pFormatIn->wBitsPerSample / 8) * 2;
            pFormatIn->nAvgBytesPerSec = pFormatIn->nSamplesPerSec * pFormatIn->nBlockAlign;
        }

        WAVEFORMATEX* pClosestMatch = NULL;
        HRESULT hr_test = g_capture_client->lpVtbl->IsFormatSupported(
            g_capture_client,
            AUDCLNT_SHAREMODE_SHARED,
            pFormatIn,
            &pClosestMatch
        );

        if (hr_test == S_OK) {
            if (pClosestMatch) CoTaskMemFree(pClosestMatch);
        } else if (hr_test == S_FALSE && pClosestMatch != NULL) {
            CoTaskMemFree(pFormatIn);
            pFormatIn = pClosestMatch;
        } else {
            if (pClosestMatch) CoTaskMemFree(pClosestMatch);
            CoTaskMemFree(pFormatIn);
            pFormatIn = NULL;
            g_capture_client->lpVtbl->GetMixFormat(g_capture_client, &pFormatIn);
        }
    }

    // Armazena as configurações negociadas nas variáveis estáticas globais para uso pela thread
    g_in_format_tag = pFormatIn->wFormatTag;
    g_in_bits_per_sample = pFormatIn->wBitsPerSample;
    g_in_channels = pFormatIn->nChannels;
    g_in_sample_rate = pFormatIn->nSamplesPerSec;
    if (g_in_format_tag == WAVE_FORMAT_EXTENSIBLE) {
        g_in_subformat = ((WAVEFORMATEXTENSIBLE*)pFormatIn)->SubFormat;
    } else {
        memset(&g_in_subformat, 0, sizeof(GUID));
    }

    // Inicializa captura em modo compartilhado
    REFERENCE_TIME bufferDuration = 200000; // 20ms
    hr = g_capture_client->lpVtbl->Initialize(
        g_capture_client,
        AUDCLNT_SHAREMODE_SHARED,
        0,
        bufferDuration,
        0,
        pFormatIn,
        NULL
    );
    CoTaskMemFree(pFormatIn);

    if (FAILED(hr)) {
        g_capture_client->lpVtbl->Release(g_capture_client);
        g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device);
        g_capture_device = NULL;
        return 0;
    }

    hr = g_capture_client->lpVtbl->GetService(g_capture_client, &local_IID_IAudioCaptureClient, (void**)&g_capture_client_in);
    if (FAILED(hr)) {
        g_capture_client->lpVtbl->Release(g_capture_client);
        g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device);
        g_capture_device = NULL;
        return 0;
    }

    // Configura o dispositivo de saída padrão se o loopback (monitoramento) estiver habilitado
    if (1) { // Sempre inicializa se possível, permitindo ativar dinamicamente
        IMMDeviceEnumerator* pEnumerator = NULL;
        hr = CoCreateInstance(&local_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &local_IID_IMMDeviceEnumerator, (void**)&pEnumerator);
        if (SUCCEEDED(hr)) {
            hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &g_render_device);
            if (SUCCEEDED(hr)) {
                hr = g_render_device->lpVtbl->Activate(g_render_device, &local_IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&g_render_client);
                if (SUCCEEDED(hr)) {
                    WAVEFORMATEX* pFormatOut = NULL;
                    hr = g_render_client->lpVtbl->GetMixFormat(g_render_client, &pFormatOut);
                    if (SUCCEEDED(hr)) {
                        REFERENCE_TIME renderBufferDuration = 1000000; // 100ms
                        hr = g_render_client->lpVtbl->Initialize(
                            g_render_client,
                            AUDCLNT_SHAREMODE_SHARED,
                            0,
                            renderBufferDuration,
                            0,
                            pFormatOut,
                            NULL
                        );

                        if (SUCCEEDED(hr)) {
                            // Salva as configurações de renderização nas variáveis globais
                            g_out_format_tag = pFormatOut->wFormatTag;
                            g_out_bits_per_sample = pFormatOut->wBitsPerSample;
                            g_out_channels = pFormatOut->nChannels;
                            g_out_sample_rate = pFormatOut->nSamplesPerSec;
                            if (g_out_format_tag == WAVE_FORMAT_EXTENSIBLE) {
                                g_out_subformat = ((WAVEFORMATEXTENSIBLE*)pFormatOut)->SubFormat;
                            } else {
                                memset(&g_out_subformat, 0, sizeof(GUID));
                            }

                            g_render_client->lpVtbl->GetService(g_render_client, &local_IID_IAudioRenderClient, (void**)&g_render_client_out);
                        }
                        CoTaskMemFree(pFormatOut);
                    }
                }
            }
            pEnumerator->lpVtbl->Release(pEnumerator);
        }
    }

    g_peak_volume = 0.0f;
    g_is_audio_capturing = 1;

    g_audio_thread = CreateThread(NULL, 0, audio_thread_proc, NULL, 0, NULL);
    if (!g_audio_thread) {
        g_is_audio_capturing = 0;
        g_capture_client_in->lpVtbl->Release(g_capture_client_in);
        g_capture_client_in = NULL;
        g_capture_client->lpVtbl->Release(g_capture_client);
        g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device);
        g_capture_device = NULL;

        if (g_render_client_out) { g_render_client_out->lpVtbl->Release(g_render_client_out); g_render_client_out = NULL; }
        if (g_render_client) { g_render_client->lpVtbl->Release(g_render_client); g_render_client = NULL; }
        if (g_render_device) { g_render_device->lpVtbl->Release(g_render_device); g_render_device = NULL; }
        return 0;
    }

    return 1;
}

EXPORT void stop_audio_capture() {
    if (!g_is_audio_capturing) return;

    g_is_audio_capturing = 0;

    if (g_audio_thread) {
        WaitForSingleObject(g_audio_thread, INFINITE);
        CloseHandle(g_audio_thread);
        g_audio_thread = NULL;
    }

    if (g_capture_client_in) {
        g_capture_client_in->lpVtbl->Release(g_capture_client_in);
        g_capture_client_in = NULL;
    }
    if (g_capture_client) {
        g_capture_client->lpVtbl->Release(g_capture_client);
        g_capture_client = NULL;
    }
    if (g_capture_device) {
        g_capture_device->lpVtbl->Release(g_capture_device);
        g_capture_device = NULL;
    }

    if (g_render_client_out) {
        g_render_client_out->lpVtbl->Release(g_render_client_out);
        g_render_client_out = NULL;
    }
    if (g_render_client) {
        g_render_client->lpVtbl->Release(g_render_client);
        g_render_client = NULL;
    }
    if (g_render_device) {
        g_render_device->lpVtbl->Release(g_render_device);
        g_render_device = NULL;
    }

}

EXPORT int is_audio_capturing() {
    return g_is_audio_capturing;
}

EXPORT int get_audio_channels() {
    return g_in_channels;
}

EXPORT float get_audio_peak_volume() {
    float peak = 0.0f;
    if (!g_is_audio_capturing) return 0.0f;
    EnterCriticalSection(&g_audio_cs);
    peak = g_peak_volume;
    LeaveCriticalSection(&g_audio_cs);
    return peak;
}

EXPORT void set_audio_loopback(int enabled) {
    EnterCriticalSection(&g_audio_cs);
    g_enable_loopback = enabled;
    LeaveCriticalSection(&g_audio_cs);
}

EXPORT void set_audio_deinterleave(int enabled) {
    EnterCriticalSection(&g_audio_cs);
    g_audio_deinterleave = enabled;
    LeaveCriticalSection(&g_audio_cs);
}

EXPORT int get_audio_deinterleave() {
    int enabled;
    EnterCriticalSection(&g_audio_cs);
    enabled = g_audio_deinterleave;
    LeaveCriticalSection(&g_audio_cs);
    return enabled;
}

void init_audio_capture_backend() {
    InitializeCriticalSection(&g_audio_cs);
}

void shutdown_audio_capture_backend() {
    DeleteCriticalSection(&g_audio_cs);
}
