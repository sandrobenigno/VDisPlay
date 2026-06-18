#include "include/video_capture.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <stdio.h>

static HANDLE g_capture_thread = NULL;
static int g_is_capturing = 0;
static FrameCallback g_callback = NULL;

static IMFMediaSource* g_media_source = NULL;
static IMFSourceReader* g_source_reader = NULL;

static CRITICAL_SECTION g_frame_cs;
static uint8_t* g_last_frame_buffer = NULL;
static int g_last_frame_size = 0;
static int g_width = 0;
static int g_height = 0;
static int g_brightness_offset = 0;

typedef struct {
    int width;
    int height;
} Resolution;

static Resolution g_unique_resolutions[100];
static int g_unique_resolutions_count = 0;

static void wchar_to_utf8(const WCHAR* src, char* dst, int dst_len) {
    if (!src || !dst || dst_len <= 0) return;
    WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dst_len, NULL, NULL);
    dst[dst_len - 1] = '\0';
}

static void utf8_to_wchar(const char* src, WCHAR* dst, int dst_len) {
    if (!src || !dst || dst_len <= 0) return;
    MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst_len);
    dst[dst_len - 1] = L'\0';
}

// Local CLSIDs e IIDs para evitar dependências de linker
static const CLSID local_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0xa}};
static const IID local_IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0x47, 0x0}};
static const GUID local_GUID_ContainerFormatPng = {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
static const GUID local_GUID_WICPixelFormat32bppBGRA = {0x6fddc7eb, 0x4e17, 0x45d5, {0xa3, 0x80, 0xdd, 0x30, 0xfd, 0x36, 0xdb, 0xcd}};
static const GUID local_GUID_WICPixelFormat32bppRGBA = {0xf5c7ad2d, 0x6a8d, 0x43dd, {0xa7, 0xa8, 0xa2, 0x99, 0x35, 0x26, 0x1a, 0xe9}};

#ifndef MF_E_NO_MORE_TYPES
#define MF_E_NO_MORE_TYPES ((HRESULT)0xC00D36B9)
#endif

static HRESULT create_media_source(const char* device_id, IMFMediaSource** ppSource) {
    IMFAttributes* pAttributes = NULL;
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

    hr = pAttributes->lpVtbl->SetGUID(pAttributes, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
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
        hr = pDevice->lpVtbl->GetAllocatedString(pDevice, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &symLink, &symLinkLen);
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

    hr = pFoundDevice->lpVtbl->ActivateObject(pFoundDevice, &IID_IMFMediaSource, (void**)ppSource);
    pFoundDevice->lpVtbl->Release(pFoundDevice);
    return hr;
}

static DWORD WINAPI capture_thread_proc(LPVOID lpParam) {
    HRESULT hr;
    IMFSample* pSample = NULL;
    DWORD streamIndex, flags;
    LONGLONG timestamp;

    while (g_is_capturing) {
        hr = g_source_reader->lpVtbl->ReadSample(
            g_source_reader,
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            &streamIndex,
            &flags,
            &timestamp,
            &pSample
        );

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
                BYTE* pData = NULL;
                DWORD maxLength = 0, currentLength = 0;
                hr = pBuffer->lpVtbl->Lock(pBuffer, &pData, &maxLength, &currentLength);
                if (SUCCEEDED(hr)) {
                    
                    // Aloca ou redimensiona o buffer compartilhado
                    EnterCriticalSection(&g_frame_cs);
                    if (!g_last_frame_buffer || g_last_frame_size != (int)currentLength) {
                        g_last_frame_buffer = (uint8_t*)realloc(g_last_frame_buffer, currentLength);
                        g_last_frame_size = (int)currentLength;
                    }
                    
                    // Aplica ajuste de brilho e copia para o buffer da captura (convertendo de BGRA para RGBA)
                    int local_offset = g_brightness_offset;
                    if (local_offset != 0) {
                        for (DWORD i = 0; i < currentLength; i += 4) {
                            int b = pData[i] + local_offset;
                            int g = pData[i+1] + local_offset;
                            int r = pData[i+2] + local_offset;

                            g_last_frame_buffer[i]   = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r)); // R
                            g_last_frame_buffer[i+1] = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g)); // G
                            g_last_frame_buffer[i+2] = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b)); // B
                            g_last_frame_buffer[i+3] = pData[i+3]; // A
                        }
                    } else {
                        for (DWORD i = 0; i < currentLength; i += 4) {
                            g_last_frame_buffer[i]   = pData[i+2]; // R
                            g_last_frame_buffer[i+1] = pData[i+1]; // G
                            g_last_frame_buffer[i+2] = pData[i];   // B
                            g_last_frame_buffer[i+3] = pData[i+3]; // A
                        }
                    }

                    int w = g_width;
                    int h = g_height;
                    FrameCallback cb = g_callback;
                    LeaveCriticalSection(&g_frame_cs);

                    // Executa a callback enviando os dados
                    if (cb) {
                        cb(g_last_frame_buffer, g_last_frame_size, w, h);
                    }

                    pBuffer->lpVtbl->Unlock(pBuffer);
                }
                pBuffer->lpVtbl->Release(pBuffer);
            }
            pSample->lpVtbl->Release(pSample);
            pSample = NULL;
        } else {
            // Dorme um pouco para não consumir 100% de CPU caso não haja frames
            Sleep(1);
        }
    }

    return 0;
}

