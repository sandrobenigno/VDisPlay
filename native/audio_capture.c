#include "include/audio_capture.h"
#include "include/common.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Estado global de captura de áudio
// ---------------------------------------------------------------------------
static HANDLE g_audio_thread       = NULL;
static volatile int g_is_audio_capturing = 0;  // R3: volatile para visibilidade entre threads
static int    g_enable_loopback    = 0;
static float  g_peak_volume        = 0.0f;
static int    g_audio_deinterleave = 0;

static IMMDevice*          g_capture_device   = NULL;
static IAudioClient*       g_capture_client   = NULL;
static IAudioCaptureClient* g_capture_client_in = NULL;

static IMMDevice*          g_render_device    = NULL;
static IAudioClient*       g_render_client    = NULL;
static IAudioRenderClient* g_render_client_out = NULL;

static CRITICAL_SECTION g_audio_cs;

// Configurações negociadas de entrada e saída
static WORD  g_in_format_tag      = 0;
static WORD  g_in_bits_per_sample = 0;
static WORD  g_in_channels        = 0;
static DWORD g_in_sample_rate     = 0;
static GUID  g_in_subformat       = {0};

static WORD  g_out_format_tag      = 0;
static WORD  g_out_bits_per_sample = 0;
static WORD  g_out_channels        = 0;
static DWORD g_out_sample_rate     = 0;
static GUID  g_out_subformat       = {0};

// IIDs WASAPI locais (R8: mantidos apenas aqui, common.h já tem os de IMMDevice)
static const IID local_IID_IAudioClient =
    {0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}};
static const IID local_IID_IAudioClient2 =
    {0x726778cd, 0xf60a, 0x4eda, {0x82, 0xde, 0xe4, 0x76, 0x10, 0xcd, 0x78, 0xaa}};
static const IID local_IID_IAudioCaptureClient =
    {0xc8adbd64, 0xe71e, 0x48a0, {0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17}};
static const IID local_IID_IAudioRenderClient =
    {0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}};

