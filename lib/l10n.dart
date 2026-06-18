import 'dart:io';
import 'package:flutter/foundation.dart';

// ============================================================
// Locale enum
// ============================================================

enum AppLocale { en, pt }

// ============================================================
// Abstract interface — all user-visible strings
// ============================================================

abstract class AppStrings {
  // ---- General ----
  String get appTitle;
  String get noVideoActive;
  String get pressM;

  // ---- Device Selector ----
  String get menuTitle;
  String get videoSection;
  String get audioSection;
  String get monitorAudioLabel;
  String get stereoCorrectionLabel;
  String get stereoCorrectionDesc;
  String get inputLevelLabel;
  String get closeAppLabel;
  String get doneLabel;
  String get confirmExitTitle;
  String get confirmExitMsg;
  String get cancelLabel;
  String get exitLabel;
  String get noVideoDevices;
  String get noAudioDevices;
  String toastVideo(String name);
  String toastAudio(String name);
  String toastAudioMonoPaired(String name);
  String get toastAudioMono2;
  String get toastAudioMonitorEnabled;
  String get toastAudioMonitorDisabled;
  String get toastStereoOn;
  String get toastStereoOff;

  // ---- Help Overlay ----
  String get helpTitle;
  String get shortcutsHeader;
  String get pressEscToClose;
  String get languageLabel;
  // Shortcut rows
  String get skMenuTitle;
  String get skMenuDesc;
  String get skHelpTitle;
  String get skHelpDesc;
  String get skResTitle;
  String get skResDesc;
  String get skFullscreenTitle;
  String get skFullscreenDesc;
  String get skEscTitle;
  String get skEscDesc;
  String get skTopTitle;
  String get skTopDesc;
  String get skScreenshotTitle;
  String get skScreenshotDesc;
  String get skBrightnessTitle;
  String get skBrightnessDesc;
  String get skAudioTitle;
  String get skAudioDesc;
  String get skChannelTitle;
  String get skChannelDesc;

  // ---- Resolution Selector ----
  String get resolutionTitle;
  String get currentlyActiveLabel;
  String get availableResolutionsLabel;
  String get limitFpsLabel;
  String get nativeLabel;
  String get unavailableLabel;
  String get noCaptureLabel;
  String get noResolutionsLabel;
  String get toastChangingFormat;
  String toastFormatChanged(int w, int h, String fps);
  String get toastFormatError;
  String get toastFpsUnavailable;

  // ---- Key Shortcuts Toasts ----
  String get toastWindowRestored;
  String get toastFullscreen;
  String toastScreenshotSaved(String filename);
  String get toastScreenshotError;
  String get toastNoPreview;
  String get toastAlwaysOnTopOn;
  String get toastAlwaysOnTopOff;
  String get toastAudioMonitorOn;
  String get toastAudioMonitorOff;
  String get toastNoAudioDevice;
  String toastBrightness(int value);
  String toastChannel(int n, String name);
  String toastAudioPaired(String name);
  String toastAudioMonoPairedMsg(String name);
}

// ============================================================
// English strings
// ============================================================

class _AppStringsEn implements AppStrings {
  const _AppStringsEn();

  @override String get appTitle => 'VDisPlay';
  @override String get noVideoActive => 'No active video device';
  @override String get pressM => 'Press [M] to open settings';

