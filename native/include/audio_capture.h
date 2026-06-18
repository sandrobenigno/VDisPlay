#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

// Inicia a captura de áudio para o ID de dispositivo fornecido
EXPORT int start_audio_capture(const char* device_id, int enable_loopback);

// Para a captura de áudio ativa
EXPORT void stop_audio_capture();

// Retorna se o áudio está capturando (1 = Sim, 0 = Não)
EXPORT int is_audio_capturing();

// Retorna o pico de volume atual (VU meter, valor de 0.0 a 1.0)
EXPORT float get_audio_peak_volume();

// Altera dinamicamente se o áudio capturado deve ser reproduzido nos alto-falantes
EXPORT void set_audio_loopback(int enabled);

// Retorna o número de canais negociado
EXPORT int get_audio_channels();

// Ativa ou desativa a desintercalação de áudio mono de 96kHz em estéreo
EXPORT void set_audio_deinterleave(int enabled);

// Retorna se a desintercalação de áudio está ativa
EXPORT int get_audio_deinterleave();

#endif // AUDIO_CAPTURE_H
