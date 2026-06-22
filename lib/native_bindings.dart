import 'dart:developer' as dev;
import 'dart:ffi';
import 'package:ffi/ffi.dart';

// Typedefs para as funções em C nativo
typedef InitDevicesBackendC = Int32 Function();
typedef InitDevicesBackendDart = int Function();

typedef ShutdownDevicesBackendC = Void Function();
typedef ShutdownDevicesBackendDart = void Function();

typedef EnumVideoDevicesC = Int32 Function();
typedef EnumVideoDevicesDart = int Function();

typedef EnumAudioDevicesC = Int32 Function();
typedef EnumAudioDevicesDart = int Function();

typedef EnumAudioOutputDevicesC = Int32 Function();
typedef EnumAudioOutputDevicesDart = int Function();

typedef GetDeviceStringC = Pointer<Utf8> Function(Int32 index);
typedef GetDeviceStringDart = Pointer<Utf8> Function(int index);

typedef StartAudioCaptureC = Int32 Function(Pointer<Utf8> deviceId, Int32 enableLoopback);
typedef StartAudioCaptureDart = int Function(Pointer<Utf8> deviceId, int enableLoopback);

typedef SetAudioOutputDeviceC = Void Function(Pointer<Utf8> deviceId);
typedef SetAudioOutputDeviceDart = void Function(Pointer<Utf8> deviceId);

typedef StopAudioCaptureC = Void Function();
typedef StopAudioCaptureDart = void Function();

typedef IsAudioCapturingC = Int32 Function();
typedef IsAudioCapturingDart = int Function();

typedef GetAudioPeakVolumeC = Float Function();
typedef GetAudioPeakVolumeDart = double Function();

typedef GetAudioChannelsC = Int32 Function();
typedef GetAudioChannelsDart = int Function();

typedef SetAudioLoopbackC = Void Function(Int32 enabled);
typedef SetAudioLoopbackDart = void Function(int enabled);

typedef SetAudioDeinterleaveC = Void Function(Int32 enabled);
typedef SetAudioDeinterleaveDart = void Function(int enabled);

typedef GetAudioDeinterleaveC = Int32 Function();
typedef GetAudioDeinterleaveDart = int Function();


typedef SetBrightnessOffsetC = Void Function(Int32 offset);
typedef SetBrightnessOffsetDart = void Function(int offset);

typedef GetVideoDimensionC = Int32 Function();
typedef GetVideoDimensionDart = int Function();

typedef SaveScreenshotC = Int32 Function(Pointer<Utf8> filepath);
typedef SaveScreenshotDart = int Function(Pointer<Utf8> filepath);

typedef GetAvailableResolutionsCountC = Int32 Function();
typedef GetAvailableResolutionsCountDart = int Function();

typedef GetAvailableResolutionDimensionC = Int32 Function(Int32 index);
typedef GetAvailableResolutionDimensionDart = int Function(int index);

typedef IsFpsSupportedC = Int32 Function(Int32 width, Int32 height, Int32 targetFps);
typedef IsFpsSupportedDart = int Function(int width, int height, int targetFps);

typedef SetVideoResolutionAndFpsC = Int32 Function(Int32 width, Int32 height, Int32 fpsPreference);
typedef SetVideoResolutionAndFpsDart = int Function(int width, int height, int fpsPreference);

typedef GetCurrentFpsC = Double Function();
typedef GetCurrentFpsDart = double Function();

class NativeBindings {
  static late final DynamicLibrary _lib;
  
  static late final InitDevicesBackendDart initDevicesBackend;
  static late final ShutdownDevicesBackendDart shutdownDevicesBackend;
  static late final EnumVideoDevicesDart enumVideoDevices;
  static late final EnumAudioDevicesDart enumAudioDevices;
  static late final EnumAudioOutputDevicesDart enumAudioOutputDevices;
  static late final GetVideoDimensionDart getVideoWidth;
  static late final GetVideoDimensionDart getVideoHeight;
  
  static late final GetDeviceStringDart _getVideoDeviceName;
  static late final GetDeviceStringDart _getVideoDeviceId;
  static late final GetDeviceStringDart _getAudioDeviceName;
  static late final GetDeviceStringDart _getAudioDeviceId;
  
