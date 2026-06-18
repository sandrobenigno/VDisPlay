#include "include/video_capture.h"
#include "include/common.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Estado global de captura
// ---------------------------------------------------------------------------
static HANDLE         g_capture_thread  = NULL;
static volatile int   g_is_capturing    = 0;   // R3: volatile garante visibilidade entre threads
static FrameCallback  g_callback        = NULL;

static IMFMediaSource*  g_media_source  = NULL;
static IMFSourceReader* g_source_reader = NULL;

static CRITICAL_SECTION g_frame_cs;
static uint8_t* g_last_frame_buffer = NULL;
static int      g_last_frame_size   = 0;
static int      g_width             = 0;
static int      g_height            = 0;
static int      g_brightness_offset = 0;

// ---------------------------------------------------------------------------
// Cache de modos de mídia (P2/P5)
// Populado uma única vez em start_video_capture; reutilizado por todas as
// consultas subsequentes sem chamar a API COM repetidamente.
// ---------------------------------------------------------------------------
typedef struct {
    int    width;
    int    height;
    double fps;
} MediaMode;

#define MAX_MEDIA_MODES 256

static MediaMode g_media_modes[MAX_MEDIA_MODES];
static int       g_media_modes_count = 0;

// Resoluções únicas (subconjunto do cache acima, para a UI)
static int g_unique_resolutions[100][2]; // [i][0]=w, [i][1]=h
static int g_unique_resolutions_count = 0;

