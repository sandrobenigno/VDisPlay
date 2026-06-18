#include "video_texture.h"
#include <iostream>

VideoTexture::VideoTexture(flutter::TextureRegistrar* registrar) : registrar_(registrar) {
    pixel_buffer_front_ = {nullptr, 0, 0};
    
    texture_variant_ = std::make_unique<flutter::TextureVariant>(
        flutter::PixelBufferTexture([this](size_t width, size_t height) -> const FlutterDesktopPixelBuffer* {
            return this->CopyPixelBuffer(width, height);
        })
    );
    
    texture_id_ = registrar_->RegisterTexture(texture_variant_.get());
}

VideoTexture::~VideoTexture() {
    if (texture_id_ != -1) {
        registrar_->UnregisterTexture(texture_id_);
    }
}

const FlutterDesktopPixelBuffer* VideoTexture::CopyPixelBuffer(size_t width, size_t height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (buffer_front_.empty()) {
        return nullptr;
    }
    
    pixel_buffer_front_.buffer = buffer_front_.data();
    pixel_buffer_front_.width = width_;
    pixel_buffer_front_.height = height_;
    
    return &pixel_buffer_front_;
}

void VideoTexture::OnFrameReceived(const uint8_t* buffer, int len, int width, int height) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buffer_back_.size() != (size_t)len) {
            buffer_back_.resize(len);
        }
        memcpy(buffer_back_.data(), buffer, len);
        
        // Troca os buffers (double buffering)
        std::swap(buffer_front_, buffer_back_);
        width_ = width;
        height_ = height;
    }
    
    // Sinaliza ao Flutter que um novo quadro está disponível
    registrar_->MarkTextureFrameAvailable(texture_id_);
}