  @override String get menuTitle => 'Devices & Settings';
  @override String get videoSection => 'VIDEO (Press 1-9)';
  @override String get audioSection => 'AUDIO';
  @override String get monitorAudioLabel => 'Monitor Audio (Speakers)';
  @override String get stereoCorrectionLabel => 'Stereo Correction (HDMI Capture)';
  @override String get stereoCorrectionDesc => 'For 96kHz mono capture cards (MS2109)';
  @override String get inputLevelLabel => 'Input Level';
  @override String get closeAppLabel => 'Close App';
  @override String get doneLabel => 'Done';
  @override String get confirmExitTitle => 'Exit VDisPlay?';
  @override String get confirmExitMsg => 'Are you sure you want to close the application?';
  @override String get cancelLabel => 'Cancel';
  @override String get exitLabel => 'Exit';
  @override String get noVideoDevices => 'No video devices available';
  @override String get noAudioDevices => 'No audio devices available';
  @override String toastVideo(String name) => 'Video: $name';
  @override String toastAudio(String name) => 'Audio: $name';
  @override String toastAudioMonoPaired(String name) =>
      'Audio paired: $name (MONO! Enable Stereo in Windows)';
  @override String get toastAudioMono2 =>
      'MONO Audio! Enable Stereo in Windows properties.';
  @override String get toastAudioMonitorEnabled => 'Audio Monitoring Enabled';
  @override String get toastAudioMonitorDisabled => 'Audio Monitoring Disabled';
  @override String get toastStereoOn => 'Stereo Correction Enabled';
  @override String get toastStereoOff => 'Stereo Correction Disabled';

  @override String get helpTitle => 'VDisPlay Help';
  @override String get shortcutsHeader => 'KEYBOARD SHORTCUTS';
  @override String get pressEscToClose => 'Press [ESC] or click outside to close';
  @override String get languageLabel => 'Language / Idioma';
  @override String get skMenuTitle => 'Device Menu';
  @override String get skMenuDesc =>
      'Opens or closes the video and audio selection panel.';
  @override String get skHelpTitle => 'Help / Shortcuts';
  @override String get skHelpDesc =>
      'Displays this information and shortcuts panel.';
  @override String get skResTitle => 'Resolution & FPS';
  @override String get skResDesc =>
      'Opens the menu to adjust resolution and FPS limit.';
  @override String get skFullscreenTitle => 'Toggle Fullscreen';
  @override String get skFullscreenDesc =>
      'Enters or exits fullscreen mode (borderless).';
  @override String get skEscTitle => 'Close / Back';
  @override String get skEscDesc =>
      'Closes active menus or exits fullscreen.';
  @override String get skTopTitle => 'Always on Top';
  @override String get skTopDesc =>
      'Pins or unpins the window above all others.';
  @override String get skScreenshotTitle => 'Screenshot';
  @override String get skScreenshotDesc =>
      'Saves an instant PNG screenshot of the capture.';
  @override String get skBrightnessTitle => 'Adjust Brightness';
  @override String get skBrightnessDesc =>
      'Increases or decreases image brightness in real time.';
  @override String get skAudioTitle => 'Monitor Audio';
  @override String get skAudioDesc =>
      'Turns capture card audio output on/off.';
  @override String get skChannelTitle => 'Switch Video';
  @override String get skChannelDesc =>
      'Quickly selects a video device by index.';

  @override String get resolutionTitle => 'Video Format';
  @override String get currentlyActiveLabel => 'CURRENTLY ACTIVE';
  @override String get availableResolutionsLabel => 'AVAILABLE RESOLUTIONS';
  @override String get limitFpsLabel => 'LIMIT FRAME RATE (FPS)';
  @override String get nativeLabel => 'Native';
  @override String get unavailableLabel => 'Unavailable';
  @override String get noCaptureLabel => 'No active video device';
  @override String get noResolutionsLabel => 'No resolutions available';
  @override String get toastChangingFormat => 'Changing format...';
  @override String toastFormatChanged(int w, int h, String fps) =>
      'Format changed to: $w x $h @ $fps';
  @override String get toastFormatError => 'Error changing format';
  @override String get toastFpsUnavailable =>
      'FPS not available at this resolution';

  @override String get toastWindowRestored => 'Window Restored';
  @override String get toastFullscreen => 'Fullscreen';
  @override String toastScreenshotSaved(String filename) =>
      'Screenshot saved: $filename';
  @override String get toastScreenshotError => 'Screenshot error';
  @override String get toastNoPreview => 'No active preview for screenshot';
  @override String get toastAlwaysOnTopOn => 'Always on Top: ON';
  @override String get toastAlwaysOnTopOff => 'Always on Top: OFF';
  @override String get toastAudioMonitorOn => 'Audio Monitor: ON';
  @override String get toastAudioMonitorOff => 'Audio Monitor: OFF';
  @override String get toastNoAudioDevice => 'No active audio device';
  @override String toastBrightness(int value) =>
      'Brightness: ${value > 0 ? '+' : ''}$value%';
  @override String toastChannel(int n, String name) => 'Channel $n: $name';
  @override String toastAudioPaired(String name) => 'Audio paired: $name';
  @override String toastAudioMonoPairedMsg(String name) =>
      'Audio paired: $name (MONO! Enable Stereo in Windows)';
}

