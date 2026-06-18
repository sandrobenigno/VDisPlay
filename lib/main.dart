import 'dart:async';
import 'package:flutter/material.dart';
import 'video_preview.dart';
import 'audio_manager.dart';
import 'key_shortcuts.dart';
import 'device_selector.dart';
import 'window_utils.dart';
import 'native_bindings.dart';
import 'help_overlay.dart';
import 'resolution_selector.dart';
import 'l10n.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Initialise the native backend and load persisted preferences.
  await VideoPreviewManager().init();
  await AudioManager().init();

  // Wrap the app in a ListenableBuilder so that the entire widget tree
  // rebuilds when the user switches the language.
  runApp(
    ListenableBuilder(
      listenable: LocaleNotifier.instance,
      builder: (_, __) => const VDisPlayApp(),
    ),
  );
}

class VDisPlayApp extends StatelessWidget {
  const VDisPlayApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: WindowUtils.kWindowTitle,
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark().copyWith(
        scaffoldBackgroundColor: const Color(0xFF0F0F0F),
        colorScheme: ColorScheme.fromSeed(
          seedColor: Colors.blueAccent,
          brightness: Brightness.dark,
        ),
      ),
      home: const MainPreviewScreen(),
    );
  }
}

class MainPreviewScreen extends StatefulWidget {
  const MainPreviewScreen({Key? key}) : super(key: key);

  @override
  State<MainPreviewScreen> createState() => _MainPreviewScreenState();
}

