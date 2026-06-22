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
static int g_prebuffering = 1;

static HANDLE g_audio_event        = NULL;

// Ring Buffers e Controle de Fase do Resampler
#define INPUT_RING_BUFFER_SIZE_FRAMES 48000
#define OUTPUT_RING_BUFFER_SIZE_FRAMES 48000

static float g_input_ring_buffer[INPUT_RING_BUFFER_SIZE_FRAMES * 2];
static float g_output_ring_buffer[OUTPUT_RING_BUFFER_SIZE_FRAMES * 2];

static uint64_t g_input_frames_written = 0;
static uint64_t g_output_frames_written = 0;
static uint64_t g_output_frames_read = 0;
static double   g_resampler_input_index = 0.0;

// Interpolação Cúbica Hermite (Catmull-Rom)
static inline float hermite_interp(float t, float y0, float y1, float y2, float y3) {
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + c0;
}


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

    ReadSampleFn  readSample  = select_reader(inTag,  inBits,  &inSub);
    WriteSampleFn writeSample = select_writer(outTag, outBits, &outSub);

    HRESULT hr = g_capture_client->lpVtbl->Start(g_capture_client);
    if (FAILED(hr)) {
        g_is_audio_capturing = 0;
        return 0;
    }

    if (g_render_client) {
        hr = g_render_client->lpVtbl->Start(g_render_client);
        if (FAILED(hr)) {
            g_capture_client->lpVtbl->Stop(g_capture_client);
            g_is_audio_capturing = 0;
            return 0;
        }
    }

    while (g_is_audio_capturing) {
        // Espera pelo evento WASAPI indicando novos dados disponíveis (ou timeout de 2s)
        DWORD waitResult = WaitForSingleObject(g_audio_event, 2000);
        if (waitResult != WAIT_OBJECT_0 || !g_is_audio_capturing) {
            continue;
        }

        UINT32 packetSize = 0;
        hr = g_capture_client_in->lpVtbl->GetNextPacketSize(g_capture_client_in, &packetSize);
        if (FAILED(hr)) {
            continue;
        }

        // Processa todos os pacotes acumulados
        while (packetSize > 0 && g_is_audio_capturing) {
            BYTE*  pCaptureData  = NULL;
            UINT32 numFramesRead = 0;
            DWORD  flags         = 0;

            hr = g_capture_client_in->lpVtbl->GetBuffer(
                g_capture_client_in,
                &pCaptureData, &numFramesRead, &flags,
                NULL, NULL);

            if (SUCCEEDED(hr) && numFramesRead > 0) {
                EnterCriticalSection(&g_audio_cs);
                int local_loopback     = g_enable_loopback;
                int local_deinterleave = g_audio_deinterleave;
                LeaveCriticalSection(&g_audio_cs);

                int   effChannels = inChannels;
                DWORD effRate     = inRate;
                UINT32 effFrames  = numFramesRead;

                if (local_deinterleave && inChannels == 1) {
                    effChannels = 2;
                    effRate     = inRate / 2;
                    effFrames   = numFramesRead / 2;
                }

                // Escreve os dados normalizados de entrada no buffer circular de entrada
                float peak = 0.0f;
                for (UINT32 f = 0; f < effFrames; f++) {
                    float left_sample = 0.0f;
                    float right_sample = 0.0f;

                    if (local_deinterleave && inChannels == 1) {
                        left_sample  = readSample(pCaptureData, (int)(f * 2 + 0));
                        right_sample = readSample(pCaptureData, (int)(f * 2 + 1));
                    } else if (inChannels == 1) {
                        float s = readSample(pCaptureData, (int)f);
                        left_sample  = s;
                        right_sample = s;
                    } else {
                        left_sample  = readSample(pCaptureData, (int)(f * inChannels + 0));
                        right_sample = readSample(pCaptureData, (int)(f * inChannels + 1));
                    }

                    float abs_l = left_sample < 0.0f ? -left_sample : left_sample;
                    float abs_r = right_sample < 0.0f ? -right_sample : right_sample;
                    if (abs_l > peak) peak = abs_l;
                    if (abs_r > peak) peak = abs_r;

                    uint64_t write_frame_index = g_input_frames_written + f;
                    uint32_t ring_index = (uint32_t)(write_frame_index % INPUT_RING_BUFFER_SIZE_FRAMES);
                    g_input_ring_buffer[ring_index * 2 + 0] = left_sample;
                    g_input_ring_buffer[ring_index * 2 + 1] = right_sample;
                }
                g_input_frames_written += effFrames;

                // VU Meter: decaimento suave
                EnterCriticalSection(&g_audio_cs);
                if (peak > g_peak_volume) {
                    g_peak_volume = peak;
                } else {
                    g_peak_volume = g_peak_volume * 0.95f + peak * 0.05f;
                }
                LeaveCriticalSection(&g_audio_cs);

                 // Executa resampling Hermite para o Output Ring Buffer
                 uint64_t local_buffered = g_output_frames_written - g_output_frames_read;
                 UINT32 padding = 0;
                 if (local_loopback && g_render_client) {
                     g_render_client->lpVtbl->GetCurrentPadding(g_render_client, &padding);
                 }
                 uint64_t total_buffered = local_buffered + padding;

                 double target_buffer = outRate * 0.04; // 40ms target
                 double error = 0.0;
                 double adjustment = 0.0;

                 // O P-Controller agora roda sempre para manter o sincronismo de buffer
                 // independentemente do áudio estar mutado (loopback == 0)
                 error = (double)total_buffered - target_buffer;
                 double max_error = outRate * 0.1;
                 if (error > max_error) error = max_error;
                 if (error < -max_error) error = -max_error;

                 adjustment = (error / outRate) * 0.3; // 30% por segundo de erro
                 if (adjustment > 0.02) adjustment = 0.02;
                 if (adjustment < -0.02) adjustment = -0.02;

                 double base_step = (double)effRate / outRate;
                 double input_step = base_step * (1.0 + adjustment);

                 while (g_resampler_input_index + 4.0 < (double)g_input_frames_written) {
                     double input_frame_idx = g_resampler_input_index;
                     int64_t k = (int64_t)floor(input_frame_idx);
                     float t = (float)(input_frame_idx - k);

                     int64_t idx_m1 = k - 1;
                     int64_t idx_0  = k;
                     int64_t idx_p1 = k + 1;
                     int64_t idx_p2 = k + 2;

                     uint32_t r_m1 = (uint32_t)((idx_m1 < 0 ? 0 : idx_m1) % INPUT_RING_BUFFER_SIZE_FRAMES);
                     uint32_t r_0  = (uint32_t)((idx_0  < 0 ? 0 : idx_0)  % INPUT_RING_BUFFER_SIZE_FRAMES);
                     uint32_t r_p1 = (uint32_t)((idx_p1 < 0 ? 0 : idx_p1) % INPUT_RING_BUFFER_SIZE_FRAMES);
                     uint32_t r_p2 = (uint32_t)((idx_p2 < 0 ? 0 : idx_p2) % INPUT_RING_BUFFER_SIZE_FRAMES);

                     float left_y0 = g_input_ring_buffer[r_m1 * 2 + 0];
                     float left_y1 = g_input_ring_buffer[r_0  * 2 + 0];
                     float left_y2 = g_input_ring_buffer[r_p1 * 2 + 0];
                     float left_y3 = g_input_ring_buffer[r_p2 * 2 + 0];

                     float right_y0 = g_input_ring_buffer[r_m1 * 2 + 1];
                     float right_y1 = g_input_ring_buffer[r_0  * 2 + 1];
                     float right_y2 = g_input_ring_buffer[r_p1 * 2 + 1];
                     float right_y3 = g_input_ring_buffer[r_p2 * 2 + 1];

                     float left_out  = clampf(hermite_interp(t, left_y0, left_y1, left_y2, left_y3));
                     float right_out = clampf(hermite_interp(t, right_y0, right_y1, right_y2, right_y3));

                     uint32_t out_ring_index = (uint32_t)(g_output_frames_written % OUTPUT_RING_BUFFER_SIZE_FRAMES);
                     g_output_ring_buffer[out_ring_index * 2 + 0] = left_out;
                     g_output_ring_buffer[out_ring_index * 2 + 1] = right_out;

                     g_output_frames_written++;
                     g_resampler_input_index += input_step;
                 }

                // 2. Loopback para alto-falantes
                if (g_render_client_out) {
                    UINT32 padding = 0;
                    g_render_client->lpVtbl->GetCurrentPadding(g_render_client, &padding);
                    UINT32 bufferFrameCount = 0;
                    g_render_client->lpVtbl->GetBufferSize(g_render_client, &bufferFrameCount);

                    UINT32 availableSpace = bufferFrameCount - padding;
                    uint64_t buffered_frames = g_output_frames_written - g_output_frames_read;

                    // Ajusta drift caso o atraso seja superior a 100ms
                    if (buffered_frames > outRate * 0.1) {
                        g_output_frames_read = g_output_frames_written - (uint64_t)(outRate * 0.03);
                        buffered_frames = g_output_frames_written - g_output_frames_read;
                    }
                    if (g_output_frames_written < g_output_frames_read) {
                        g_output_frames_read = g_output_frames_written;
                        buffered_frames = 0;
                    }

                    // Pre-buffering para evitar engasgos iniciais e subfluxo
                    UINT32 numFramesWrite = availableSpace;
                    if (g_prebuffering) {
                        if (buffered_frames >= (uint64_t)(outRate * 0.04)) { // 40ms
                            g_prebuffering = 0;
                        } else {
                            numFramesWrite = 0;
                        }
                    }

                    if (!g_prebuffering) {
                        // Limita a escrita ao que realmente temos no buffer circular para evitar silêncio artificial
                        if (numFramesWrite > buffered_frames) {
                            numFramesWrite = (UINT32)buffered_frames;
                        }
                    }

                    if (numFramesWrite > 0) {
                        BYTE* pRenderData = NULL;
                        hr = g_render_client_out->lpVtbl->GetBuffer(g_render_client_out, numFramesWrite, &pRenderData);
                        if (SUCCEEDED(hr)) {
                            for (UINT32 j = 0; j < numFramesWrite; j++) {
                                uint32_t out_ring_index = (uint32_t)((g_output_frames_read + j) % OUTPUT_RING_BUFFER_SIZE_FRAMES);
                                float left_out  = g_output_ring_buffer[out_ring_index * 2 + 0];
                                float right_out = g_output_ring_buffer[out_ring_index * 2 + 1];

                                for (WORD ch = 0; ch < outChannels; ch++) {
                                    float val = 0.0f;
                                    if (local_loopback) {
                                        if (ch == 0) val = left_out;
                                        else if (ch == 1) val = right_out;
                                    }
                                    writeSample(pRenderData, (int)(j * outChannels + ch), val);
                                }
                            }

                            g_output_frames_read += numFramesWrite;
                            g_render_client_out->lpVtbl->ReleaseBuffer(g_render_client_out, numFramesWrite, 0);
                        }
                    }
                } else {
                    // Sem render_client_out disponível, descarta os frames para não travar o buffer
                    g_output_frames_read = g_output_frames_written;
                }

                g_capture_client_in->lpVtbl->ReleaseBuffer(g_capture_client_in, numFramesRead);
            }

            hr = g_capture_client_in->lpVtbl->GetNextPacketSize(g_capture_client_in, &packetSize);
            if (FAILED(hr)) {
                break;
            }
        }
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

    // Inicializa captura em modo compartilhado com buffer de 20ms e suporte a eventos
    REFERENCE_TIME bufferDuration = 200000;
    hr = g_capture_client->lpVtbl->Initialize(
        g_capture_client, AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK, bufferDuration, 0, pFormatIn, NULL);
    CoTaskMemFree(pFormatIn);

    if (FAILED(hr)) {
        g_capture_client->lpVtbl->Release(g_capture_client); g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device); g_capture_device = NULL;
        return 0;
    }

    g_audio_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!g_audio_event) {
        g_capture_client->lpVtbl->Release(g_capture_client); g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device); g_capture_device = NULL;
        return 0;
    }

    hr = g_capture_client->lpVtbl->SetEventHandle(g_capture_client, g_audio_event);
    if (FAILED(hr)) {
        CloseHandle(g_audio_event); g_audio_event = NULL;
        g_capture_client->lpVtbl->Release(g_capture_client); g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device); g_capture_device = NULL;
        return 0;
    }

    hr = g_capture_client->lpVtbl->GetService(
        g_capture_client, &local_IID_IAudioCaptureClient,
        (void**)&g_capture_client_in);
    if (FAILED(hr)) {
        CloseHandle(g_audio_event); g_audio_event = NULL;
        g_capture_client->lpVtbl->Release(g_capture_client); g_capture_client = NULL;
        g_capture_device->lpVtbl->Release(g_capture_device); g_capture_device = NULL;
        return 0;
    }

    // Inicializa dispositivo de saída (sempre, para permitir ativação dinâmica do loopback)
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
    g_prebuffering       = 1;
    g_input_frames_written = 0;
    g_output_frames_written = 0;
    g_output_frames_read = 0;
    g_resampler_input_index = 0.0;
    memset(g_input_ring_buffer, 0, sizeof(g_input_ring_buffer));
    memset(g_output_ring_buffer, 0, sizeof(g_output_ring_buffer));

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
        CloseHandle(g_audio_event); g_audio_event = NULL;

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

    // Acorda a thread imediatamente se estiver travada no WaitForSingleObject
    if (g_audio_event) {
        SetEvent(g_audio_event);
    }

    if (g_audio_thread) {
        WaitForSingleObject(g_audio_thread, INFINITE);
        CloseHandle(g_audio_thread);
        g_audio_thread = NULL;
    }

    if (g_audio_event) {
        CloseHandle(g_audio_event);
        g_audio_event = NULL;
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