static int compare_resolutions(const void* a, const void* b) {
    Resolution* resA = (Resolution*)a;
    Resolution* resB = (Resolution*)b;
    int areaA = resA->width * resA->height;
    int areaB = resB->width * resB->height;
    if (areaA != areaB) {
        return areaB - areaA; // Descending order of area
    }
    return resB->width - resA->width; // Descending order of width
}

static void populate_unique_resolutions() {
    g_unique_resolutions_count = 0;
    if (!g_source_reader) return;

    for (DWORD i = 0; ; i++) {
        IMFMediaType* pType = NULL;
        HRESULT hr = g_source_reader->lpVtbl->GetNativeMediaType(g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pType);
        if (hr == MF_E_NO_MORE_TYPES || FAILED(hr)) {
            break;
        }

        UINT64 frameSize = 0;
        UINT32 w = 0, h = 0;
        if (SUCCEEDED(pType->lpVtbl->GetUINT64(pType, &MF_MT_FRAME_SIZE, &frameSize))) {
            w = (UINT32)(frameSize >> 32);
            h = (UINT32)frameSize;
        }
        pType->lpVtbl->Release(pType);

        if (w > 0 && h > 0) {
            int found = 0;
            for (int j = 0; j < g_unique_resolutions_count; j++) {
                if (g_unique_resolutions[j].width == (int)w && g_unique_resolutions[j].height == (int)h) {
                    found = 1;
                    break;
                }
            }
            if (!found && g_unique_resolutions_count < 100) {
                g_unique_resolutions[g_unique_resolutions_count].width = (int)w;
                g_unique_resolutions[g_unique_resolutions_count].height = (int)h;
                g_unique_resolutions_count++;
            }
        }
    }

    if (g_unique_resolutions_count > 1) {
        qsort(g_unique_resolutions, g_unique_resolutions_count, sizeof(Resolution), compare_resolutions);
    }
}