// ============================================================
// Portuguese strings
// ============================================================

class _AppStringsPt implements AppStrings {
  const _AppStringsPt();

  @override String get appTitle => 'VDisPlay';
  @override String get noVideoActive => 'Nenhum dispositivo de vídeo ativo';
  @override String get pressM =>
      'Pressione [M] para abrir as configurações';

  @override String get menuTitle => 'Dispositivos e Ajustes';
  @override String get videoSection => 'VÍDEO (Pressione 1-9)';
  @override String get audioSection => 'ÁUDIO';
  @override String get monitorAudioLabel => 'Monitorar Áudio (Alto-falantes)';
  @override String get stereoCorrectionLabel => 'Correção Estéreo (Placa HDMI)';
  @override String get stereoCorrectionDesc =>
      'Para placas de 96kHz mono (MS2109)';
  @override String get inputLevelLabel => 'Nível de Entrada';
  @override String get closeAppLabel => 'Fechar App';
  @override String get doneLabel => 'Pronto';
  @override String get confirmExitTitle => 'Sair do VDisPlay?';
  @override String get confirmExitMsg =>
      'Tem certeza de que deseja fechar o aplicativo?';
  @override String get cancelLabel => 'Cancelar';
  @override String get exitLabel => 'Sair';
  @override String get noVideoDevices =>
      'Nenhum dispositivo de vídeo disponível';
  @override String get noAudioDevices =>
      'Nenhum dispositivo de áudio disponível';
  @override String toastVideo(String name) => 'Vídeo: $name';
  @override String toastAudio(String name) => 'Áudio: $name';
  @override String toastAudioMonoPaired(String name) =>
      'Áudio pareado: $name (MONO! Ative Estéreo no Windows)';
  @override String get toastAudioMono2 =>
      'Áudio MONO! Ative Estéreo nas propriedades do Windows.';
  @override String get toastAudioMonitorEnabled =>
      'Monitoramento de Áudio Ativado';
  @override String get toastAudioMonitorDisabled =>
      'Monitoramento de Áudio Desativado';
  @override String get toastStereoOn => 'Correção Estéreo Ativada';
  @override String get toastStereoOff => 'Correção Estéreo Desativada';

  @override String get helpTitle => 'Ajuda do VDisPlay';
  @override String get shortcutsHeader => 'TECLAS DE ATALHO';
  @override String get pressEscToClose =>
      'Pressione [ESC] ou clique fora para fechar';
  @override String get languageLabel => 'Language / Idioma';
  @override String get skMenuTitle => 'Menu de Dispositivos';
  @override String get skMenuDesc =>
      'Abre ou fecha o painel de seleção de vídeo e áudio.';
  @override String get skHelpTitle => 'Ajuda / Atalhos';
  @override String get skHelpDesc =>
      'Exibe este painel de informações e atalhos.';
  @override String get skResTitle => 'Resolução e FPS';
  @override String get skResDesc =>
      'Abre o menu para ajustar a resolução e o limite de FPS.';
  @override String get skFullscreenTitle => 'Alternar Tela Cheia';
  @override String get skFullscreenDesc =>
      'Entra ou sai do modo tela cheia (borderless).';
  @override String get skEscTitle => 'Fechar / Voltar';
  @override String get skEscDesc =>
      'Fecha menus ativos ou sai do modo tela cheia.';
  @override String get skTopTitle => 'Sempre no Topo';
  @override String get skTopDesc =>
      'Fixa ou desafixa a janela sobre todas as outras.';
  @override String get skScreenshotTitle => 'Captura de Tela';
  @override String get skScreenshotDesc =>
      'Salva um screenshot PNG instantâneo da captura.';
  @override String get skBrightnessTitle => 'Ajustar Brilho';
  @override String get skBrightnessDesc =>
      'Aumenta ou diminui o brilho da imagem em tempo real.';
  @override String get skAudioTitle => 'Monitorar Áudio';
  @override String get skAudioDesc =>
      'Liga/desliga a saída de som da placa no sistema.';
  @override String get skChannelTitle => 'Trocar Vídeo';
  @override String get skChannelDesc =>
      'Seleciona rapidamente o dispositivo de vídeo pelo índice.';

