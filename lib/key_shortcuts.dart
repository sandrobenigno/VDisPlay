import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'video_preview.dart';
import 'audio_manager.dart';
import 'window_utils.dart';
import 'l10n.dart';

class GlobalKeyboardShortcutHandler extends StatefulWidget {
  final Widget child;
  final VoidCallback onToggleMenu;
  final bool isMenuOpen;
  final VoidCallback onToggleHelp;
  final bool isHelpOpen;
  final VoidCallback onToggleResolution;
  final bool isResolutionOpen;
  final Function(String message) showToast;

  const GlobalKeyboardShortcutHandler({
    Key? key,
    required this.child,
    required this.onToggleMenu,
    required this.isMenuOpen,
    required this.onToggleHelp,
    required this.isHelpOpen,
    required this.onToggleResolution,
    required this.isResolutionOpen,
    required this.showToast,
  }) : super(key: key);

  @override
  State<GlobalKeyboardShortcutHandler> createState() =>
      _GlobalKeyboardShortcutHandlerState();
}

class _GlobalKeyboardShortcutHandlerState
    extends State<GlobalKeyboardShortcutHandler> {
  final FocusNode _focusNode = FocusNode();

  @override
  void initState() {
    super.initState();
    // Garante que o foco seja solicitado na inicialização
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _focusNode.requestFocus();
    });
  }

  @override
  void dispose() {
    _focusNode.dispose();
    super.dispose();
  }

  void _handleKeyEvent(KeyEvent event) {
    if (event is! KeyDownEvent) return;

    final key = event.logicalKey;
    final s = LocaleNotifier.instance.s;

    // Atalho 'M': Alterna Menu
    if (key == LogicalKeyboardKey.keyM) {
      widget.onToggleMenu();
      return;
    }

    // Atalho 'H': Alterna Ajuda
    if (key == LogicalKeyboardKey.keyH) {
      widget.onToggleHelp();
      return;
    }

    // Atalho 'R': Alterna Resolução/FPS
    if (key == LogicalKeyboardKey.keyR) {
      widget.onToggleResolution();
      return;
    }

    // Atalho 'ESC': Fecha menu ou sai de Fullscreen
    if (key == LogicalKeyboardKey.escape) {
      if (widget.isMenuOpen) {
        widget.onToggleMenu();
      } else if (widget.isHelpOpen) {
        widget.onToggleHelp();
      } else if (widget.isResolutionOpen) {
        widget.onToggleResolution();
      } else if (WindowUtils.isFullscreen()) {
        WindowUtils.exitFullscreen();
        widget.showToast(s.toastWindowRestored);
      }
      return;
    }

    // Atalhos 'F' ou 'F11': Tela Cheia
    if (key == LogicalKeyboardKey.keyF || key == LogicalKeyboardKey.f11) {
      WindowUtils.toggleFullscreen();
      widget.showToast(
        WindowUtils.isFullscreen() ? s.toastFullscreen : s.toastWindowRestored,
      );
      return;
    }

    // Atalho 'S': Screenshot
    if (key == LogicalKeyboardKey.keyS) {
      final videoManager = VideoPreviewManager();
      if (videoManager.isCapturing) {
        videoManager.takeScreenshot().then((path) {
          if (path != null) {
            // Show only the filename, not the full path, to keep the toast short.
            final filename = path.split(RegExp(r'[/\\]')).last;
            widget.showToast(s.toastScreenshotSaved(filename));
          } else {
            widget.showToast(s.toastScreenshotError);
          }
        });
      } else {
        widget.showToast(s.toastNoPreview);
      }
      return;
    }

    // Atalho 'T': Always on Top
    if (key == LogicalKeyboardKey.keyT) {
      WindowUtils.toggleAlwaysOnTop();
      widget.showToast(
        WindowUtils.isAlwaysOnTop() ? s.toastAlwaysOnTopOn : s.toastAlwaysOnTopOff,
      );
      return;
    }

    // Atalho 'F12': Alterna monitoramento de áudio
    if (key == LogicalKeyboardKey.f12) {
      final audioManager = AudioManager();
      if (audioManager.selectedDeviceId != null) {
        audioManager.setLoopback(!audioManager.isLoopbackEnabled);
        widget.showToast(
          audioManager.isLoopbackEnabled
              ? s.toastAudioMonitorOn
              : s.toastAudioMonitorOff,
        );
      } else {
        widget.showToast(s.toastNoAudioDevice);
      }
      return;
    }

    // Atalhos '+' e '-': Brilho
    if (key == LogicalKeyboardKey.equal ||
        key == LogicalKeyboardKey.numpadAdd ||
        key == LogicalKeyboardKey.add) {
      final videoManager = VideoPreviewManager();
      videoManager.setBrightness(videoManager.brightnessOffset + 5);
      widget.showToast(s.toastBrightness(videoManager.brightnessOffset));
      return;
    }
    if (key == LogicalKeyboardKey.minus ||
        key == LogicalKeyboardKey.numpadSubtract) {
      final videoManager = VideoPreviewManager();
      videoManager.setBrightness(videoManager.brightnessOffset - 5);
      widget.showToast(s.toastBrightness(videoManager.brightnessOffset));
      return;
    }

    // Atalhos '1' a '9': Seleção rápida de canal de vídeo
    if (key.keyId >= LogicalKeyboardKey.digit1.keyId &&
        key.keyId <= LogicalKeyboardKey.digit9.keyId) {
      final index = key.keyId - LogicalKeyboardKey.digit1.keyId; // 0 a 8
      final videoManager = VideoPreviewManager();
      if (index < videoManager.videoDevices.length) {
        final device = videoManager.videoDevices[index];
        final id = device['id']!;
        final name = device['name']!;

        videoManager.startPreview(id, name).then((_) {
          widget.showToast(s.toastChannel(index + 1, name));
          _autoMatchAudio(name);
        });
      }
      return;
    }
  }

  /// Delegates to [AudioManager.tryAutoMatch] — single implementation, no duplication.
  void _autoMatchAudio(String cameraName) {
    final s = LocaleNotifier.instance.s;
    AudioManager().tryAutoMatch(cameraName).then((result) {
      if (result.audioName != null) {
        widget.showToast(
          result.isMono
              ? s.toastAudioMonoPairedMsg(result.audioName!)
              : s.toastAudioPaired(result.audioName!),
        );
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return KeyboardListener(
      focusNode: _focusNode,
      onKeyEvent: _handleKeyEvent,
      autofocus: true,
      child: widget.child,
    );
  }
}
