import 'dart:ui';
import 'package:flutter/material.dart';
import 'video_preview.dart';
import 'l10n.dart';

class ResolutionSelectorOverlay extends StatefulWidget {
  final VoidCallback onClose;
  final Function(String msg) showToast;

  const ResolutionSelectorOverlay({
    Key? key,
    required this.onClose,
    required this.showToast,
  }) : super(key: key);

  @override
  State<ResolutionSelectorOverlay> createState() =>
      _ResolutionSelectorOverlayState();
}

class _ResolutionSelectorOverlayState
    extends State<ResolutionSelectorOverlay> {
  final VideoPreviewManager _videoManager = VideoPreviewManager();

  @override
  void initState() {
    super.initState();
    _videoManager.addListener(_onManagerUpdate);
    // Also rebuild when the locale changes
    LocaleNotifier.instance.addListener(_onManagerUpdate);
  }

  @override
  void dispose() {
    _videoManager.removeListener(_onManagerUpdate);
    LocaleNotifier.instance.removeListener(_onManagerUpdate);
    super.dispose();
  }

  void _onManagerUpdate() {
    if (mounted) setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    final s = LocaleNotifier.instance.s;
    final available = _videoManager.availableResolutions;
    final activeWidth = _videoManager.selectedWidth;
    final activeHeight = _videoManager.selectedHeight;
    final activeFps = _videoManager.currentFps;
    final activeFpsPreference = _videoManager.fpsPreference;
    final hasCapture = _videoManager.isCapturing;

    return Center(
      child: ClipRRect(
        borderRadius: BorderRadius.circular(16),
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 15, sigmaY: 15),
          child: Container(
            width: 460,
            height: 520,
            decoration: BoxDecoration(
              color: const Color(0xEC121212),
              borderRadius: BorderRadius.circular(16),
              border: Border.all(
                color: Colors.white.withOpacity(0.08),
                width: 1,
              ),
              boxShadow: [
                BoxShadow(
                  color: Colors.black.withOpacity(0.6),
                  blurRadius: 40,
                  spreadRadius: 2,
                )
              ],
            ),
            padding:
                const EdgeInsets.symmetric(horizontal: 24, vertical: 20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Header
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Row(
                      children: [
                        const Icon(Icons.settings_overscan,
                            color: Colors.blueAccent, size: 22),
                        const SizedBox(width: 8),
                        Text(
                          s.resolutionTitle,
                          style: const TextStyle(
                            color: Colors.white,
                            fontSize: 17,
                            fontWeight: FontWeight.bold,
                            letterSpacing: 0.2,
                          ),
                        ),
                      ],
                    ),
                    IconButton(
                      icon: const Icon(Icons.close, color: Colors.white54),
                      onPressed: widget.onClose,
                      hoverColor: Colors.white.withOpacity(0.05),
                      splashRadius: 20,
                    ),
                  ],
                ),
                const Divider(color: Colors.white10, height: 20),

                if (!hasCapture) ...[
                  Expanded(
                    child: Center(
                      child: Text(
                        s.noCaptureLabel,
                        style: const TextStyle(
                            color: Colors.grey, fontSize: 13),
                      ),
                    ),
                  )
                ] else ...[
                  // Resolução Ativa
                  Container(
                    padding: const EdgeInsets.symmetric(
                        horizontal: 14, vertical: 12),
                    decoration: BoxDecoration(
                      color: Colors.white.withOpacity(0.03),
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(
                          color: Colors.white.withOpacity(0.06)),
                    ),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          s.currentlyActiveLabel,
                          style: const TextStyle(
                            color: Colors.blueAccent,
                            fontSize: 10,
                            fontWeight: FontWeight.bold,
                            letterSpacing: 0.5,
                          ),
                        ),
                        const SizedBox(height: 6),
                        Row(
                          mainAxisAlignment:
                              MainAxisAlignment.spaceBetween,
                          children: [
                            Text(
                              '$activeWidth x $activeHeight',
                              style: const TextStyle(
                                color: Colors.white,
                                fontSize: 18,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            Container(
                              padding: const EdgeInsets.symmetric(
                                  horizontal: 8, vertical: 4),
                              decoration: BoxDecoration(
                                color: Colors.greenAccent.withOpacity(0.1),
                                borderRadius: BorderRadius.circular(6),
                                border: Border.all(
                                    color:
                                        Colors.greenAccent.withOpacity(0.2)),
                              ),
                              child: Text(
                                '${activeFps.toStringAsFixed(2)} FPS',
                                style: const TextStyle(
                                  color: Colors.greenAccent,
                                  fontSize: 12,
                                  fontWeight: FontWeight.bold,
                                ),
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(height: 18),

                  // Lista de resoluções
                  Text(
                    s.availableResolutionsLabel,
                    style: const TextStyle(
                      color: Colors.grey,
                      fontSize: 11,
                      fontWeight: FontWeight.bold,
                      letterSpacing: 0.5,
                    ),
                  ),
                  const SizedBox(height: 8),

                  Expanded(
                    child: Container(
                      decoration: BoxDecoration(
                        color: Colors.black26,
                        borderRadius: BorderRadius.circular(10),
                        border: Border.all(
                            color: Colors.white.withOpacity(0.04)),
                      ),
                      child: available.isEmpty
                          ? Center(
                              child: Text(
                                s.noResolutionsLabel,
                                style: const TextStyle(
                                    color: Colors.grey, fontSize: 12),
                              ),
                            )
                          : Scrollbar(
                              child: ListView.builder(
                                shrinkWrap: true,
                                itemCount: available.length,
                                physics: const BouncingScrollPhysics(),
                                itemBuilder: (context, index) {
                                  final size = available[index];
                                  final w = size.width.toInt();
                                  final h = size.height.toInt();
                                  final isSelected =
                                      (w == activeWidth && h == activeHeight);

                                  return Material(
                                    color: Colors.transparent,
                                    child: ListTile(
                                      title: Text(
                                        '$w x $h',
                                        style: TextStyle(
                                          color: isSelected
                                              ? Colors.blueAccent
                                              : Colors.white70,
                                          fontSize: 14,
                                          fontWeight: isSelected
                                              ? FontWeight.bold
                                              : FontWeight.normal,
                                        ),
                                      ),
                                      trailing: isSelected
                                          ? const Icon(Icons.check_circle,
                                              color: Colors.blueAccent,
                                              size: 18)
                                          : null,
                                      dense: true,
                                      visualDensity: VisualDensity.compact,
                                      onTap: () {
                                        if (!isSelected) {
                                          _changeFormat(
                                              w, h, activeFpsPreference);
                                        }
                                      },
                                      selected: isSelected,
                                      hoverColor:
                                          Colors.white.withOpacity(0.03),
                                    ),
                                  );
                                },
                              ),
                            ),
                    ),
                  ),
                  const SizedBox(height: 16),

                  // Seletor de FPS
                  Text(
                    s.limitFpsLabel,
                    style: const TextStyle(
                      color: Colors.grey,
                      fontSize: 11,
                      fontWeight: FontWeight.bold,
                      letterSpacing: 0.5,
                    ),
                  ),
                  const SizedBox(height: 8),

                  Row(
                    children: [
                      Expanded(
                          child: _buildFpsButton(
                              s.nativeLabel, 0, activeFpsPreference,
                              activeWidth, activeHeight, s)),
                      const SizedBox(width: 8),
                      Expanded(
                          child: _buildFpsButton(
                              '30 FPS', 30, activeFpsPreference,
                              activeWidth, activeHeight, s)),
                      const SizedBox(width: 8),
                      Expanded(
                          child: _buildFpsButton(
                              '60 FPS', 60, activeFpsPreference,
                              activeWidth, activeHeight, s)),
                    ],
                  ),
                ],
              ],
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildFpsButton(String label, int value, int activeValue,
      int width, int height, AppStrings s) {
    final isSelected = (value == activeValue);

    bool isSupported = true;
    if (value != 0) {
      isSupported = _videoManager.isFpsSupported(width, height, value);
    }

    final Color textColor = isSelected
        ? Colors.white
        : (isSupported ? Colors.white70 : Colors.white24);

    final Color borderClr = isSelected
        ? Colors.blueAccent
        : (isSupported
            ? Colors.white.withOpacity(0.08)
            : Colors.white.withOpacity(0.02));

    final Color bgClr =
        isSelected ? Colors.blueAccent.withOpacity(0.15) : Colors.transparent;

    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: isSupported
            ? () {
                if (!isSelected) {
                  _changeFormat(width, height, value);
                }
              }
            : () {
                widget.showToast(s.toastFpsUnavailable);
              },
        borderRadius: BorderRadius.circular(8),
        child: Container(
          padding: const EdgeInsets.symmetric(vertical: 10),
          decoration: BoxDecoration(
            color: bgClr,
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: borderClr, width: 1.2),
          ),
          child: Center(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  label,
                  style: TextStyle(
                    color: textColor,
                    fontSize: 13,
                    fontWeight:
                        isSelected ? FontWeight.bold : FontWeight.w500,
                  ),
                ),
                if (value != 0 && !isSupported) ...[
                  const SizedBox(height: 2),
                  Text(
                    s.unavailableLabel,
                    style: const TextStyle(
                      color: Colors.white24,
                      fontSize: 9,
                    ),
                  ),
                ],
              ],
            ),
          ),
        ),
      ),
    );
  }

  Future<void> _changeFormat(int width, int height, int fpsPref) async {
    final s = LocaleNotifier.instance.s;
    widget.showToast(s.toastChangingFormat);
    final success =
        await _videoManager.changeResolutionAndFps(width, height, fpsPref);
    if (success) {
      final fpsLabel = fpsPref == 0 ? s.nativeLabel : '$fpsPref FPS';
      widget.showToast(s.toastFormatChanged(width, height, fpsLabel));
    } else {
      widget.showToast(s.toastFormatError);
    }
  }
}