// ---------------------------------------------------------------------------
// GUIDs WIC locais (evita dependências de linker)
// ---------------------------------------------------------------------------
static const CLSID local_CLSID_WICImagingFactory  =
    {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const IID   local_IID_IWICImagingFactory   =
    {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID  local_GUID_ContainerFormatPng  =
    {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
static const GUID  local_GUID_WICPixelFormat32bppRGBA =
    {0xf5c7ad2d, 0x6a8d, 0x43dd, {0xa7, 0xa8, 0xa2, 0x99, 0x35, 0x26, 0x1a, 0xe9}};
static const GUID  local_GUID_WICPixelFormat32bppBGRA =
    {0x6fddc7eb, 0x4e17, 0x45d5, {0xa3, 0x80, 0xdd, 0x30, 0xfd, 0x36, 0xdb, 0xcd}};

#ifndef MF_E_NO_MORE_TYPES
#define MF_E_NO_MORE_TYPES ((HRESULT)0xC00D36B9)
#endif

// ---------------------------------------------------------------------------
// Cria IMFMediaSource para o device_id fornecido
// ---------------------------------------------------------------------------
static HRESULT create_media_source(const char* device_id, IMFMediaSource** ppSource) {
    IMFAttributes* pAttributes = NULL;
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

    hr = pAttributes->lpVtbl->SetGUID(pAttributes,
        &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        pAttributes->lpVtbl->Release(pAttributes);
        return hr;
    }

    IMFActivate** ppDevices = NULL;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    pAttributes->lpVtbl->Release(pAttributes);
    if (FAILED(hr)) return hr;

    IMFActivate* pFoundDevice = NULL;
    for (UINT32 i = 0; i < count; i++) {
        IMFActivate* pDevice = ppDevices[i];
        if (!pDevice) continue;

        WCHAR* symLink = NULL;
        UINT32 symLinkLen = 0;
        hr = pDevice->lpVtbl->GetAllocatedString(pDevice,
            &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            &symLink, &symLinkLen);

        if (SUCCEEDED(hr) && symLink) {
            char utf8_link[512];
            wchar_to_utf8(symLink, utf8_link, sizeof(utf8_link));
            CoTaskMemFree(symLink);

            if (strcmp(utf8_link, device_id) == 0) {
                pFoundDevice = pDevice;
                pFoundDevice->lpVtbl->AddRef(pFoundDevice);
            }
        }
        pDevice->lpVtbl->Release(pDevice);
    }
    CoTaskMemFree(ppDevices);

    if (!pFoundDevice) return E_FAIL;
    hr = pFoundDevice->lpVtbl->ActivateObject(
        pFoundDevice, &IID_IMFMediaSource, (void**)ppSource);
    pFoundDevice->lpVtbl->Release(pFoundDevice);
    return hr;
}

// ---------------------------------------------------------------------------
// Thread de captura
// ---------------------------------------------------------------------------
static DWORD WINAPI capture_thread_proc(LPVOID lpParam) {
    (void)lpParam;

    // Inicializa COM nesta thread worker (R9)
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    IMFSample* pSample    = NULL;
    DWORD      streamIndex, flags;
    LONGLONG   timestamp;

    while (g_is_capturing) {
        HRESULT hr = g_source_reader->lpVtbl->ReadSample(
            g_source_reader,
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            &streamIndex, &flags, &timestamp, &pSample);

        if (FAILED(hr)) {
            Sleep(10);
            continue;
        }

        if (flags & MF_SOURCE_READERF_STREAMTICK) {
            continue;
        }

        if (pSample) {
            IMFMediaBuffer* pBuffer = NULL;
            hr = pSample->lpVtbl->ConvertToContiguousBuffer(pSample, &pBuffer);
            if (SUCCEEDED(hr)) {
                BYTE*  pData        = NULL;
                DWORD  maxLength    = 0;
                DWORD  currentLength = 0;
                hr = pBuffer->lpVtbl->Lock(pBuffer, &pData, &maxLength, &currentLength);
                if (SUCCEEDED(hr)) {

                    EnterCriticalSection(&g_frame_cs);

                    // R2: verifica retorno do realloc antes de usar o ponteiro
                    if (!g_last_frame_buffer || g_last_frame_size != (int)currentLength) {
                        uint8_t* new_buf = (uint8_t*)realloc(g_last_frame_buffer, currentLength);
                        if (!new_buf) {
                            // Sem memória: pula este frame sem alterar o estado
                            LeaveCriticalSection(&g_frame_cs);
                            pBuffer->lpVtbl->Unlock(pBuffer);
                            pBuffer->lpVtbl->Release(pBuffer);
                            pSample->lpVtbl->Release(pSample);
                            pSample = NULL;
                            continue;
                        }
                        g_last_frame_buffer = new_buf;
                        g_last_frame_size   = (int)currentLength;
                    }

                    // P1: conversão BGRA→RGBA com operação de 32 bits (1 leitura + 1 escrita/pixel)
                    // Troca apenas os bytes R (pos 16) e B (pos 0) mantendo G e A intactos.
                    int local_offset = g_brightness_offset;
                    DWORD n_pixels   = currentLength / 4;

                    if (local_offset != 0) {
                        // Com ajuste de brilho: precisa operar byte a byte
                        const uint8_t* src = pData;
                        uint8_t*       dst = g_last_frame_buffer;
                        for (DWORD i = 0; i < n_pixels; i++) {
                            int b = src[0] + local_offset;
                            int g = src[1] + local_offset;
                            int r = src[2] + local_offset;
                            dst[0] = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r)); // R
                            dst[1] = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g)); // G
                            dst[2] = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b)); // B
                            dst[3] = 255;                                        // A (força totalmente opaco)
                            src += 4; dst += 4;
                        }
                    } else {
                        // Sem ajuste de brilho: swap de 32 bits (R↔B e força Alpha = 255)
                        const uint32_t* src32 = (const uint32_t*)pData;
                        uint32_t*       dst32 = (uint32_t*)g_last_frame_buffer;
                        for (DWORD i = 0; i < n_pixels; i++) {
                            uint32_t px = src32[i];
                            // BGRA (0xXXRRGGBB) → RGBA (0xFFBBGGRR) com Alpha forçado a 0xFF (opaco)
                            dst32[i] = 0xFF000000u
                                     | (px & 0x0000FF00u)
                                     | ((px >> 16) & 0x000000FFu)
                                     | ((px & 0x000000FFu) << 16);
                        }
                    }

                    // R1: callback invocada DENTRO da critical section para garantir
                    // que o buffer não seja liberado enquanto o Flutter o lê.
                    if (g_callback && g_last_frame_buffer) {
                        g_callback(g_last_frame_buffer, g_last_frame_size, g_width, g_height);
                    }

                    LeaveCriticalSection(&g_frame_cs);
                    pBuffer->lpVtbl->Unlock(pBuffer);
                }
                pBuffer->lpVtbl->Release(pBuffer);
            }
            pSample->lpVtbl->Release(pSample);
            pSample = NULL;
        } else {
            // Nenhum frame disponível — dorme brevemente para não queimar CPU
            Sleep(1);
        }
    }

    CoUninitialize();
    return 0;
}