static const GUID local_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT =
    {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID local_KSDATAFORMAT_SUBTYPE_PCM =
    {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

// ---------------------------------------------------------------------------
// Leitura/escrita de amostras normalizadas em [-1.0, 1.0]
// ---------------------------------------------------------------------------

// P4: tipo de ponteiro de função para eliminar branches por amostra no loop interno
typedef float (*ReadSampleFn)(const BYTE* data, int index);
typedef void  (*WriteSampleFn)(BYTE* data, int index, float sample);

static float read_ieee_float   (const BYTE* d, int i) { return ((const float*)d)[i]; }
static float read_pcm_int16    (const BYTE* d, int i) { return ((const int16_t*)d)[i] / 32768.0f; }
static float read_pcm_int32    (const BYTE* d, int i) { return ((const int32_t*)d)[i] / 2147483648.0f; }
static float read_pcm_int24(const BYTE* d, int i) {
    const uint8_t* p = d + i * 3;
    int32_t val = ((int32_t)p[0] << 8) | ((int32_t)p[1] << 16) | ((int32_t)p[2] << 24);
    val >>= 8;
    return val / 8388608.0f;
}
static float read_zero(const BYTE* d, int i) { (void)d; (void)i; return 0.0f; }

static void write_ieee_float(BYTE* d, int i, float s) { ((float*)d)[i] = s; }
static void write_pcm_int16 (BYTE* d, int i, float s) {
    ((int16_t*)d)[i] = (int16_t)(s * 32767.0f);
}
static void write_pcm_int32 (BYTE* d, int i, float s) {
    ((int32_t*)d)[i] = (int32_t)(s * 2147483647.0f);
}
static void write_noop(BYTE* d, int i, float s) { (void)d; (void)i; (void)s; }

// Seleciona função de leitura com base no formato WAVEFORMAT
static ReadSampleFn select_reader(WORD formatTag, WORD bits, const GUID* subFmt) {
    if (formatTag == WAVE_FORMAT_IEEE_FLOAT) return read_ieee_float;
    if (formatTag == WAVE_FORMAT_PCM) {
        if (bits == 16) return read_pcm_int16;
        if (bits == 24) return read_pcm_int24;
        if (bits == 32) return read_pcm_int32;
    }
    if (formatTag == WAVE_FORMAT_EXTENSIBLE) {
        if (IsEqualGUID(subFmt, &local_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
            return read_ieee_float;
        if (IsEqualGUID(subFmt, &local_KSDATAFORMAT_SUBTYPE_PCM)) {
            if (bits == 16) return read_pcm_int16;
            if (bits == 24) return read_pcm_int24;
            if (bits == 32) return read_pcm_int32;
        }
    }
    return read_zero;
}

// Seleciona função de escrita com base no formato WAVEFORMAT
static WriteSampleFn select_writer(WORD formatTag, WORD bits, const GUID* subFmt) {
    if (formatTag == WAVE_FORMAT_IEEE_FLOAT) return write_ieee_float;
    if (formatTag == WAVE_FORMAT_PCM) {
        if (bits == 16) return write_pcm_int16;
        if (bits == 32) return write_pcm_int32;
    }
    if (formatTag == WAVE_FORMAT_EXTENSIBLE) {
        if (IsEqualGUID(subFmt, &local_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
            return write_ieee_float;
        if (IsEqualGUID(subFmt, &local_KSDATAFORMAT_SUBTYPE_PCM)) {
            if (bits == 16) return write_pcm_int16;
            if (bits == 32) return write_pcm_int32;
        }
    }
    return write_noop;
}

// Clamp inline (P3: usa float em vez de double)
static inline float clampf(float v) {
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

// ---------------------------------------------------------------------------
// Thread de captura de áudio
// ---------------------------------------------------------------------------
static DWORD WINAPI audio_thread_proc(LPVOID lpParam) {
    (void)lpParam;

    // Copia configurações de formato para locais (sem lock durante o loop)
    WORD  inTag      = g_in_format_tag;
    WORD  inBits     = g_in_bits_per_sample;
    WORD  inChannels = g_in_channels;
    DWORD inRate     = g_in_sample_rate;
    GUID  inSub      = g_in_subformat;

    WORD  outTag      = g_out_format_tag;
    WORD  outBits     = g_out_bits_per_sample;
    WORD  outChannels = g_out_channels;
    DWORD outRate     = g_out_sample_rate;
    GUID  outSub      = g_out_subformat;

    // P4: seleciona ponteiros de função uma única vez antes do loop
    ReadSampleFn  readSample  = select_reader(inTag,  inBits,  &inSub);
    WriteSampleFn writeSample = select_writer(outTag, outBits, &outSub);

    HRESULT hr = g_capture_client->lpVtbl->Start(g_capture_client);
    if (FAILED(hr)) {
        // R6: sinaliza parada para que o Flutter não fique lendo dados vazios
        g_is_audio_capturing = 0;
        return 0;
    }

    if (g_render_client) {
        hr = g_render_client->lpVtbl->Start(g_render_client);
        if (FAILED(hr)) {
            g_capture_client->lpVtbl->Stop(g_capture_client);
            g_is_audio_capturing = 0;  // R6
            return 0;
        }
    }

    while (g_is_audio_capturing) {
        UINT32 packetSize = 0;
        hr = g_capture_client_in->lpVtbl->GetNextPacketSize(
            g_capture_client_in, &packetSize);
        if (FAILED(hr)) {
            Sleep(5);
            continue;
        }

        if (packetSize == 0) {
            Sleep(1);
            continue;
        }

        BYTE*  pCaptureData  = NULL;
        UINT32 numFramesRead = 0;
        DWORD  flags         = 0;

        hr = g_capture_client_in->lpVtbl->GetBuffer(
            g_capture_client_in,
            &pCaptureData, &numFramesRead, &flags,
            NULL, NULL);

        if (FAILED(hr) || numFramesRead == 0) continue;

        // 1. VU meter — P4: sem branches por amostra; usa ponteiro de função pré-selecionado
        float peak = 0.0f;
        UINT32 total_samples = numFramesRead * inChannels;
        for (UINT32 i = 0; i < total_samples; i++) {
            float val = readSample(pCaptureData, i);
            float abs_val = val < 0.0f ? -val : val;
            if (abs_val > peak) peak = abs_val;
        }

        // Aplica decaimento suave ao pico
        EnterCriticalSection(&g_audio_cs);
        if (peak > g_peak_volume) {
            g_peak_volume = peak;
        } else {
            g_peak_volume = g_peak_volume * 0.95f + peak * 0.05f;
        }
        int local_loopback     = g_enable_loopback;
        int local_deinterleave = g_audio_deinterleave;
        LeaveCriticalSection(&g_audio_cs);

        // 2. Loopback para alto-falantes
        if (local_loopback && g_render_client_out) {
            int   effChannels = inChannels;
            DWORD effRate     = inRate;
            UINT32 effFrames  = numFramesRead;

            if (local_deinterleave && inChannels == 1) {
                effChannels = 2;
                effRate     = inRate / 2;
                effFrames   = numFramesRead / 2;
            }

            // P3: resampling linear em float (era double desnecessariamente)
            UINT32 numFramesWrite = (UINT32)(effFrames * (float)outRate / (float)effRate);

            UINT32 padding = 0;
            g_render_client->lpVtbl->GetCurrentPadding(g_render_client, &padding);
            UINT32 bufferFrameCount = 0;
            g_render_client->lpVtbl->GetBufferSize(g_render_client, &bufferFrameCount);
            UINT32 availableSpace = bufferFrameCount - padding;

            if (numFramesWrite <= availableSpace) {
                BYTE* pRenderData = NULL;
                hr = g_render_client_out->lpVtbl->GetBuffer(
                    g_render_client_out, numFramesWrite, &pRenderData);
                if (SUCCEEDED(hr)) {
                    // Resampling linear com mapeamento de canais.
                    // Envia áudio apenas para os canais frontal esquerdo (0) e direito (1).
                    // Canais adicionais (centro, LFE, surround) são zerados para evitar
                    // cancelamento de fase em sistemas 5.1/7.1.
                    for (UINT32 j = 0; j < numFramesWrite; j++) {
                        float x  = (float)j * (float)effFrames / (float)numFramesWrite;
                        UINT32 k = (UINT32)x;
                        float  t = x - (float)k;

                        for (WORD ch = 0; ch < outChannels; ch++) {
                            float interpolated = 0.0f;
                            if (ch <= 1) { // apenas L e R
                                WORD in_ch = ch;
                                if (effChannels == 1) {
                                    in_ch = 0; // mono → stereo
                                } else if (ch >= (WORD)effChannels) {
                                    in_ch = (WORD)(effChannels - 1);
                                }
                                float s1 = readSample(pCaptureData,
                                    (int)(k * effChannels + in_ch));
                                float s2 = (k + 1 < effFrames)
                                    ? readSample(pCaptureData,
                                        (int)((k + 1) * effChannels + in_ch))
                                    : s1;
                                interpolated = clampf((1.0f - t) * s1 + t * s2);
                            }
                            writeSample(pRenderData,
                                (int)(j * outChannels + ch), interpolated);
                        }
                    }
                    g_render_client_out->lpVtbl->ReleaseBuffer(
                        g_render_client_out, numFramesWrite, 0);
                }
            }
        }

        g_capture_client_in->lpVtbl->ReleaseBuffer(g_capture_client_in, numFramesRead);
    }

    g_capture_client->lpVtbl->Stop(g_capture_client);
    if (g_render_client) g_render_client->lpVtbl->Stop(g_render_client);

    return 0;
}

// ---------------------------------------------------------------------------
// Inicia captura de áudio
// ---------------------------------------------------------------------------
EXPORT int start_audio_capture(const char* device_id, int enable_loopback) {
    if (g_is_audio_capturing) {
        stop_audio_capture();
    }

    g_enable_loopback = enable_loopback;

    // R7: usa o helper compartilhado de common.h (sem duplicação de código)
    HRESULT hr = common_get_capture_device_by_id(device_id, &g_capture_device);
    if (FAILED(hr)) return 0;

    hr = g_capture_device->lpVtbl->Activate(
        g_capture_device, &local_IID_IAudioClient,
        CLSCTX_ALL, NULL, (void**)&g_capture_client);
    if (FAILED(hr)) {
        g_capture_device->lpVtbl->Release(g_capture_device);
        g_capture_device = NULL;
        return 0;
    }

    // Configura modo RAW (ignora filtros do Windows: cancelamento de ruído,
    // corte de graves, AGC, etc.)
    IAudioClient2* pAudioClient2 = NULL;
    if (SUCCEEDED(g_capture_client->lpVtbl->QueryInterface(
            g_capture_client, &local_IID_IAudioClient2, (void**)&pAudioClient2))
            && pAudioClient2) {
        AudioClientProperties props;
        memset(&props, 0, sizeof(props));
        props.cbSize   = sizeof(props);
        props.bIsOffload = FALSE;
        props.eCategory = 0;  // AudioCategory_Other
        props.Options   = 1;  // AUDCLNT_STREAMOPTIONS_RAW
        pAudioClient2->lpVtbl->SetClientProperties(pAudioClient2, &props);
        pAudioClient2->lpVtbl->Release(pAudioClient2);
    }

    WAVEFORMATEX* pFormatIn = NULL;
    hr = g_capture_client->lpVtbl->GetMixFormat(g_capture_client, &pFormatIn);
    if (FAILED(hr)) {
        g_capture_client->lpVtbl->Release(g_capture_client); g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device); g_capture_device = NULL;
        return 0;
    }

    // Tenta negociar estéreo caso o MixFormat padrão seja mono.
    // Comum em placas de captura USB na primeira conexão.
    if (pFormatIn->nChannels == 1) {
        if (pFormatIn->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            WAVEFORMATEXTENSIBLE* pExt = (WAVEFORMATEXTENSIBLE*)pFormatIn;
            pExt->Format.nChannels     = 2;
            pExt->Format.nBlockAlign   = (pExt->Format.wBitsPerSample / 8) * 2;
            pExt->Format.nAvgBytesPerSec =
                pExt->Format.nSamplesPerSec * pExt->Format.nBlockAlign;
            pExt->dwChannelMask = 3; // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT
        } else {
            pFormatIn->nChannels      = 2;
            pFormatIn->nBlockAlign    = (pFormatIn->wBitsPerSample / 8) * 2;
            pFormatIn->nAvgBytesPerSec =
                pFormatIn->nSamplesPerSec * pFormatIn->nBlockAlign;
        }

        WAVEFORMATEX* pClosestMatch = NULL;
        HRESULT hr_test = g_capture_client->lpVtbl->IsFormatSupported(
            g_capture_client, AUDCLNT_SHAREMODE_SHARED, pFormatIn, &pClosestMatch);

        if (hr_test == S_OK) {
            if (pClosestMatch) CoTaskMemFree(pClosestMatch);
        } else if (hr_test == S_FALSE && pClosestMatch) {
            CoTaskMemFree(pFormatIn);
            pFormatIn = pClosestMatch;
        } else {
            if (pClosestMatch) CoTaskMemFree(pClosestMatch);
            CoTaskMemFree(pFormatIn);
            pFormatIn = NULL;
            g_capture_client->lpVtbl->GetMixFormat(g_capture_client, &pFormatIn);
        }
    }

    // Armazena configurações negociadas para uso pela thread de captura
    g_in_format_tag      = pFormatIn->wFormatTag;
    g_in_bits_per_sample = pFormatIn->wBitsPerSample;
    g_in_channels        = pFormatIn->nChannels;
    g_in_sample_rate     = pFormatIn->nSamplesPerSec;
    if (g_in_format_tag == WAVE_FORMAT_EXTENSIBLE) {
        g_in_subformat = ((WAVEFORMATEXTENSIBLE*)pFormatIn)->SubFormat;
    } else {
        memset(&g_in_subformat, 0, sizeof(GUID));
    }

    // Inicializa captura em modo compartilhado com buffer de 20ms
    REFERENCE_TIME bufferDuration = 200000;
    hr = g_capture_client->lpVtbl->Initialize(
        g_capture_client, AUDCLNT_SHAREMODE_SHARED,
        0, bufferDuration, 0, pFormatIn, NULL);
    CoTaskMemFree(pFormatIn);

    if (FAILED(hr)) {
        g_capture_client->lpVtbl->Release(g_capture_client); g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device); g_capture_device = NULL;
        return 0;
    }

    hr = g_capture_client->lpVtbl->GetService(
        g_capture_client, &local_IID_IAudioCaptureClient,
        (void**)&g_capture_client_in);
    if (FAILED(hr)) {
        g_capture_client->lpVtbl->Release(g_capture_client); g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device); g_capture_device = NULL;
        return 0;
    }

    // Inicializa dispositivo de saída (sempre, para permitir ativação dinâmica do loopback)
    // R10: removido `if (1)` espúrio — bloco sempre executado, desaninhado diretamente
    {
        IMMDeviceEnumerator* pEnum = NULL;
        hr = CoCreateInstance(&COMMON_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
            &COMMON_IID_IMMDeviceEnumerator, (void**)&pEnum);
        if (SUCCEEDED(hr)) {
            hr = pEnum->lpVtbl->GetDefaultAudioEndpoint(
                pEnum, eRender, eConsole, &g_render_device);
            if (SUCCEEDED(hr)) {
                hr = g_render_device->lpVtbl->Activate(
                    g_render_device, &local_IID_IAudioClient,
                    CLSCTX_ALL, NULL, (void**)&g_render_client);
                if (SUCCEEDED(hr)) {
                    WAVEFORMATEX* pFormatOut = NULL;
                    hr = g_render_client->lpVtbl->GetMixFormat(
                        g_render_client, &pFormatOut);
                    if (SUCCEEDED(hr)) {
                        REFERENCE_TIME renderBufDur = 1000000; // 100ms
                        hr = g_render_client->lpVtbl->Initialize(
                            g_render_client, AUDCLNT_SHAREMODE_SHARED,
                            0, renderBufDur, 0, pFormatOut, NULL);
                        if (SUCCEEDED(hr)) {
                            g_out_format_tag      = pFormatOut->wFormatTag;
                            g_out_bits_per_sample = pFormatOut->wBitsPerSample;
                            g_out_channels        = pFormatOut->nChannels;
                            g_out_sample_rate     = pFormatOut->nSamplesPerSec;
                            if (g_out_format_tag == WAVE_FORMAT_EXTENSIBLE) {
                                g_out_subformat =
                                    ((WAVEFORMATEXTENSIBLE*)pFormatOut)->SubFormat;
                            } else {
                                memset(&g_out_subformat, 0, sizeof(GUID));
                            }
                            g_render_client->lpVtbl->GetService(
                                g_render_client, &local_IID_IAudioRenderClient,
                                (void**)&g_render_client_out);
                        }
                        CoTaskMemFree(pFormatOut);
                    }
                }
            }
            pEnum->lpVtbl->Release(pEnum);
        }
    }

    g_peak_volume        = 0.0f;
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
        if (g_render_client)     { g_render_client->lpVtbl->Release(g_render_client);         g_render_client     = NULL; }
        if (g_render_device)     { g_render_device->lpVtbl->Release(g_render_device);         g_render_device     = NULL; }
        return 0;
    }

    return 1;
}

// ---------------------------------------------------------------------------
// Para captura de áudio
// ---------------------------------------------------------------------------
EXPORT void stop_audio_capture() {
    if (!g_is_audio_capturing) return;

    g_is_audio_capturing = 0;

    if (g_audio_thread) {
        WaitForSingleObject(g_audio_thread, INFINITE);
        CloseHandle(g_audio_thread);
        g_audio_thread = NULL;
    }

    if (g_capture_client_in) { g_capture_client_in->lpVtbl->Release(g_capture_client_in); g_capture_client_in = NULL; }
    if (g_capture_client)    { g_capture_client->lpVtbl->Release(g_capture_client);         g_capture_client    = NULL; }
    if (g_capture_device)    { g_capture_device->lpVtbl->Release(g_capture_device);         g_capture_device    = NULL; }

    if (g_render_client_out) { g_render_client_out->lpVtbl->Release(g_render_client_out);   g_render_client_out = NULL; }
    if (g_render_client)     { g_render_client->lpVtbl->Release(g_render_client);           g_render_client     = NULL; }
    if (g_render_device)     { g_render_device->lpVtbl->Release(g_render_device);           g_render_device     = NULL; }
}

EXPORT int is_audio_capturing() {
    return g_is_audio_capturing;
}

EXPORT int get_audio_channels() {
    return g_in_channels;
}

EXPORT float get_audio_peak_volume() {
    if (!g_is_audio_capturing) return 0.0f;
    float peak;
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

// ---------------------------------------------------------------------------
// Inicialização / finalização do backend
// ---------------------------------------------------------------------------
void init_audio_capture_backend() {
    InitializeCriticalSection(&g_audio_cs);
}

void shutdown_audio_capture_backend() {
    stop_audio_capture();
    DeleteCriticalSection(&g_audio_cs);
}
