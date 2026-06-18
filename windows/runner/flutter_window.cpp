#include "flutter_window.h"

#include <optional>

#include "flutter/generated_plugin_registrant.h"
#include "video_texture.h"
#include "../flutter/ephemeral/cpp_client_wrapper/texture_registrar_impl.h"
#include <flutter/method_channel.h>
#include <flutter/standard_method_codec.h>

extern "C" {
#include "include/video_capture.h"
}

static VideoTexture* g_active_texture = nullptr;

static void on_native_frame(const uint8_t* buffer, int len, int width, int height) {
    if (g_active_texture) {
        g_active_texture->OnFrameReceived(buffer, len, width, height);
    }
}

FlutterWindow::FlutterWindow(const flutter::DartProject& project)
    : project_(project) {}

FlutterWindow::~FlutterWindow() {}

bool FlutterWindow::OnCreate() {
  if (!Win32Window::OnCreate()) {
    return false;
  }

  RECT frame = GetClientArea();

  // The size here must match the window dimensions to avoid unnecessary surface
  // creation / destruction in the startup path.
  flutter_controller_ = std::make_unique<flutter::FlutterViewController>(
      frame.right - frame.left, frame.bottom - frame.top, project_);
  // Ensure that basic setup of the controller was successful.
  if (!flutter_controller_->engine() || !flutter_controller_->view()) {
    return false;
  }
  RegisterPlugins(flutter_controller_->engine());
  SetChildContent(flutter_controller_->view()->GetNativeWindow());

  // Configuração do canal de métodos para comunicação e controle do preview
  FlutterDesktopPluginRegistrarRef core_registrar = 
      flutter_controller_->engine()->GetRegistrarForPlugin("VdisplayRunnerPlugin");
  FlutterDesktopTextureRegistrarRef texture_registrar_ref = 
      FlutterDesktopRegistrarGetTextureRegistrar(core_registrar);
  texture_registrar_ = std::make_unique<flutter::TextureRegistrarImpl>(texture_registrar_ref);
  auto registrar = texture_registrar_.get();
  
  // Guardamos o canal de método em uma variável estática ou apenas o configuramos
  // para escutar mensagens. Em C++, o handler mantém a assinatura ativa.
  auto channel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
      flutter_controller_->engine()->messenger(),
      "vdisplay/texture",
      &flutter::StandardMethodCodec::GetInstance()
  );

  // Mantemos uma referência ao canal estática ou via lambda
  static std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> s_channel;
  s_channel = std::move(channel);

  s_channel->SetMethodCallHandler(
      [this, registrar](const flutter::MethodCall<flutter::EncodableValue>& call,
                         std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
          if (call.method_name() == "createTexture") {
              if (!video_texture_) {
                  video_texture_ = std::make_unique<VideoTexture>(registrar);
                  g_active_texture = video_texture_.get();
              }
              result->Success(flutter::EncodableValue(video_texture_->texture_id()));
          } 
          else if (call.method_name() == "disposeTexture") {
              if (video_texture_) {
                  stop_video_capture();
                  g_active_texture = nullptr;
                  video_texture_.reset();
              }
              result->Success(flutter::EncodableValue(true));
          } 
          else if (call.method_name() == "startCapture") {
              const auto* arguments = std::get_if<flutter::EncodableMap>(call.arguments());
              std::string device_id;
              if (arguments) {
                  auto it = arguments->find(flutter::EncodableValue("deviceId"));
                  if (it != arguments->end() && std::holds_alternative<std::string>(it->second)) {
                      device_id = std::get<std::string>(it->second);
                  }
              }
              
              if (device_id.empty()) {
                  result->Error("INVALID_ARGUMENTS", "device_id is required");
                  return;
              }

              int success = start_video_capture(device_id.c_str(), on_native_frame);
              result->Success(flutter::EncodableValue(success == 1));
          } 
          else if (call.method_name() == "stopCapture") {
              stop_video_capture();
              result->Success(flutter::EncodableValue(true));
          }
          else {
              result->NotImplemented();
          }
      }
  );

  flutter_controller_->engine()->SetNextFrameCallback([&]() {
    this->Show();
  });

  // Flutter can complete the first frame before the "show window" callback is
  // registered. The following call ensures a frame is pending to ensure the
  // window is shown. It is a no-op if the first frame hasn't completed yet.
  flutter_controller_->ForceRedraw();

  return true;
}

void FlutterWindow::OnDestroy() {
  stop_video_capture();
  g_active_texture = nullptr;
  video_texture_.reset();
  texture_registrar_.reset();

  if (flutter_controller_) {
    flutter_controller_ = nullptr;
  }

  Win32Window::OnDestroy();
}

LRESULT
FlutterWindow::MessageHandler(HWND hwnd, UINT const message,
                              WPARAM const wparam,
                              LPARAM const lparam) noexcept {
  // Give Flutter, including plugins, an opportunity to handle window messages.
  if (flutter_controller_) {
    std::optional<LRESULT> result =
        flutter_controller_->HandleTopLevelWindowProc(hwnd, message, wparam,
                                                      lparam);
    if (result) {
      return *result;
    }
  }

  switch (message) {
    case WM_FONTCHANGE:
      flutter_controller_->engine()->ReloadSystemFonts();
      break;
  }

  return Win32Window::MessageHandler(hwnd, message, wparam, lparam);
}