class _MainPreviewScreenState extends State<MainPreviewScreen>
    with WidgetsBindingObserver {
  bool _isMenuOpen = false;
  bool _isHelpOpen = false;
  bool _isResolutionOpen = false;
  String? _toastMessage;
  Timer? _toastTimer;

  final VideoPreviewManager _videoManager = VideoPreviewManager();
  final AudioManager _audioManager = AudioManager();

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);

    _videoManager.addListener(_onManagerUpdate);
    _audioManager.addListener(_onManagerUpdate);

    // If there is no active video device on startup, open the menu automatically.
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!_videoManager.isCapturing) {
        setState(() => _isMenuOpen = true);
      }
    });
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _videoManager.removeListener(_onManagerUpdate);
    _audioManager.removeListener(_onManagerUpdate);
    _toastTimer?.cancel();
    super.dispose();
  }

  /// Called by the OS when the app lifecycle changes.
  /// On Windows, `detached` fires before the process exits, giving us a
  /// chance to properly release native handles.
  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.detached) {
      _audioManager.stopCapture();
      _videoManager.shutdown();
    }
  }

  void _onManagerUpdate() {
    if (mounted) setState(() {});
  }

  void _showToast(String message) {
    _toastTimer?.cancel();
    setState(() => _toastMessage = message);
    _toastTimer = Timer(const Duration(milliseconds: 1800), () {
      if (mounted) setState(() => _toastMessage = null);
    });
  }

  void _toggleMenu() {
    setState(() {
      _isMenuOpen = !_isMenuOpen;
      if (_isMenuOpen) {
        _isHelpOpen = false;
        _isResolutionOpen = false;
        _videoManager.refreshDevices();
      }
    });
  }

  void _toggleHelp() {
    setState(() {
      _isHelpOpen = !_isHelpOpen;
      if (_isHelpOpen) {
        _isMenuOpen = false;
        _isResolutionOpen = false;
      }
    });
  }

  void _toggleResolution() {
    setState(() {
      _isResolutionOpen = !_isResolutionOpen;
      if (_isResolutionOpen) {
        _isMenuOpen = false;
        _isHelpOpen = false;
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final s = LocaleNotifier.instance.s;
    final hasPreview =
        _videoManager.isCapturing && _videoManager.textureId != null;
    final isFullscreen = WindowUtils.isFullscreen();

    // Dynamic aspect ratio
    double aspectRatio = 16 / 9;
    if (hasPreview && NativeBindings.isReady) {
      final w = NativeBindings.getVideoWidth();
      final h = NativeBindings.getVideoHeight();
      if (w > 0 && h > 0) aspectRatio = w / h;
    }

    return Scaffold(
      body: GlobalKeyboardShortcutHandler(
        onToggleMenu: _toggleMenu,
        isMenuOpen: _isMenuOpen,
        onToggleHelp: _toggleHelp,
        isHelpOpen: _isHelpOpen,
        onToggleResolution: _toggleResolution,
        isResolutionOpen: _isResolutionOpen,
        showToast: _showToast,
        child: Container(
          decoration: BoxDecoration(
            border: isFullscreen
                ? null
                : Border.all(color: const Color(0xFF252525), width: 1),
          ),
          child: Stack(
            children: [
              // 1. Tela Preta / Preview de Vídeo
              Positioned.fill(
                child: !hasPreview
                    ? _buildEmptyState(s)
                    : Center(
                        child: AspectRatio(
                          aspectRatio: aspectRatio,
                          child: Texture(textureId: _videoManager.textureId!),
                        ),
                      ),
              ),

              // 2. Marca d'água — indicador de canal e áudio (sutil)
              if (hasPreview && !_isMenuOpen)
                Positioned(
                  bottom: 16,
                  right: 16,
                  child: AnimatedOpacity(
                    opacity: 0.4,
                    duration: const Duration(milliseconds: 300),
                    child: Container(
                      padding: const EdgeInsets.symmetric(
                          horizontal: 10, vertical: 6),
                      decoration: BoxDecoration(
                        color: Colors.black54,
                        borderRadius: BorderRadius.circular(6),
                      ),
                      child: Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          if (_audioManager.isCapturing) ...[
                            Icon(
                              _audioManager.isLoopbackEnabled
                                  ? Icons.volume_up
                                  : Icons.volume_mute,
                              color: _audioManager.isLoopbackEnabled
                                  ? Colors.greenAccent
                                  : Colors.grey,
                              size: 14,
                            ),
                            const SizedBox(width: 6),
                          ],
                          Text(
                            _videoManager.selectedDeviceName ?? 'Preview',
                            style: const TextStyle(
                              color: Colors.white70,
                              fontSize: 11,
                              fontWeight: FontWeight.w500,
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),

              // 3. Toast de feedback rápido
              if (_toastMessage != null)
                Positioned(
                  top: 24,
                  left: 24,
                  child: Container(
                    padding: const EdgeInsets.symmetric(
                        horizontal: 14, vertical: 8),
                    decoration: BoxDecoration(
                      color: const Color(0xEC1E1E1E),
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(
                        color: Colors.blueAccent.withOpacity(0.4),
                        width: 1,
                      ),
                      boxShadow: [
                        BoxShadow(
                          color: Colors.black.withOpacity(0.4),
                          blurRadius: 10,
                        )
                      ],
                    ),
                    child: Text(
                      _toastMessage!,
                      style: const TextStyle(
                        color: Colors.white,
                        fontSize: 12,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ),
                ),

              // 4. Overlay — Configurações (Menu M)
              if (_isMenuOpen)
                Positioned.fill(
                  child: GestureDetector(
                    onTap: _toggleMenu,
                    child: Container(
                      color: Colors.black45,
                      child: GestureDetector(
                        onTap: () {},
                        child: DeviceSelectorOverlay(
                          onClose: _toggleMenu,
                          showToast: _showToast,
                        ),
                      ),
                    ),
                  ),
                ),

              // 5. Overlay — Ajuda (Menu H)
              if (_isHelpOpen)
                Positioned.fill(
                  child: GestureDetector(
                    onTap: _toggleHelp,
                    child: Container(
                      color: Colors.black45,
                      child: GestureDetector(
                        onTap: () {},
                        child: HelpOverlay(onClose: _toggleHelp),
                      ),
                    ),
                  ),
                ),

              // 6. Overlay — Resolução/FPS (Menu R)
              if (_isResolutionOpen)
                Positioned.fill(
                  child: GestureDetector(
                    onTap: _toggleResolution,
                    child: Container(
                      color: Colors.black45,
                      child: GestureDetector(
                        onTap: () {},
                        child: ResolutionSelectorOverlay(
                          onClose: _toggleResolution,
                          showToast: _showToast,
                        ),
                      ),
                    ),
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildEmptyState(AppStrings s) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(
            Icons.videocam_off_outlined,
            size: 48,
            color: Colors.grey.withOpacity(0.4),
          ),
          const SizedBox(height: 16),
          Text(
            s.noVideoActive,
            style: const TextStyle(
              color: Colors.grey,
              fontSize: 14,
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 8),
          Container(
            padding:
                const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
            decoration: BoxDecoration(
              color: Colors.white.withOpacity(0.03),
              borderRadius: BorderRadius.circular(6),
              border: Border.all(color: Colors.white.withOpacity(0.05)),
            ),
            child: Text(
              s.pressM,
              style: const TextStyle(
                color: Colors.grey,
                fontSize: 11,
              ),
            ),
          ),
        ],
      ),
    );
  }
}
