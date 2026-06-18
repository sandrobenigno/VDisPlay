import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'video_preview.dart';
import 'audio_manager.dart';
import 'window_utils.dart';
import 'native_bindings.dart';

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
  State<GlobalKeyboardShortcutHandler> createState() => _GlobalKeyboardShortcutHandlerState();
}

class _GlobalKeyboardShortcutHandlerState extends State<GlobalKeyboardShortcutHandler> {
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
        setState(() {
          WindowUtils.exitFullscreen();
        });
        widget.showToast("Janela Restaurada");
      }
      return;
    }

    // Atalhos 'F' ou 'F11': Tela Cheia
    if (key == LogicalKeyboardKey.keyF || key == LogicalKeyboardKey.f11) {
      setState(() {
        WindowUtils.toggleFullscreen();
      });
      if (WindowUtils.isFullscreen()) {
        widget.showToast("Tela Cheia");
      } else {
        widget.showToast("Janela Restaurada");
      }
      return;
    }

    // Atalho 'S': Screenshot
    if (key == LogicalKeyboardKey.keyS) {
      final manager = VideoPreviewManager();
      if (manager.isCapturing) {
        manager.takeScreenshot().then((path) {
          if (path != null) {
            widget.showToast("Screenshot salva em: ${path.split('\\').last}");
          } else {
            widget.showToast("Erro ao tirar screenshot");
          }
        });
      } else {
        widget.showToast("Nenhum preview ativo para screenshot");
      }
      return;
    }

    // Atalho 'T': Always on Top
    if (key == LogicalKeyboardKey.keyT) {
      WindowUtils.toggleAlwaysOnTop();
      final onTop = WindowUtils.isAlwaysOnTop();
      widget.showToast(onTop ? "Sempre no topo: ATIVADO" : "Sempre no topo: DESATIVADO");
      return;
    }

    // Atalho 'F12': Alterna monitoramento de áudio nos alto-falantes
    if (key == LogicalKeyboardKey.f12) {
      final audioManager = AudioManager();
      if (audioManager.selectedDeviceId != null) {
        audioManager.setLoopback(!audioManager.isLoopbackEnabled);
        widget.showToast(audioManager.isLoopbackEnabled
            ? "Monitoramento de Áudio: ATIVADO"
            : "Monitoramento de Áudio: DESATIVADO");
      } else {
        widget.showToast("Nenhum dispositivo de áudio ativo");
      }
      return;
    }

    // Atalhos '+' e '-': Brilho
    if (key == LogicalKeyboardKey.equal || key == LogicalKeyboardKey.numpadAdd || key == LogicalKeyboardKey.add) {
      final manager = VideoPreviewManager();
      manager.setBrightness(manager.brightnessOffset + 5);
      widget.showToast("Brilho: ${manager.brightnessOffset > 0 ? '+' : ''}${manager.brightnessOffset}%");
      return;
    }
    if (key == LogicalKeyboardKey.minus || key == LogicalKeyboardKey.numpadSubtract) {
      final manager = VideoPreviewManager();
      manager.setBrightness(manager.brightnessOffset - 5);
      widget.showToast("Brilho: ${manager.brightnessOffset > 0 ? '+' : ''}${manager.brightnessOffset}%");
      return;
    }

    // Atalhos '1' a '9': Seleção rápida de canal de vídeo
    if (key.keyId >= LogicalKeyboardKey.digit1.keyId && key.keyId <= LogicalKeyboardKey.digit9.keyId) {
      final index = key.keyId - LogicalKeyboardKey.digit1.keyId; // 0 a 8
      final manager = VideoPreviewManager();
      if (index < manager.videoDevices.length) {
        final device = manager.videoDevices[index];
        final id = device['id']!;
        final name = device['name']!;
        
        manager.startPreview(id, name).then((_) {
          widget.showToast("Canal ${index + 1}: $name");
          
          // Matching de áudio inteligente automático ao trocar canal
          _autoMatchAudio(name);
        });
      }
      return;
    }
  }

  void _autoMatchAudio(String cameraName) {
    final audioManager = AudioManager();
    final videoManager = VideoPreviewManager();
    
    // Atualiza lista de áudio antes
    audioManager.stopCapture().then((_) {
      NativeBindings.enumAudioDevices();
      
      // Busca substring correspondente nos dispositivos de áudio
      String? matchedAudioId;
      String? matchedAudioName;
      
      final cleanCamName = cameraName.toLowerCase();
      
      final count = NativeBindings.enumAudioDevices();
      for (int i = 0; i < count; i++) {
        final aName = NativeBindings.getAudioDeviceName(i);
        final aId = NativeBindings.getAudioDeviceId(i);
        
        // Se o nome do áudio contém o nome da câmera ou vice versa
        if (aName.toLowerCase().contains(cleanCamName) || cleanCamName.contains(aName.toLowerCase())) {
          matchedAudioId = aId;
          matchedAudioName = aName;
          break;
        }
      }
      
      if (matchedAudioId != null) {
        audioManager.startCapture(matchedAudioId, audioManager.isLoopbackEnabled).then((channels) {
          if (channels == 1) {
            widget.showToast("Áudio pareado: $matchedAudioName (MONO! Ative Estéreo no Windows)");
          } else {
            widget.showToast("Áudio pareado: $matchedAudioName");
          }
        });
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
