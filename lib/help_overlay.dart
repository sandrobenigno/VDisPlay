import 'dart:ui';
import 'package:flutter/material.dart';
import 'l10n.dart';

class HelpOverlay extends StatelessWidget {
  final VoidCallback onClose;

  const HelpOverlay({
    Key? key,
    required this.onClose,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    // ListenableBuilder ensures the overlay rebuilds immediately when the
    // user toggles the language from within this overlay.
    return ListenableBuilder(
      listenable: LocaleNotifier.instance,
      builder: (context, _) {
        final s = LocaleNotifier.instance.s;
        final locale = LocaleNotifier.instance.locale;

        return Center(
          child: ClipRRect(
            borderRadius: BorderRadius.circular(16),
            child: BackdropFilter(
              filter: ImageFilter.blur(sigmaX: 15, sigmaY: 15),
              child: Container(
                width: 500,
                height: 610,
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
                padding: const EdgeInsets.symmetric(
                    horizontal: 24, vertical: 20),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    // Header
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Row(
                          children: [
                            const Icon(Icons.help_outline,
                                color: Colors.blueAccent, size: 22),
                            const SizedBox(width: 8),
                            Text(
                              s.helpTitle,
                              style: const TextStyle(
                                color: Colors.white,
                                fontSize: 18,
                                fontWeight: FontWeight.bold,
                                letterSpacing: 0.2,
                              ),
                            ),
                          ],
                        ),
                        IconButton(
                          icon: const Icon(Icons.close,
                              color: Colors.white54),
                          onPressed: onClose,
                          hoverColor: Colors.white.withOpacity(0.05),
                          splashRadius: 20,
                        ),
                      ],
                    ),
                    const Divider(color: Colors.white10, height: 20),

                    // App Info
                    Container(
                      padding: const EdgeInsets.all(12),
                      decoration: BoxDecoration(
                        color: Colors.white.withOpacity(0.03),
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(
                            color: Colors.white.withOpacity(0.05)),
                      ),
                      child: const Column(
                        children: [
                          Text(
                            'VDisPlay V1.0',
                            style: TextStyle(
                              color: Colors.white,
                              fontSize: 15,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                          SizedBox(height: 4),
                          Text(
                            'By Sandro Benigno (2026)',
                            style:
                                TextStyle(color: Colors.grey, fontSize: 12),
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 12),

                    // Language Toggle
                    Container(
                      padding: const EdgeInsets.symmetric(
                          horizontal: 12, vertical: 8),
                      decoration: BoxDecoration(
                        color: Colors.white.withOpacity(0.02),
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(
                            color: Colors.white.withOpacity(0.04)),
                      ),
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Row(
                            children: [
                              const Icon(Icons.language,
                                  color: Colors.blueAccent, size: 16),
                              const SizedBox(width: 8),
                              Text(
                                s.languageLabel,
                                style: const TextStyle(
                                    color: Colors.white70, fontSize: 12),
                              ),
                            ],
                          ),
                          Row(
                            children: [
                              _buildLangButton(
                                label: 'EN',
                                isActive: locale == AppLocale.en,
                                onTap: () => LocaleNotifier.instance
                                    .setLocale(AppLocale.en),
                              ),
                              const SizedBox(width: 6),
                              _buildLangButton(
                                label: 'PT',
                                isActive: locale == AppLocale.pt,
                                onTap: () => LocaleNotifier.instance
                                    .setLocale(AppLocale.pt),
                              ),
                            ],
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 12),

                    // Shortcut list header
                    Text(
                      s.shortcutsHeader,
                      style: const TextStyle(
                        color: Colors.blueAccent,
                        fontSize: 11,
                        fontWeight: FontWeight.bold,
                        letterSpacing: 1.0,
                      ),
                    ),
                    const SizedBox(height: 8),

                    // Shortcuts Scrollable area
                    Expanded(
                      child: ListView(
                        physics: const BouncingScrollPhysics(),
                        children: [
                          _buildShortcutRow('M', s.skMenuTitle, s.skMenuDesc),
                          _buildShortcutRow('H', s.skHelpTitle, s.skHelpDesc),
                          _buildShortcutRow('R', s.skResTitle, s.skResDesc),
                          _buildShortcutRow('F / F11', s.skFullscreenTitle,
                              s.skFullscreenDesc),
                          _buildShortcutRow(
                              'ESC', s.skEscTitle, s.skEscDesc),
                          _buildShortcutRow('T', s.skTopTitle, s.skTopDesc),
                          _buildShortcutRow(
                              'S', s.skScreenshotTitle, s.skScreenshotDesc),
                          _buildShortcutRow('+ / -', s.skBrightnessTitle,
                              s.skBrightnessDesc),
                          _buildShortcutRow(
                              'F12', s.skAudioTitle, s.skAudioDesc),
                          _buildShortcutRow(
                              '1 – 9', s.skChannelTitle, s.skChannelDesc),
                        ],
                      ),
                    ),
                    const SizedBox(height: 12),

                    // Footer hint
                    Center(
                      child: Text(
                        s.pressEscToClose,
                        style: TextStyle(
                          color: Colors.white.withOpacity(0.3),
                          fontSize: 11,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        );
      },
    );
  }

  Widget _buildLangButton({
    required String label,
    required bool isActive,
    required VoidCallback onTap,
  }) {
    return GestureDetector(
      onTap: onTap,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 150),
        padding:
            const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
        decoration: BoxDecoration(
          color: isActive
              ? Colors.blueAccent.withOpacity(0.20)
              : Colors.white.withOpacity(0.04),
          borderRadius: BorderRadius.circular(6),
          border: Border.all(
            color: isActive
                ? Colors.blueAccent.withOpacity(0.60)
                : Colors.white.withOpacity(0.08),
          ),
        ),
        child: Text(
          label,
          style: TextStyle(
            color: isActive ? Colors.blueAccent : Colors.white54,
            fontSize: 12,
            fontWeight:
                isActive ? FontWeight.bold : FontWeight.normal,
          ),
        ),
      ),
    );
  }

  Widget _buildShortcutRow(
      String keyText, String title, String description) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8.0),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Key Badge
          Container(
            width: 75,
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 4),
            decoration: BoxDecoration(
              color: const Color(0xFF252525),
              borderRadius: BorderRadius.circular(6),
              border: Border.all(
                color: Colors.white.withOpacity(0.12),
                width: 1,
              ),
              boxShadow: const [
                BoxShadow(
                  color: Colors.black26,
                  offset: Offset(0, 1.5),
                  blurRadius: 1,
                )
              ],
            ),
            child: Center(
              child: Text(
                keyText,
                style: const TextStyle(
                  color: Colors.white,
                  fontFamily: 'Courier',
                  fontSize: 11,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ),
          ),
          const SizedBox(width: 16),
          // Shortcut Description
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 13,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  description,
                  style: const TextStyle(
                    color: Colors.white70,
                    fontSize: 11.5,
                    height: 1.3,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
