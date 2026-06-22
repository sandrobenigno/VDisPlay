import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:ffi/ffi.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'native_bindings.dart';


class AudioManager extends ChangeNotifier {
  static final AudioManager _instance = AudioManager._internal();
  factory AudioManager() => _instance;
  AudioManager._internal();

  String? _selectedDeviceId;
  String? _selectedOutputDeviceId;
  bool _isLoopbackEnabled = false;
  bool _isDeinterleaveEnabled = false;
  double _peakVolume = 0.0;
  Timer? _vuTimer;

  String? get selectedDeviceId => _selectedDeviceId;
  String? get selectedOutputDeviceId => _selectedOutputDeviceId;
  bool get isLoopbackEnabled => _isLoopbackEnabled;
  bool get isDeinterleaveEnabled => _isDeinterleaveEnabled;
  double get peakVolume => _peakVolume;
  bool get isCapturing =>
      NativeBindings.isReady && NativeBindings.isAudioCapturing() == 1;

  void startVUPolling() {
    _vuTimer?.cancel();
    _vuTimer = Timer.periodic(const Duration(milliseconds: 50), (timer) {
      if (isCapturing) {
        final vol = NativeBindings.getAudioPeakVolume();
        if (_peakVolume != vol) {
          _peakVolume = vol;
          notifyListeners();
        }
      } else if (_peakVolume > 0.0) {
        _peakVolume = 0.0;
        notifyListeners();
      }
    });
  }

  void stopVUPolling() {
    _vuTimer?.cancel();
    _vuTimer = null;
    _peakVolume = 0.0;
    notifyListeners();
  }

  Future<void> init() async {
    final prefs = await SharedPreferences.getInstance();
    _isLoopbackEnabled = prefs.getBool('audio_loopback_enabled') ?? false;
    _isDeinterleaveEnabled =
        prefs.getBool('audio_deinterleave_enabled') ?? false;

    final savedAudioOutputDevice = prefs.getString('last_audio_output_device');
    if (savedAudioOutputDevice != null && NativeBindings.isReady) {
      _selectedOutputDeviceId = savedAudioOutputDevice;
      NativeBindings.setAudioOutputDevice(savedAudioOutputDevice);
    }

    final savedAudioDevice = prefs.getString('last_audio_device');
    if (savedAudioDevice != null && NativeBindings.isReady) {
      final count = NativeBindings.enumAudioDevices();
      for (int i = 0; i < count; i++) {
        if (NativeBindings.getAudioDeviceId(i) == savedAudioDevice) {
          await startCapture(savedAudioDevice, _isLoopbackEnabled);
          break;
        }
      }
    }
  }

  Future<int> startCapture(String deviceId, bool enableLoopback) async {
    _selectedDeviceId = deviceId;
    _isLoopbackEnabled = enableLoopback;

    if (!NativeBindings.isReady) return 0;

    NativeBindings.setAudioDeinterleave(_isDeinterleaveEnabled ? 1 : 0);

    final deviceIdPtr = deviceId.toNativeUtf8();
    NativeBindings.startAudioCapture(deviceIdPtr, enableLoopback ? 1 : 0);
    calloc.free(deviceIdPtr);

    // Salva preferências
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('last_audio_device', deviceId);
    await prefs.setBool('audio_loopback_enabled', enableLoopback);

    startVUPolling();
    notifyListeners();

    return NativeBindings.getAudioChannels();
  }

  Future<void> stopCapture() async {
    // Always stop VU polling before stopping capture to avoid reading
    // stale peak data after the audio engine is torn down.
    stopVUPolling();
    if (NativeBindings.isReady) {
      NativeBindings.stopAudioCapture();
    }
    notifyListeners();
  }

  Future<void> setLoopback(bool enabled) async {
    _isLoopbackEnabled = enabled;
    if (NativeBindings.isReady) {
      NativeBindings.setAudioLoopback(enabled ? 1 : 0);
    }

    final prefs = await SharedPreferences.getInstance();
    await prefs.setBool('audio_loopback_enabled', enabled);

    notifyListeners();
  }

  Future<void> setDeinterleave(bool enabled) async {
    _isDeinterleaveEnabled = enabled;
    if (NativeBindings.isReady) {
      NativeBindings.setAudioDeinterleave(enabled ? 1 : 0);
    }

    final prefs = await SharedPreferences.getInstance();
    await prefs.setBool('audio_deinterleave_enabled', enabled);

    notifyListeners();
  }

  Future<void> setOutputDevice(String deviceId) async {
    _selectedOutputDeviceId = deviceId;
    if (NativeBindings.isReady) {
      NativeBindings.setAudioOutputDevice(deviceId);
    }

    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('last_audio_output_device', deviceId);

    // If already capturing, we should ideally restart capture so the backend picks up the new output device.
    if (isCapturing && _selectedDeviceId != null) {
      await stopCapture();
      // small delay to let backend shutdown
      await Future.delayed(const Duration(milliseconds: 100));
      await startCapture(_selectedDeviceId!, _isLoopbackEnabled);
    } else {
      notifyListeners();
    }
  }

}
