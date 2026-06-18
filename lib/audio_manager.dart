import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:ffi/ffi.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'native_bindings.dart';

/// Result record returned by [AudioManager.tryAutoMatch].
typedef AutoMatchResult = ({String? audioName, bool isMono});

class AudioManager extends ChangeNotifier {
  static final AudioManager _instance = AudioManager._internal();
  factory AudioManager() => _instance;
  AudioManager._internal();

  String? _selectedDeviceId;
  bool _isLoopbackEnabled = false;
  bool _isDeinterleaveEnabled = false;
  double _peakVolume = 0.0;
  Timer? _vuTimer;

  String? get selectedDeviceId => _selectedDeviceId;
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

  /// Tries to automatically pair an audio device whose name overlaps with
  /// [cameraName]. Returns the matched device name and whether it is MONO.
  /// Returns `(audioName: null, isMono: false)` if no match is found.
  Future<AutoMatchResult> tryAutoMatch(String cameraName) async {
    await stopCapture();

    if (!NativeBindings.isReady) return (audioName: null, isMono: false);

    final count = NativeBindings.enumAudioDevices();
    final cleanCamName = cameraName.toLowerCase();

    String? matchedId;
    String? matchedName;

    for (int i = 0; i < count; i++) {
      final aName = NativeBindings.getAudioDeviceName(i);
      final aId = NativeBindings.getAudioDeviceId(i);
      if (aName.toLowerCase().contains(cleanCamName) ||
          cleanCamName.contains(aName.toLowerCase())) {
        matchedId = aId;
        matchedName = aName;
        break;
      }
    }

    if (matchedId == null) return (audioName: null, isMono: false);

    final channels = await startCapture(matchedId, _isLoopbackEnabled);
    return (audioName: matchedName, isMono: channels == 1);
  }
}
