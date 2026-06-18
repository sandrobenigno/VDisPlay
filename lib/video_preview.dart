import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:ffi/ffi.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'native_bindings.dart';

class VideoPreviewManager extends ChangeNotifier {
  static final VideoPreviewManager _instance = VideoPreviewManager._internal();
  factory VideoPreviewManager() => _instance;
  VideoPreviewManager._internal();

  static const MethodChannel _channel = MethodChannel('vdisplay/texture');

  int? _textureId;
  String? _selectedDeviceId;
  String? _selectedDeviceName;
  int _brightnessOffset = 0;
  bool _isCapturing = false;

  List<Map<String, String>> _videoDevices = [];

  int? get textureId => _textureId;
  String? get selectedDeviceId => _selectedDeviceId;
  String? get selectedDeviceName => _selectedDeviceName;
  int get brightnessOffset => _brightnessOffset;
  bool get isCapturing => _isCapturing;
  List<Map<String, String>> get videoDevices => _videoDevices;

  Future<void> init() async {
    NativeBindings.init();
    NativeBindings.initDevicesBackend();

    // Carrega preferências de brilho
    final prefs = await SharedPreferences.getInstance();
    _brightnessOffset = prefs.getInt('video_brightness_offset') ?? 0;
    NativeBindings.setBrightnessOffset(_brightnessOffset);

    // Lista os dispositivos inicialmente
    refreshDevices();
  }

  void refreshDevices() {
    _videoDevices.clear();
    final count = NativeBindings.enumVideoDevices();
    for (int i = 0; i < count; i++) {
      final name = NativeBindings.getVideoDeviceName(i);
      final id = NativeBindings.getVideoDeviceId(i);
      _videoDevices.add({'name': name, 'id': id});
    }
    notifyListeners();
  }

  int _selectedWidth = 0;
  int _selectedHeight = 0;
  int _fpsPreference = 0; // 0 = Nativo, 30 = 30 FPS, 60 = 60 FPS

  int get selectedWidth => _selectedWidth;
  int get selectedHeight => _selectedHeight;
  int get fpsPreference => _fpsPreference;

  List<Size> get availableResolutions {
    if (!_isCapturing) return [];
    final count = NativeBindings.getAvailableResolutionsCount();
    final list = <Size>[];
    for (int i = 0; i < count; i++) {
      final w = NativeBindings.getAvailableResolutionWidth(i);
      final h = NativeBindings.getAvailableResolutionHeight(i);
      if (w > 0 && h > 0) {
        list.add(Size(w.toDouble(), h.toDouble()));
      }
    }
    return list;
  }

  double get currentFps {
    if (!_isCapturing) return 0.0;
    return NativeBindings.getCurrentFps();
  }

  bool isFpsSupported(int width, int height, int targetFps) {
    if (!_isCapturing) return false;
    return NativeBindings.isFpsSupported(width, height, targetFps) == 1;
  }

  Future<bool> changeResolutionAndFps(int width, int height, int fpsPref) async {
    if (!_isCapturing) return false;
    final success = NativeBindings.setVideoResolutionAndFps(width, height, fpsPref);
    if (success == 1) {
      _selectedWidth = width;
      _selectedHeight = height;
      _fpsPreference = fpsPref;
      notifyListeners();
      return true;
    }
    return false;
  }

  Future<void> startPreview(String deviceId, String name) async {
    _selectedDeviceId = deviceId;
    _selectedDeviceName = name;

    try {
      // 1. Cria a textura nativa se necessário
      if (_textureId == null) {
        final id = await _channel.invokeMethod<int>('createTexture');
        _textureId = id;
      }

      // 2. Inicia a captura nativa vinculada a esta textura
      if (_textureId != null) {
        final success = await _channel.invokeMethod<bool>('startCapture', {
          'deviceId': deviceId,
        });
        _isCapturing = success ?? false;
      }

      if (_isCapturing) {
        _selectedWidth = NativeBindings.getVideoWidth();
        _selectedHeight = NativeBindings.getVideoHeight();
        _fpsPreference = 0; // Default: Nativo
      }

      // Salva preferências do dispositivo ativo
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('last_video_device', deviceId);
    } catch (e) {
      print('Erro ao iniciar preview: $e');
      _isCapturing = false;
    }

    notifyListeners();
  }

  Future<void> stopPreview() async {
    if (!_isCapturing) return;
    try {
      await _channel.invokeMethod('stopCapture');
      await _channel.invokeMethod('disposeTexture');
      _textureId = null;
      _isCapturing = false;
    } catch (e) {
      print('Erro ao parar preview: $e');
    }
    notifyListeners();
  }

  void setBrightness(int offset) {
    // Clampa entre -100 e 100
    _brightnessOffset = offset.clamp(-100, 100);
    NativeBindings.setBrightnessOffset(_brightnessOffset);
    
    // Salva preferências
    SharedPreferences.getInstance().then((prefs) {
      prefs.setInt('video_brightness_offset', _brightnessOffset);
    });

    notifyListeners();
  }

  Future<String?> takeScreenshot() async {
    if (!_isCapturing) return null;
    try {
      final dir = Directory.current.path;
      final timestamp = DateTime.now().millisecondsSinceEpoch;
      final filepath = '$dir\\screenshot_$timestamp.png';
      
      final filepathPtr = filepath.toNativeUtf8();
      final success = NativeBindings.saveScreenshot(filepathPtr);
      calloc.free(filepathPtr);

      if (success == 1) {
        return filepath;
      }
    } catch (e) {
      print('Erro ao salvar screenshot: $e');
    }
    return null;
  }

  @override
  void dispose() {
    stopPreview();
    NativeBindings.shutdownDevicesBackend();
    super.dispose();
  }
}