  @override String get resolutionTitle => 'Formato de Vídeo';
  @override String get currentlyActiveLabel => 'ATIVO ATUALMENTE';
  @override String get availableResolutionsLabel => 'RESOLUÇÕES DISPONÍVEIS';
  @override String get limitFpsLabel => 'LIMITAR TAXA DE QUADROS (FPS)';
  @override String get nativeLabel => 'Nativo';
  @override String get unavailableLabel => 'Indisponível';
  @override String get noCaptureLabel => 'Nenhum dispositivo de vídeo ativo';
  @override String get noResolutionsLabel => 'Nenhuma resolução disponível';
  @override String get toastChangingFormat => 'Alterando formato...';
  @override String toastFormatChanged(int w, int h, String fps) =>
      'Formato alterado para: $w x $h @ $fps';
  @override String get toastFormatError => 'Erro ao alterar formato';
  @override String get toastFpsUnavailable =>
      'FPS não disponível nesta resolução';

  @override String get toastWindowRestored => 'Janela Restaurada';
  @override String get toastFullscreen => 'Tela Cheia';
  @override String toastScreenshotSaved(String filename) =>
      'Screenshot salva em: $filename';
  @override String get toastScreenshotError => 'Erro ao tirar screenshot';
  @override String get toastNoPreview =>
      'Nenhum preview ativo para screenshot';
  @override String get toastAlwaysOnTopOn => 'Sempre no topo: ATIVADO';
  @override String get toastAlwaysOnTopOff => 'Sempre no topo: DESATIVADO';
  @override String get toastAudioMonitorOn => 'Monitoramento de Áudio: ATIVADO';
  @override String get toastAudioMonitorOff =>
      'Monitoramento de Áudio: DESATIVADO';
  @override String get toastNoAudioDevice =>
      'Nenhum dispositivo de áudio ativo';
  @override String toastBrightness(int value) =>
      'Brilho: ${value > 0 ? '+' : ''}$value%';
  @override String toastChannel(int n, String name) => 'Canal $n: $name';
  @override String toastAudioPaired(String name) => 'Áudio pareado: $name';
  @override String toastAudioMonoPairedMsg(String name) =>
      'Áudio pareado: $name (MONO! Ative Estéreo no Windows)';
}

// ============================================================
// LocaleNotifier — singleton ChangeNotifier
// ============================================================

class LocaleNotifier extends ChangeNotifier {
  static final LocaleNotifier _instance = LocaleNotifier._internal();
  factory LocaleNotifier() => _instance;

  /// Direct access without creating an instance via the factory.
  static LocaleNotifier get instance => _instance;

  LocaleNotifier._internal() {
    _locale = _detectSystemLocale();
  }

  late AppLocale _locale;

  AppLocale get locale => _locale;

  /// Returns the current string table.
  AppStrings get s =>
      _locale == AppLocale.pt ? const _AppStringsPt() : const _AppStringsEn();

  void setLocale(AppLocale locale) {
    if (_locale == locale) return;
    _locale = locale;
    notifyListeners();
  }

  void toggleLocale() {
    setLocale(_locale == AppLocale.pt ? AppLocale.en : AppLocale.pt);
  }

  /// Detects the OS locale at startup.
  /// Falls back to English if detection fails.
  static AppLocale _detectSystemLocale() {
    try {
      final lang = Platform.localeName.split('_').first.toLowerCase();
      if (lang == 'pt') return AppLocale.pt;
    } catch (_) {
      // Platform.localeName not available — use English
    }
    return AppLocale.en;
  }
}
