#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

// Inicializa o backend do Windows (COM e Media Foundation)
EXPORT int init_devices_backend();

// Finaliza o backend e libera recursos
EXPORT void shutdown_devices_backend();

// Enumera os dispositivos e retorna o total encontrado de vídeo
EXPORT int enum_video_devices();

// Enumera os dispositivos e retorna o total encontrado de áudio
EXPORT int enum_audio_devices();

// Obtém o nome do dispositivo de vídeo pelo índice
EXPORT const char* get_video_device_name(int index);

// Obtém o ID do dispositivo de vídeo pelo índice
EXPORT const char* get_video_device_id(int index);

// Obtém o nome do dispositivo de áudio pelo índice
EXPORT const char* get_audio_device_name(int index);

// Obtém o ID do dispositivo de áudio pelo índice
EXPORT const char* get_audio_device_id(int index);

#endif // DEVICE_MANAGER_H
