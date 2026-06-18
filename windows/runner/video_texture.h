#ifndef RUNNER_VIDEO_TEXTURE_H_
#define RUNNER_VIDEO_TEXTURE_H_

#include <flutter/texture_registrar.h>
#include <mutex>
#include <vector>
#include <memory>
#include <stdint.h>

class VideoTexture {
 public:
  VideoTexture(flutter::TextureRegistrar* registrar);
  ~VideoTexture();

  int64_t texture_id() const { return texture_id_; }

  // Callback consumida pelo Flutter para renderização
  const FlutterDesktopPixelBuffer* CopyPixelBuffer(size_t width, size_t height);

  // Callback de recebimento do frame de vídeo vindo do kernel C
  void OnFrameReceived(const uint8_t* buffer, int len, int width, int height);

 private:
  flutter::TextureRegistrar* registrar_;
  int64_t texture_id_ = -1;
  std::unique_ptr<flutter::TextureVariant> texture_variant_;

  std::mutex mutex_;
  std::vector<uint8_t> buffer_front_;
  std::vector<uint8_t> buffer_back_;
  FlutterDesktopPixelBuffer pixel_buffer_front_;
  
  int width_ = 0;
  int height_ = 0;
};

#endif  // RUNNER_VIDEO_TEXTURE_H_
