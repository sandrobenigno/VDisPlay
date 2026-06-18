import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'package:win32/win32.dart';

class WindowUtils {
  /// Must match the `title` set in MaterialApp and the Win32 window title.
  static const String kWindowTitle = 'vdisplay';
  static bool _isFullscreen = false;
  static bool _isAlwaysOnTop = false;
  
  static int _savedStyle = 0;
  static int _savedExStyle = 0;
  
  static int _savedLeft = 100;
  static int _savedTop = 100;
  static int _savedWidth = 1280;
  static int _savedHeight = 720;

  static HWND getHwnd() {
    final titlePtr = kWindowTitle.toNativeUtf16();
    final result = FindWindow(PCWSTR(nullptr), PCWSTR(titlePtr));
    calloc.free(titlePtr);
    return result.value;
  }

  static void toggleAlwaysOnTop() {
    final hwnd = getHwnd();
    if (hwnd == nullptr || hwnd.address == 0) return;
    
    _isAlwaysOnTop = !_isAlwaysOnTop;
    SetWindowPos(
      hwnd,
      _isAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
      0, 0, 0, 0,
      SWP_NOMOVE | SWP_NOSIZE
    );
  }

  static bool isAlwaysOnTop() => _isAlwaysOnTop;
  static bool isFullscreen() => _isFullscreen;

  static void toggleFullscreen() {
    final hwnd = getHwnd();
    if (hwnd == nullptr || hwnd.address == 0) return;

    if (!_isFullscreen) {
      // Salva estilo e coordenadas atuais antes do fullscreen
      _savedStyle = GetWindowLongPtr(hwnd, GWL_STYLE).value;
      _savedExStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE).value;
      
      final rect = calloc<RECT>();
      GetWindowRect(hwnd, rect);
      _savedLeft = rect.ref.left;
      _savedTop = rect.ref.top;
      _savedWidth = rect.ref.right - rect.ref.left;
      _savedHeight = rect.ref.bottom - rect.ref.top;
      calloc.free(rect);

      // Obtém as coordenadas do monitor atual
      final monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      final monitorInfo = calloc<MONITORINFO>();
      monitorInfo.ref.cbSize = sizeOf<MONITORINFO>();
      GetMonitorInfo(monitor, monitorInfo);

      // Altera o estilo para borderless (popup)
      SetWindowLongPtr(hwnd, GWL_STYLE, _savedStyle & ~WS_OVERLAPPEDWINDOW);

      final w = monitorInfo.ref.rcMonitor.right - monitorInfo.ref.rcMonitor.left;
      final h = monitorInfo.ref.rcMonitor.bottom - monitorInfo.ref.rcMonitor.top;
      final left = monitorInfo.ref.rcMonitor.left;
      final top = monitorInfo.ref.rcMonitor.top;
      
      calloc.free(monitorInfo);

      SetWindowPos(
        hwnd,
        HWND_TOP,
        left,
        top,
        w,
        h,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED
      );
      
      _isFullscreen = true;
    } else {
      // Restaura estilos e posição salvos
      SetWindowLongPtr(hwnd, GWL_STYLE, _savedStyle);
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, _savedExStyle);
      
      SetWindowPos(
        hwnd,
        HWND_TOP,
        _savedLeft,
        _savedTop,
        _savedWidth,
        _savedHeight,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED
      );

      // Re-aplica Always on Top se estivesse ativo
      if (_isAlwaysOnTop) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      }
      
      _isFullscreen = false;
    }
  }

  static void exitFullscreen() {
    if (_isFullscreen) {
      toggleFullscreen();
    }
  }
}