EXPORT int start_video_capture(const char* device_id, FrameCallback callback) {
    if (g_is_capturing) {
        stop_video_capture();
    }

    HRESULT hr = create_media_source(device_id, &g_media_source);
    if (FAILED(hr)) return 0;

    IMFAttributes* pAttributes = NULL;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (SUCCEEDED(hr)) {
        pAttributes->lpVtbl->SetUINT32(pAttributes, &MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    }

    hr = MFCreateSourceReaderFromMediaSource(g_media_source, pAttributes, &g_source_reader);
    if (pAttributes) pAttributes->lpVtbl->Release(pAttributes);

    if (FAILED(hr)) {
        g_media_source->lpVtbl->Release(g_media_source);
        g_media_source = NULL;
        return 0;
    }

    populate_unique_resolutions();

    // Busca e seleciona a melhor resolução e framerate nativos da fonte física
    IMFMediaType* pBestNativeType = NULL;
    UINT32 max_width = 0;
    UINT32 max_height = 0;
    double max_fps = 0.0;

    for (DWORD i = 0; ; i++) {
        IMFMediaType* pType = NULL;
        HRESULT hr_enum = g_source_reader->lpVtbl->GetNativeMediaType(g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pType);
        if (hr_enum == MF_E_NO_MORE_TYPES || FAILED(hr_enum)) {
            break;
        }

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
            if (den > 0) {
                fps = (double)num / den;
            }
        }

        if (w * h > max_width * max_height || (w * h == max_width * max_height && fps > max_fps)) {
            max_width = w;
            max_height = h;
            max_fps = fps;
            if (pBestNativeType) {
                pBestNativeType->lpVtbl->Release(pBestNativeType);
            }
            pBestNativeType = pType;
        } else {
            pType->lpVtbl->Release(pType);
        }
    }

    if (pBestNativeType) {
        g_source_reader->lpVtbl->SetCurrentMediaType(g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pBestNativeType);
        pBestNativeType->lpVtbl->Release(pBestNativeType);
    }

    // Configura o formato de saída do Source Reader para RGB32 (BGRA de 32 bits)
    IMFMediaType* pTargetType = NULL;
    hr = MFCreateMediaType(&pTargetType);
    if (SUCCEEDED(hr)) {
        pTargetType->lpVtbl->SetGUID(pTargetType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
        pTargetType->lpVtbl->SetGUID(pTargetType, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
        hr = g_source_reader->lpVtbl->SetCurrentMediaType(g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pTargetType);
        pTargetType->lpVtbl->Release(pTargetType);
    }

    if (FAILED(hr)) {
        g_source_reader->lpVtbl->Release(g_source_reader);
        g_source_reader = NULL;
        g_media_source->lpVtbl->Release(g_media_source);
        g_media_source = NULL;
        return 0;
    }

    // Obtém dimensões reais da captura
    IMFMediaType* pCurrentType = NULL;
    hr = g_source_reader->lpVtbl->GetCurrentMediaType(g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
    if (SUCCEEDED(hr)) {
        UINT32 w = 0, h = 0;
        UINT64 frameSize = 0;
        hr = pCurrentType->lpVtbl->GetUINT64(pCurrentType, &MF_MT_FRAME_SIZE, &frameSize);
        if (SUCCEEDED(hr)) {
            w = (UINT32)(frameSize >> 32);
            h = (UINT32)frameSize;
        }
        g_width = (int)w;
        g_height = (int)h;
        pCurrentType->lpVtbl->Release(pCurrentType);
    } else {
        g_width = 640;
        g_height = 480;
    }

    g_callback = callback;
    g_is_capturing = 1;

    g_capture_thread = CreateThread(NULL, 0, capture_thread_proc, NULL, 0, NULL);
    if (!g_capture_thread) {
        g_is_capturing = 0;
        g_source_reader->lpVtbl->Release(g_source_reader);
        g_source_reader = NULL;
        g_media_source->lpVtbl->Release(g_media_source);
        g_media_source = NULL;
        return 0;
    }

    return 1;
}

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

EXPORT int save_screenshot(const char* filepath) {
    int success = 0;
    EnterCriticalSection(&g_frame_cs);
    if (!g_last_frame_buffer || g_width <= 0 || g_height <= 0) {
        LeaveCriticalSection(&g_frame_cs);
        return 0;
    }

    // Copia o buffer de forma rápida para liberar a critical section logo
    int w = g_width;
    int h = g_height;
    int stride = w * 4;
    int buffer_size = g_last_frame_size;
    uint8_t* temp_buf = (uint8_t*)malloc(buffer_size);
    memcpy(temp_buf, g_last_frame_buffer, buffer_size);
    LeaveCriticalSection(&g_frame_cs);

    // Inicialização do WIC para gravação de PNG
    IWICImagingFactory* pFactory = NULL;
    HRESULT hr = CoCreateInstance(&local_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &local_IID_IWICImagingFactory, (void**)&pFactory);
    if (SUCCEEDED(hr)) {
        IWICStream* pStream = NULL;
        hr = pFactory->lpVtbl->CreateStream(pFactory, &pStream);
        if (SUCCEEDED(hr)) {
            WCHAR wpath[MAX_PATH];
            utf8_to_wchar(filepath, wpath, MAX_PATH);

            hr = pStream->lpVtbl->InitializeFromFilename(pStream, wpath, GENERIC_WRITE);
            if (SUCCEEDED(hr)) {
                IWICBitmapEncoder* pEncoder = NULL;
                hr = pFactory->lpVtbl->CreateEncoder(pFactory, &local_GUID_ContainerFormatPng, NULL, &pEncoder);
                if (SUCCEEDED(hr)) {
                    hr = pEncoder->lpVtbl->Initialize(pEncoder, (IStream*)pStream, WICBitmapEncoderNoCache);
                    if (SUCCEEDED(hr)) {
                        IWICBitmapFrameEncode* pFrame = NULL;
                        hr = pEncoder->lpVtbl->CreateNewFrame(pEncoder, &pFrame, NULL);
                        if (SUCCEEDED(hr)) {
                            hr = pFrame->lpVtbl->Initialize(pFrame, NULL);
                            if (SUCCEEDED(hr)) {
                                hr = pFrame->lpVtbl->SetSize(pFrame, w, h);
                                if (SUCCEEDED(hr)) {
                                    WICPixelFormatGUID format = local_GUID_WICPixelFormat32bppRGBA;
                                    hr = pFrame->lpVtbl->SetPixelFormat(pFrame, &format);
                                    if (SUCCEEDED(hr)) {
                                        hr = pFrame->lpVtbl->WritePixels(pFrame, h, stride, buffer_size, temp_buf);
                                        if (SUCCEEDED(hr)) {
                                            hr = pFrame->lpVtbl->Commit(pFrame);
                                            if (SUCCEEDED(hr)) {
                                                hr = pEncoder->lpVtbl->Commit(pEncoder);
                                                if (SUCCEEDED(hr)) {
                                                    success = 1;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
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

EXPORT int get_video_width() {
    int w = 0;
    EnterCriticalSection(&g_frame_cs);
    w = g_width;
    LeaveCriticalSection(&g_frame_cs);
    return w;
}

EXPORT int get_video_height() {
    int h = 0;
    EnterCriticalSection(&g_frame_cs);
    h = g_height;
    LeaveCriticalSection(&g_frame_cs);
    return h;
}

EXPORT int get_available_resolutions_count() {
    return g_unique_resolutions_count;
}

EXPORT int get_available_resolution_width(int index) {
    if (index < 0 || index >= g_unique_resolutions_count) return 0;
    return g_unique_resolutions[index].width;
}

EXPORT int get_available_resolution_height(int index) {
    if (index < 0 || index >= g_unique_resolutions_count) return 0;
    return g_unique_resolutions[index].height;
}

EXPORT int is_fps_supported(int width, int height, int target_fps) {
    if (!g_source_reader) return 0;
    int supported = 0;
    for (DWORD i = 0; ; i++) {
        IMFMediaType* pType = NULL;
        HRESULT hr = g_source_reader->lpVtbl->GetNativeMediaType(g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pType);
        if (hr == MF_E_NO_MORE_TYPES || FAILED(hr)) {
            break;
        }

        UINT64 frameSize = 0;
        UINT32 w = 0, h = 0;
        if (SUCCEEDED(pType->lpVtbl->GetUINT64(pType, &MF_MT_FRAME_SIZE, &frameSize))) {
            w = (UINT32)(frameSize >> 32);
            h = (UINT32)frameSize;
        }

        UINT64 frameRate = 0;
        double fps = 0.0;
        if (SUCCEEDED(pType->lpVtbl->GetUINT64(pType, &MF_MT_FRAME_RATE, &frameRate))) {
            UINT32 num = (UINT32)(frameRate >> 32);
            UINT32 den = (UINT32)frameRate;
            if (den > 0) {
                fps = (double)num / den;
            }
        }
        pType->lpVtbl->Release(pType);

        if ((int)w == width && (int)h == height) {
            if (target_fps == 30 && fps >= 29.0 && fps <= 31.0) {
                supported = 1;
                break;
            }
            if (target_fps == 60 && fps >= 58.0 && fps <= 62.0) {
                supported = 1;
                break;
            }
        }
    }
    return supported;
}

EXPORT double get_current_fps() {
    if (!g_source_reader) return 0.0;
    double fps = 0.0;
    IMFMediaType* pType = NULL;
    HRESULT hr = g_source_reader->lpVtbl->GetCurrentMediaType(g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pType);
    if (SUCCEEDED(hr)) {
        UINT64 frameRate = 0;
        if (SUCCEEDED(pType->lpVtbl->GetUINT64(pType, &MF_MT_FRAME_RATE, &frameRate))) {
            UINT32 num = (UINT32)(frameRate >> 32);
            UINT32 den = (UINT32)frameRate;
            if (den > 0) {
                fps = (double)num / den;
            }
        }
        pType->lpVtbl->Release(pType);
    }
    return fps;
}

EXPORT int set_video_resolution_and_fps(int width, int height, int fps_preference) {
    if (!g_source_reader) return 0;

    int was_capturing = g_is_capturing;
    g_is_capturing = 0;
    if (g_capture_thread) {
        WaitForSingleObject(g_capture_thread, INFINITE);
        CloseHandle(g_capture_thread);
        g_capture_thread = NULL;
    }

    IMFMediaType* pBestType = NULL;
    double best_fps = -1.0;

    for (DWORD i = 0; ; i++) {
        IMFMediaType* pType = NULL;
        HRESULT hr = g_source_reader->lpVtbl->GetNativeMediaType(g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pType);
        if (hr == MF_E_NO_MORE_TYPES || FAILED(hr)) {
            break;
        }

        UINT64 frameSize = 0;
        UINT32 w = 0, h = 0;
        if (SUCCEEDED(pType->lpVtbl->GetUINT64(pType, &MF_MT_FRAME_SIZE, &frameSize))) {
            w = (UINT32)(frameSize >> 32);
            h = (UINT32)frameSize;
        }

        UINT64 frameRate = 0;
        double fps = 0.0;
        if (SUCCEEDED(pType->lpVtbl->GetUINT64(pType, &MF_MT_FRAME_RATE, &frameRate))) {
            UINT32 num = (UINT32)(frameRate >> 32);
            UINT32 den = (UINT32)frameRate;
            if (den > 0) {
                fps = (double)num / den;
            }
        }

        if ((int)w == width && (int)h == height) {
            if (fps_preference == 30) {
                if (fps >= 29.0 && fps <= 31.0) {
                    if (pBestType) pBestType->lpVtbl->Release(pBestType);
                    pBestType = pType;
                    best_fps = fps;
                    break;
                }
            } else if (fps_preference == 60) {
                if (fps >= 58.0 && fps <= 62.0) {
                    if (pBestType) pBestType->lpVtbl->Release(pBestType);
                    pBestType = pType;
                    best_fps = fps;
                    break;
                }
            }
            if (fps > best_fps) {
                if (pBestType) pBestType->lpVtbl->Release(pBestType);
                pBestType = pType;
                best_fps = fps;
            } else {
                pType->lpVtbl->Release(pType);
            }
        } else {
            pType->lpVtbl->Release(pType);
        }
    }

    int success = 0;
    if (pBestType) {
        HRESULT hr = g_source_reader->lpVtbl->SetCurrentMediaType(g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pBestType);
        pBestType->lpVtbl->Release(pBestType);
        
        if (SUCCEEDED(hr)) {
            IMFMediaType* pTargetType = NULL;
            hr = MFCreateMediaType(&pTargetType);
            if (SUCCEEDED(hr)) {
                pTargetType->lpVtbl->SetGUID(pTargetType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
                pTargetType->lpVtbl->SetGUID(pTargetType, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
                hr = g_source_reader->lpVtbl->SetCurrentMediaType(g_source_reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pTargetType);
                pTargetType->lpVtbl->Release(pTargetType);
            }

            if (SUCCEEDED(hr)) {
                EnterCriticalSection(&g_frame_cs);
                g_width = width;
                g_height = height;
                LeaveCriticalSection(&g_frame_cs);
                success = 1;
            }
        }
    }

    if (was_capturing) {
        g_is_capturing = 1;
        g_capture_thread = CreateThread(NULL, 0, capture_thread_proc, NULL, 0, NULL);
    }

    return success;
}

void init_video_capture_backend() {
    InitializeCriticalSection(&g_frame_cs);
}

void shutdown_video_capture_backend() {
    DeleteCriticalSection(&g_frame_cs);
}
