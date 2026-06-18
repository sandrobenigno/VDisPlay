#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

#include <stdint.h>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

// Assinatura da callback que receberá o buffer de frames (RGBA ou BGRA)
typedef void (*FrameCallback)(const uint8_t* buffer, int len, int width, int height);

// Inicia a captura de vídeo para o ID de dispositivo fornecido
EXPORT int start_video_capture(const char* device_id, FrameCallback callback);

// Para a captura de vídeo ativa
EXPORT void stop_video_capture();

// Retorna se o vídeo está capturando no momento (1 = Sim, 0 = Não)
EXPORT int is_video_capturing();

// Salva o último frame capturado em formato PNG no caminho fornecido
EXPORT int save_screenshot(const char* filepath);

// Retorna a largura do vídeo atualmente capturado
EXPORT int get_video_width();

// Retorna a altura do vídeo atualmente capturado
EXPORT int get_video_height();

// Define o ajuste de brilho (-100 a 100)
EXPORT void set_brightness_offset(int offset);

// Retorna a quantidade de resoluções únicas disponíveis para o dispositivo atual
EXPORT int get_available_resolutions_count();

// Retorna a largura de uma resolução disponível por índice
EXPORT int get_available_resolution_width(int index);

// Retorna a altura de uma resolução disponível por índice
EXPORT int get_available_resolution_height(int index);

// Verifica se um FPS específico é suportado para uma resolução específica (retorna 1 se sim, 0 se não)
EXPORT int is_fps_supported(int width, int height, int target_fps);

// Altera a resolução e o FPS em execução
EXPORT int set_video_resolution_and_fps(int width, int height, int fps_preference);

// Retorna a taxa de quadros (FPS) atual
EXPORT double get_current_fps();

#endif // VIDEO_CAPTURE_H
