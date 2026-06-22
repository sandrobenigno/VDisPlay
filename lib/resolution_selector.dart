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

    final String? selectedValue = available.any((size) => size.width.toInt() == activeWidth && size.height.toInt() == activeHeight)
        ? '${activeWidth}x$activeHeight'
        : (available.isNotEmpty ? '${available.first.width.toInt()}x${available.first.height.toInt()}' : null);

    return Center(
      child: ClipRRect(
        borderRadius: BorderRadius.circular(16),
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 15, sigmaY: 15),
          child: Container(
            width: 440,
            height: hasCapture ? 400 : 240,
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
                  const SizedBox(height: 14),

                  // Dropdown de resoluções
                  _buildResolutionDropdown(
                    label: s.availableResolutionsLabel,
                    icon: Icons.aspect_ratio,
                    color: Colors.blueAccent,
                    items: available,
                    selectedValue: selectedValue,
                    noItemsText: s.noResolutionsLabel,
                    onChanged: (val) {
                      if (val != null) {
                        final parts = val.split('x');
                        if (parts.length == 2) {
                          final w = int.tryParse(parts[0]);
                          final h = int.tryParse(parts[1]);
                          if (w != null && h != null) {
                            _changeFormat(w, h, activeFpsPreference);
                          }
                        }
                      }
                    },
                  ),
                  const SizedBox(height: 14),

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
                  const SizedBox(height: 6),

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
                const SizedBox(height: 14),
                // Footer Actions
                Row(
                  mainAxisAlignment: MainAxisAlignment.end,
                  children: [
                    ElevatedButton(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.blueAccent,
                        shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(8)),
                      ),
                      onPressed: widget.onClose,
                      child: Text(s.doneLabel,
                          style: const TextStyle(
                              color: Colors.white, fontSize: 12)),
                    )
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildResolutionDropdown({
    required String label,
    required IconData icon,
    required Color color,
    required List<Size> items,
    required String? selectedValue,
    required void Function(String?) onChanged,
    required String noItemsText,
  }) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(
          label,
          style: TextStyle(
            color: color,
            fontSize: 10,
            fontWeight: FontWeight.bold,
            letterSpacing: 1.0,
          ),
        ),
        const SizedBox(height: 6),
        if (items.isEmpty)
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 4.0),
            child: Text(
              noItemsText,
              style: const TextStyle(color: Colors.grey, fontSize: 12),
            ),
          )
        else
          Container(
            height: 38,
            decoration: BoxDecoration(
              color: Colors.white.withOpacity(0.08),
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: Colors.white.withOpacity(0.12)),
            ),
            padding: const EdgeInsets.symmetric(horizontal: 10),
            child: DropdownButtonHideUnderline(
              child: DropdownButton<String>(
                isExpanded: true,
                dropdownColor: const Color(0xFF1E1E1E),
                icon: Icon(Icons.arrow_drop_down, color: color),
                value: selectedValue,
                onChanged: onChanged,
                items: items.map((size) {
                  final w = size.width.toInt();
                  final h = size.height.toInt();
                  final val = '${w}x$h';
                  final isSelected = val == selectedValue;
                  return DropdownMenuItem<String>(
                    value: val,
                    child: Row(
                      children: [
                        Icon(icon, color: isSelected ? color : Colors.grey, size: 16),
                        const SizedBox(width: 8),
                        Expanded(
                          child: Text(
                            '$w x $h',
                            style: TextStyle(
                              color: isSelected ? Colors.white : Colors.grey[300],
                              fontSize: 13,
                            ),
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                          ),
                        ),
                      ],
                    ),
                  );
                }).toList(),
              ),
            ),
          ),
      ],
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