  static late final GetDeviceStringDart _getAudioOutputDeviceName;
  static late final GetDeviceStringDart _getAudioOutputDeviceId;
  static late final SetAudioOutputDeviceDart _setAudioOutputDevice;
  
  static late final StartAudioCaptureDart startAudioCapture;
  static late final StopAudioCaptureDart stopAudioCapture;
  static late final IsAudioCapturingDart isAudioCapturing;
  static late final GetAudioPeakVolumeDart getAudioPeakVolume;
  static late final GetAudioChannelsDart getAudioChannels;
  static late final SetAudioLoopbackDart setAudioLoopback;
  static late final SetAudioDeinterleaveDart setAudioDeinterleave;
  static late final GetAudioDeinterleaveDart getAudioDeinterleave;

  
  static late final SetBrightnessOffsetDart setBrightnessOffset;
  static late final SaveScreenshotDart saveScreenshot;
  
  static late final GetAvailableResolutionsCountDart getAvailableResolutionsCount;
  static late final GetAvailableResolutionDimensionDart getAvailableResolutionWidth;
  static late final GetAvailableResolutionDimensionDart getAvailableResolutionHeight;
  static late final IsFpsSupportedDart isFpsSupported;
  static late final SetVideoResolutionAndFpsDart setVideoResolutionAndFps;
  static late final GetCurrentFpsDart getCurrentFps;

  static bool _initialized = false;

  /// Whether the native library was loaded and all symbols resolved.
  static bool get isReady => _initialized;