// ---------------------------------------------------------------------------
// Popula g_media_modes[] e g_unique_resolutions[] em uma única passagem (P5)
// ---------------------------------------------------------------------------
static void populate_media_modes() {
    g_media_modes_count        = 0;
    g_unique_resolutions_count = 0;
    if (!g_source_reader) return;

    for (DWORD i = 0; ; i++) {
        IMFMediaType* pType = NULL;
        HRESULT hr = g_source_reader->lpVtbl->GetNativeMediaType(
            g_source_reader,
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            i, &pType);
        if (hr == MF_E_NO_MORE_TYPES || FAILED(hr)) break;

        UINT64 frameSize = 0;
        UINT32 w = 0, h = 0;
        if (SUCCEEDED(pType->lpVtbl->GetUINT64(pType, &MF_MT_FRAME_SIZE, &frameSize))) {
            w = (UINT32)(frameSize >> 32);
            h = (UINT32)frameSize;
        }

        UINT64 frameRate = 0;
        double fps = 30.0;
        if (SUCCEEDED(pType->lpVtbl->GetUINT64(pType, &MF_MT_FRAME_RATE, &frameRate))) {
            UINT32 num = (UINT32)(frameRate >> 32);
            UINT32 den = (UINT32)frameRate;
            if (den > 0) fps = (double)num / den;
        }
        pType->lpVtbl->Release(pType);

        if (w == 0 || h == 0) continue;

        // Adiciona ao cache de modos completo
        if (g_media_modes_count < MAX_MEDIA_MODES) {
            g_media_modes[g_media_modes_count].width  = (int)w;
            g_media_modes[g_media_modes_count].height = (int)h;
            g_media_modes[g_media_modes_count].fps    = fps;
            g_media_modes_count++;
        }

        // Adiciona à lista de resoluções únicas (para a UI)
        int found = 0;
        for (int j = 0; j < g_unique_resolutions_count; j++) {
            if (g_unique_resolutions[j][0] == (int)w &&
                g_unique_resolutions[j][1] == (int)h) {
                found = 1; break;
            }
        }
        if (!found && g_unique_resolutions_count < 100) {
            g_unique_resolutions[g_unique_resolutions_count][0] = (int)w;
            g_unique_resolutions[g_unique_resolutions_count][1] = (int)h;
            g_unique_resolutions_count++;
        }
    }

    // Ordena resoluções únicas por área decrescente (maior primeiro)
    for (int a = 0; a < g_unique_resolutions_count - 1; a++) {
        for (int b = a + 1; b < g_unique_resolutions_count; b++) {
            int areaA = g_unique_resolutions[a][0] * g_unique_resolutions[a][1];
            int areaB = g_unique_resolutions[b][0] * g_unique_resolutions[b][1];
            if (areaB > areaA) {
                int tw = g_unique_resolutions[a][0], th = g_unique_resolutions[a][1];
                g_unique_resolutions[a][0] = g_unique_resolutions[b][0];
                g_unique_resolutions[a][1] = g_unique_resolutions[b][1];
                g_unique_resolutions[b][0] = tw;
                g_unique_resolutions[b][1] = th;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Inicia a captura de vídeo
// ---------------------------------------------------------------------------
EXPORT int start_video_capture(const char* device_id, FrameCallback callback) {
    if (g_is_capturing) {
        stop_video_capture();
    }

    HRESULT hr = create_media_source(device_id, &g_media_source);
    if (FAILED(hr)) return 0;

    IMFAttributes* pAttributes = NULL;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (SUCCEEDED(hr)) {
        pAttributes->lpVtbl->SetUINT32(
            pAttributes, &MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    }

    hr = MFCreateSourceReaderFromMediaSource(g_media_source, pAttributes, &g_source_reader);
    if (pAttributes) pAttributes->lpVtbl->Release(pAttributes);

    if (FAILED(hr)) {
        g_media_source->lpVtbl->Release(g_media_source);
        g_media_source = NULL;
        return 0;
    }

    // P5: popula o cache de modos em uma única passagem (inclui resoluções únicas)
    populate_media_modes();

    // Seleciona o melhor modo nativo (maior área, maior fps) usando o cache
    int    best_idx  = -1;
    double best_fps  = -1.0;
    int    best_area = 0;
    for (int i = 0; i < g_media_modes_count; i++) {
        int area = g_media_modes[i].width * g_media_modes[i].height;
        if (area > best_area || (area == best_area && g_media_modes[i].fps > best_fps)) {
            best_area = area;
            best_fps  = g_media_modes[i].fps;
            best_idx  = i;
        }
    }

    if (best_idx >= 0) {
        // Aplica o modo nativo selecionado via GetNativeMediaType pelo mesmo índice
        IMFMediaType* pBestNativeType = NULL;
        DWORD modeIdx = (DWORD)best_idx;
        if (SUCCEEDED(g_source_reader->lpVtbl->GetNativeMediaType(g_source_reader,
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, modeIdx, &pBestNativeType))) {
            g_source_reader->lpVtbl->SetCurrentMediaType(g_source_reader,
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pBestNativeType);
            pBestNativeType->lpVtbl->Release(pBestNativeType);
        }
    }

    // Configura formato de saída do Source Reader para RGB32 (BGRA 32 bits)
    IMFMediaType* pTargetType = NULL;
    hr = MFCreateMediaType(&pTargetType);
    if (SUCCEEDED(hr)) {
        pTargetType->lpVtbl->SetGUID(pTargetType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
        pTargetType->lpVtbl->SetGUID(pTargetType, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
        hr = g_source_reader->lpVtbl->SetCurrentMediaType(g_source_reader,
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pTargetType);
        pTargetType->lpVtbl->Release(pTargetType);
    }

    if (FAILED(hr)) {
        g_source_reader->lpVtbl->Release(g_source_reader); g_source_reader = NULL;
        g_media_source->lpVtbl->Release(g_media_source);   g_media_source  = NULL;
        return 0;
    }

    // Obtém dimensões reais após negociação de formato
    IMFMediaType* pCurrentType = NULL;
    hr = g_source_reader->lpVtbl->GetCurrentMediaType(
        g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
    if (SUCCEEDED(hr)) {
        UINT64 frameSize = 0;
        if (SUCCEEDED(pCurrentType->lpVtbl->GetUINT64(
                pCurrentType, &MF_MT_FRAME_SIZE, &frameSize))) {
            g_width  = (int)(UINT32)(frameSize >> 32);
            g_height = (int)(UINT32)frameSize;
        }
        pCurrentType->lpVtbl->Release(pCurrentType);
    } else {
        g_width  = 640;
        g_height = 480;
    }

    g_callback    = callback;
    g_is_capturing = 1;

    g_capture_thread = CreateThread(NULL, 0, capture_thread_proc, NULL, 0, NULL);
    if (!g_capture_thread) {
        // R5: reverte o flag se a thread não foi criada
        g_is_capturing = 0;
        g_source_reader->lpVtbl->Release(g_source_reader); g_source_reader = NULL;
        g_media_source->lpVtbl->Release(g_media_source);   g_media_source  = NULL;
        return 0;
    }

    return 1;
}

// ---------------------------------------------------------------------------
// Para a captura de vídeo
// ---------------------------------------------------------------------------
EXPORT void stop_video_capture() {
    if (!g_is_capturing) return;

    g_is_capturing = 0;

    if (g_capture_thread) {
        WaitForSingleObject(g_capture_thread, INFINITE);
        CloseHandle(g_capture_thread);
        g_capture_thread = NULL;
    }

    if (g_source_reader) {
        g_source_reader->lpVtbl->Release(g_source_reader);
        g_source_reader = NULL;
    }
    if (g_media_source) {
        g_media_source->lpVtbl->Release(g_media_source);
        g_media_source = NULL;
    }

    EnterCriticalSection(&g_frame_cs);
    if (g_last_frame_buffer) {
        free(g_last_frame_buffer);
        g_last_frame_buffer = NULL;
    }
    g_last_frame_size = 0;
    LeaveCriticalSection(&g_frame_cs);
}

EXPORT int is_video_capturing() {
    return g_is_capturing;
}

EXPORT void set_brightness_offset(int offset) {
    EnterCriticalSection(&g_frame_cs);
    g_brightness_offset = offset;
    LeaveCriticalSection(&g_frame_cs);
}

// ---------------------------------------------------------------------------
// Screenshot — salva o último frame como PNG
// ---------------------------------------------------------------------------
EXPORT int save_screenshot(const char* filepath) {
    int success = 0;
    EnterCriticalSection(&g_frame_cs);
    if (!g_last_frame_buffer || g_width <= 0 || g_height <= 0) {
        LeaveCriticalSection(&g_frame_cs);
        return 0;
    }

    int w           = g_width;
    int h           = g_height;
    int stride      = w * 4;
    int buffer_size = g_last_frame_size;

    // R4: verifica retorno do malloc antes de usar
    uint8_t* temp_buf = (uint8_t*)malloc(buffer_size);
    if (!temp_buf) {
        LeaveCriticalSection(&g_frame_cs);
        return 0;
    }
    memcpy(temp_buf, g_last_frame_buffer, buffer_size);
    LeaveCriticalSection(&g_frame_cs);

    // Converte de RGBA (usado pelo Flutter) de volta para BGRA para gravação correta via WIC
    uint32_t* buf32 = (uint32_t*)temp_buf;
    int n_pixels = buffer_size / 4;
    for (int i = 0; i < n_pixels; i++) {
        uint32_t px = buf32[i];
        // RGBA (0xFFBBGGRR) -> BGRA (0xFFRRGGBB)
        buf32[i] = (px & 0xFF00FF00u)
                 | ((px >> 16) & 0x000000FFu)
                 | ((px & 0x000000FFu) << 16);
    }

    // Grava PNG via WIC
    IWICImagingFactory* pFactory = NULL;
    HRESULT hr = CoCreateInstance(&local_CLSID_WICImagingFactory, NULL,
        CLSCTX_INPROC_SERVER, &local_IID_IWICImagingFactory, (void**)&pFactory);
    if (SUCCEEDED(hr)) {
        IWICStream* pStream = NULL;
        hr = pFactory->lpVtbl->CreateStream(pFactory, &pStream);
        if (SUCCEEDED(hr)) {
            WCHAR wpath[MAX_PATH];
            utf8_to_wchar(filepath, wpath, MAX_PATH);
            hr = pStream->lpVtbl->InitializeFromFilename(pStream, wpath, GENERIC_WRITE);
            if (SUCCEEDED(hr)) {
                IWICBitmapEncoder* pEncoder = NULL;
                hr = pFactory->lpVtbl->CreateEncoder(
                    pFactory, &local_GUID_ContainerFormatPng, NULL, &pEncoder);
                if (SUCCEEDED(hr)) {
                    hr = pEncoder->lpVtbl->Initialize(
                        pEncoder, (IStream*)pStream, WICBitmapEncoderNoCache);
                    if (SUCCEEDED(hr)) {
                        IWICBitmapFrameEncode* pFrame = NULL;
                        hr = pEncoder->lpVtbl->CreateNewFrame(pEncoder, &pFrame, NULL);
                        if (SUCCEEDED(hr)) {
                            hr = pFrame->lpVtbl->Initialize(pFrame, NULL);
                            if (SUCCEEDED(hr)) hr = pFrame->lpVtbl->SetSize(pFrame, w, h);
                            if (SUCCEEDED(hr)) {
                                WICPixelFormatGUID fmt = local_GUID_WICPixelFormat32bppBGRA;
                                hr = pFrame->lpVtbl->SetPixelFormat(pFrame, &fmt);
                            }
                            if (SUCCEEDED(hr))
                                hr = pFrame->lpVtbl->WritePixels(
                                    pFrame, h, stride, buffer_size, temp_buf);
                            if (SUCCEEDED(hr)) hr = pFrame->lpVtbl->Commit(pFrame);
                            if (SUCCEEDED(hr)) hr = pEncoder->lpVtbl->Commit(pEncoder);
                            if (SUCCEEDED(hr)) success = 1;
                            pFrame->lpVtbl->Release(pFrame);
                        }
                    }
                    pEncoder->lpVtbl->Release(pEncoder);
                }
            }
            pStream->lpVtbl->Release(pStream);
        }
        pFactory->lpVtbl->Release(pFactory);
    }

    free(temp_buf);
    return success;
}

// ---------------------------------------------------------------------------
// Getters de dimensão
// ---------------------------------------------------------------------------
EXPORT int get_video_width() {
    int w;
    EnterCriticalSection(&g_frame_cs);
    w = g_width;
    LeaveCriticalSection(&g_frame_cs);
    return w;
}

EXPORT int get_video_height() {
    int h;
    EnterCriticalSection(&g_frame_cs);
    h = g_height;
    LeaveCriticalSection(&g_frame_cs);
    return h;
}

// ---------------------------------------------------------------------------
// API de resoluções disponíveis — usa o cache (P2)
// ---------------------------------------------------------------------------
EXPORT int get_available_resolutions_count() {
    return g_unique_resolutions_count;
}

EXPORT int get_available_resolution_width(int index) {
    if (index < 0 || index >= g_unique_resolutions_count) return 0;
    return g_unique_resolutions[index][0];
}

EXPORT int get_available_resolution_height(int index) {
    if (index < 0 || index >= g_unique_resolutions_count) return 0;
    return g_unique_resolutions[index][1];
}

// P2: verifica suporte de FPS no cache local — zero chamadas COM
EXPORT int is_fps_supported(int width, int height, int target_fps) {
    for (int i = 0; i < g_media_modes_count; i++) {
        if (g_media_modes[i].width != width || g_media_modes[i].height != height) continue;
        double fps = g_media_modes[i].fps;
        if (target_fps == 30 && fps >= 29.0 && fps <= 31.0) return 1;
        if (target_fps == 60 && fps >= 58.0 && fps <= 62.0) return 1;
    }
    return 0;
}

EXPORT double get_current_fps() {
    if (!g_source_reader) return 0.0;
    double fps = 0.0;
    IMFMediaType* pType = NULL;
    HRESULT hr = g_source_reader->lpVtbl->GetCurrentMediaType(
        g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pType);
    if (SUCCEEDED(hr)) {
        UINT64 frameRate = 0;
        if (SUCCEEDED(pType->lpVtbl->GetUINT64(pType, &MF_MT_FRAME_RATE, &frameRate))) {
            UINT32 num = (UINT32)(frameRate >> 32);
            UINT32 den = (UINT32)frameRate;
            if (den > 0) fps = (double)num / den;
        }
        pType->lpVtbl->Release(pType);
    }
    return fps;
}

// ---------------------------------------------------------------------------
// Altera resolução e FPS em tempo de execução
// ---------------------------------------------------------------------------
EXPORT int set_video_resolution_and_fps(int width, int height, int fps_preference) {
    if (!g_source_reader) return 0;

    int was_capturing = g_is_capturing;
    g_is_capturing = 0;
    if (g_capture_thread) {
        WaitForSingleObject(g_capture_thread, INFINITE);
        CloseHandle(g_capture_thread);
        g_capture_thread = NULL;
    }

    // P2: seleciona o melhor índice no cache — zero chamadas COM nesta fase
    int    best_idx  = -1;
    double best_fps  = -1.0;
    for (int i = 0; i < g_media_modes_count; i++) {
        if (g_media_modes[i].width != width || g_media_modes[i].height != height) continue;
        double fps = g_media_modes[i].fps;

        if (fps_preference == 30 && fps >= 29.0 && fps <= 31.0) {
            best_idx = i; best_fps = fps; break;
        }
        if (fps_preference == 60 && fps >= 58.0 && fps <= 62.0) {
            best_idx = i; best_fps = fps; break;
        }
        if (fps > best_fps) { best_fps = fps; best_idx = i; }
    }

    int success = 0;
    if (best_idx >= 0) {
        // Aplica o modo via GetNativeMediaType com o índice do cache
        IMFMediaType* pBestType = NULL;
        HRESULT hr = g_source_reader->lpVtbl->GetNativeMediaType(
            g_source_reader,
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            (DWORD)best_idx, &pBestType);

        if (SUCCEEDED(hr)) {
            hr = g_source_reader->lpVtbl->SetCurrentMediaType(
                g_source_reader,
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                NULL, pBestType);
            pBestType->lpVtbl->Release(pBestType);

            if (SUCCEEDED(hr)) {
                // Reaplica conversão para RGB32
                IMFMediaType* pTargetType = NULL;
                hr = MFCreateMediaType(&pTargetType);
                if (SUCCEEDED(hr)) {
                    pTargetType->lpVtbl->SetGUID(
                        pTargetType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
                    pTargetType->lpVtbl->SetGUID(
                        pTargetType, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
                    hr = g_source_reader->lpVtbl->SetCurrentMediaType(
                        g_source_reader,
                        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                        NULL, pTargetType);
                    pTargetType->lpVtbl->Release(pTargetType);
                }

                if (SUCCEEDED(hr)) {
                    EnterCriticalSection(&g_frame_cs);
                    g_width  = width;
                    g_height = height;
                    LeaveCriticalSection(&g_frame_cs);
                    success = 1;
                }
            }
        }
    }

    // R5: só reativa a flag e cria a thread se CreateThread tiver sucesso
    if (was_capturing) {
        g_capture_thread = CreateThread(NULL, 0, capture_thread_proc, NULL, 0, NULL);
        if (g_capture_thread) {
            g_is_capturing = 1;
        } else {
            // Falha ao recriar thread: captura fica parada, retorna falha
            success = 0;
        }
    }

    return success;
}

// ---------------------------------------------------------------------------
// Inicialização / finalização do backend
// ---------------------------------------------------------------------------
void init_video_capture_backend() {
    InitializeCriticalSection(&g_frame_cs);
}

void shutdown_video_capture_backend() {
    stop_video_capture();
    DeleteCriticalSection(&g_frame_cs);
}