  static void init() {
    if (_initialized) return;
    try {
      _lib = DynamicLibrary.open('native_library.dll');
      
      initDevicesBackend = _lib
          .lookup<NativeFunction<InitDevicesBackendC>>('init_devices_backend')
          .asFunction();
          
      shutdownDevicesBackend = _lib
          .lookup<NativeFunction<ShutdownDevicesBackendC>>('shutdown_devices_backend')
          .asFunction();
          
      enumVideoDevices = _lib
          .lookup<NativeFunction<EnumVideoDevicesC>>('enum_video_devices')
          .asFunction();
          
      enumAudioDevices = _lib
          .lookup<NativeFunction<EnumAudioDevicesC>>('enum_audio_devices')
          .asFunction();

      enumAudioOutputDevices = _lib
          .lookup<NativeFunction<EnumAudioOutputDevicesC>>('enum_audio_output_devices')
          .asFunction();

      getVideoWidth = _lib
          .lookup<NativeFunction<GetVideoDimensionC>>('get_video_width')
          .asFunction();

      getVideoHeight = _lib
          .lookup<NativeFunction<GetVideoDimensionC>>('get_video_height')
          .asFunction();
          
      _getVideoDeviceName = _lib
          .lookup<NativeFunction<GetDeviceStringC>>('get_video_device_name')
          .asFunction();
          
      _getVideoDeviceId = _lib
          .lookup<NativeFunction<GetDeviceStringC>>('get_video_device_id')
          .asFunction();
          
      _getAudioDeviceName = _lib
          .lookup<NativeFunction<GetDeviceStringC>>('get_audio_device_name')
          .asFunction();
          
      _getAudioDeviceId = _lib
          .lookup<NativeFunction<GetDeviceStringC>>('get_audio_device_id')
          .asFunction();

      _getAudioOutputDeviceName = _lib
          .lookup<NativeFunction<GetDeviceStringC>>('get_audio_output_device_name')
          .asFunction();
          
      _getAudioOutputDeviceId = _lib
          .lookup<NativeFunction<GetDeviceStringC>>('get_audio_output_device_id')
          .asFunction();
          
      _setAudioOutputDevice = _lib
          .lookup<NativeFunction<SetAudioOutputDeviceC>>('set_audio_output_device')
          .asFunction();
          
      startAudioCapture = _lib
          .lookup<NativeFunction<StartAudioCaptureC>>('start_audio_capture')
          .asFunction();
          
      stopAudioCapture = _lib
          .lookup<NativeFunction<StopAudioCaptureC>>('stop_audio_capture')
          .asFunction();
          
      isAudioCapturing = _lib
          .lookup<NativeFunction<IsAudioCapturingC>>('is_audio_capturing')
          .asFunction();
          
      getAudioPeakVolume = _lib
          .lookup<NativeFunction<GetAudioPeakVolumeC>>('get_audio_peak_volume')
          .asFunction();
          
      getAudioChannels = _lib
          .lookup<NativeFunction<GetAudioChannelsC>>('get_audio_channels')
          .asFunction();
          
      setAudioLoopback = _lib
          .lookup<NativeFunction<SetAudioLoopbackC>>('set_audio_loopback')
          .asFunction();

      setAudioDeinterleave = _lib
          .lookup<NativeFunction<SetAudioDeinterleaveC>>('set_audio_deinterleave')
          .asFunction();

      getAudioDeinterleave = _lib
          .lookup<NativeFunction<GetAudioDeinterleaveC>>('get_audio_deinterleave')
          .asFunction();

          
      setBrightnessOffset = _lib
          .lookup<NativeFunction<SetBrightnessOffsetC>>('set_brightness_offset')
          .asFunction();
          
      saveScreenshot = _lib
          .lookup<NativeFunction<SaveScreenshotC>>('save_screenshot')
          .asFunction();

      getAvailableResolutionsCount = _lib
          .lookup<NativeFunction<GetAvailableResolutionsCountC>>('get_available_resolutions_count')
          .asFunction();

      getAvailableResolutionWidth = _lib
          .lookup<NativeFunction<GetAvailableResolutionDimensionC>>('get_available_resolution_width')
          .asFunction();

      getAvailableResolutionHeight = _lib
          .lookup<NativeFunction<GetAvailableResolutionDimensionC>>('get_available_resolution_height')
          .asFunction();

      isFpsSupported = _lib
          .lookup<NativeFunction<IsFpsSupportedC>>('is_fps_supported')
          .asFunction();

      setVideoResolutionAndFps = _lib
          .lookup<NativeFunction<SetVideoResolutionAndFpsC>>('set_video_resolution_and_fps')
          .asFunction();

      getCurrentFps = _lib
          .lookup<NativeFunction<GetCurrentFpsC>>('get_current_fps')
          .asFunction();

      _initialized = true;
    } catch (e) {
      // Keep _initialized = false so all callers receive safe defaults.
      dev.log('[NativeBindings] CRITICAL: Failed to load native_library.dll — $e', name: 'NativeBindings');
      dev.log('[NativeBindings] Make sure native_library.dll is next to the executable.', name: 'NativeBindings');
    }
  }

  static String getVideoDeviceName(int index) {
    if (!_initialized) return '';
    final ptr = _getVideoDeviceName(index);
    return ptr.toDartString();
  }

  static String getVideoDeviceId(int index) {
    if (!_initialized) return '';
    final ptr = _getVideoDeviceId(index);
    return ptr.toDartString();
  }

  static String getAudioDeviceName(int index) {
    if (!_initialized) return '';
    final ptr = _getAudioDeviceName(index);
    return ptr.toDartString();
  }

  static String getAudioDeviceId(int index) {
    if (!_initialized) return '';
    final ptr = _getAudioDeviceId(index);
    return ptr.toDartString();
  }

  static String getAudioOutputDeviceName(int index) {
    if (!_initialized) return '';
    final ptr = _getAudioOutputDeviceName(index);
    return ptr.toDartString();
  }

  static String getAudioOutputDeviceId(int index) {
    if (!_initialized) return '';
    final ptr = _getAudioOutputDeviceId(index);
    return ptr.toDartString();
  }

  static void setAudioOutputDevice(String deviceId) {
    if (!_initialized) return;
    if (deviceId.isEmpty) {
      _setAudioOutputDevice(Pointer<Utf8>.fromAddress(0));
    } else {
      final ptr = deviceId.toNativeUtf8();
      _setAudioOutputDevice(ptr);
      calloc.free(ptr);
    }
  }
}
