import 'dart:io';
import 'dart:ui';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'video_preview.dart';
import 'audio_manager.dart';
import 'native_bindings.dart';
import 'l10n.dart';

class DeviceSelectorOverlay extends StatefulWidget {
  final VoidCallback onClose;
  final Function(String msg) showToast;

  const DeviceSelectorOverlay({
    Key? key,
    required this.onClose,
    required this.showToast,
  }) : super(key: key);

  @override
  State<DeviceSelectorOverlay> createState() => _DeviceSelectorOverlayState();
}

class _DeviceSelectorOverlayState extends State<DeviceSelectorOverlay> {
  final List<Map<String, String>> _audioDevices = [];
  final List<Map<String, String>> _audioOutputDevices = [];

  @override
  void initState() {
    super.initState();
    _refreshAudioDevices();
  }

  void _refreshAudioDevices() {
    _audioDevices.clear();
    _audioOutputDevices.clear();
    if (!NativeBindings.isReady) return;
    
    final count = NativeBindings.enumAudioDevices();
    for (int i = 0; i < count; i++) {
      _audioDevices.add({
        'name': NativeBindings.getAudioDeviceName(i),
        'id': NativeBindings.getAudioDeviceId(i),
      });
    }

    final outCount = NativeBindings.enumAudioOutputDevices();
    for (int i = 0; i < outCount; i++) {
      _audioOutputDevices.add({
        'name': NativeBindings.getAudioOutputDeviceName(i),
        'id': NativeBindings.getAudioOutputDeviceId(i),
      });
    }

    setState(() {});
  }

  Widget _buildDropdown({
    required String label,
    required IconData icon,
    required Color color,
    required List<Map<String, String>> items,
    required String? selectedId,
    required void Function(String?) onChanged,
    required String noItemsText,
  }) {
    // If the selectedId is not in the list (e.g. unplugged), we just show null
    final hasSelectedItem = selectedId != null && items.any((e) => e['id'] == selectedId);
    final value = hasSelectedItem ? selectedId : (items.isNotEmpty ? items.first['id'] : null);

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
                value: value,
                onChanged: onChanged,
                items: items.map((device) {
                  final id = device['id']!;
                  final name = device['name']!;
                  return DropdownMenuItem<String>(
                    value: id,
                    child: Row(
                      children: [
                        Icon(icon, color: selectedId == id ? color : Colors.grey, size: 16),
                        const SizedBox(width: 8),
                        Expanded(
                          child: Text(
                            name,
                            style: TextStyle(
                              color: selectedId == id ? Colors.white : Colors.grey[300],
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



  void _confirmExitApp() {
    final s = LocaleNotifier.instance.s;
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF1E1E1E),
        title: Text(s.confirmExitTitle,
            style: const TextStyle(color: Colors.white)),
        content: Text(
          s.confirmExitMsg,
          style: const TextStyle(color: Colors.grey),
        ),
        actions: [
          TextButton(
            child: Text(s.cancelLabel,
                style: const TextStyle(color: Colors.grey)),
            onPressed: () => Navigator.pop(context),
          ),
          ElevatedButton(
            style:
                ElevatedButton.styleFrom(backgroundColor: Colors.redAccent),
            child: Text(s.exitLabel,
                style: const TextStyle(color: Colors.white)),
            onPressed: () {
              Navigator.pop(context);
              exit(0);
            },
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final s = LocaleNotifier.instance.s;
    final videoManager = VideoPreviewManager();
    final audioManager = AudioManager();

    return Center(
      child: ClipRRect(
        borderRadius: BorderRadius.circular(16),
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 15, sigmaY: 15),
          child: Container(
            width: 480,
            height: 480,
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
                const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Header
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Row(
                      children: [
                        const Icon(Icons.tune,
                            color: Colors.blueAccent, size: 20),
                        const SizedBox(width: 8),
                        Text(
                          s.menuTitle,
                          style: const TextStyle(
                            color: Colors.white,
                            fontSize: 16,
                            fontWeight: FontWeight.bold,
                            letterSpacing: 0.2,
                          ),
                        ),
                      ],
                    ),
                    IconButton(
                      icon: const Icon(Icons.close,
                          color: Colors.grey, size: 20),
                      onPressed: widget.onClose,
                      hoverColor: Colors.white10,
                      splashRadius: 20,
                    )
                  ],
                ),
                const Divider(color: Colors.white12, height: 8),

                _buildDropdown(
                  label: s.videoSection,
                  icon: Icons.videocam,
                  color: Colors.blueAccent,
                  items: videoManager.videoDevices,
                  selectedId: videoManager.selectedDeviceId,
                  noItemsText: s.noVideoDevices,
                  onChanged: (id) {
                    if (id != null) {
                      final name = videoManager.videoDevices.firstWhere((e) => e['id'] == id)['name']!;
                      videoManager.startPreview(id, name).then((_) {
                        widget.showToast(s.toastVideo(name));
                        setState(() {});
                      });
                    }
                  },
                ),
                const SizedBox(height: 12),
                
                _buildDropdown(
                  label: s.audioSection,
                  icon: Icons.mic,
                  color: Colors.teal,
                  items: _audioDevices,
                  selectedId: audioManager.selectedDeviceId,
                  noItemsText: s.noAudioDevices,
                  onChanged: (id) {
                    if (id != null) {
                      final name = _audioDevices.firstWhere((e) => e['id'] == id)['name']!;
                      audioManager.startCapture(id, audioManager.isLoopbackEnabled).then((channels) {
                        if (channels == 1) {
                          widget.showToast(s.toastAudioMono2);
                        } else {
                          widget.showToast(s.toastAudio(name));
                        }
                        setState(() {});
                      });
                    }
                  },
                ),
                const SizedBox(height: 12),

                _buildDropdown(
                  label: s.audioOutputSection,
                  icon: Icons.speaker,
                  color: Colors.orangeAccent,
                  items: _audioOutputDevices,
                  selectedId: audioManager.selectedOutputDeviceId,
                  noItemsText: s.noAudioDevices,
                  onChanged: (id) {
                    if (id != null) {
                      audioManager.setOutputDevice(id).then((_) {
                        setState(() {});
                      });
                    }
                  },
                ),
                const SizedBox(height: 8),

                // Audio Monitoring Toggle and VU Meter
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: Colors.white.withOpacity(0.02),
                    borderRadius: BorderRadius.circular(8),
                    border:
                        Border.all(color: Colors.white.withOpacity(0.04)),
                  ),
                  child: Column(
                    children: [
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Row(
                            children: [
                              Icon(
                                audioManager.isLoopbackEnabled
                                    ? Icons.volume_up
                                    : Icons.volume_off,
                                color: audioManager.isLoopbackEnabled
                                    ? Colors.green
                                    : Colors.grey,
                                size: 18,
                              ),
                              const SizedBox(width: 8),
                              Text(
                                s.monitorAudioLabel,
                                style: const TextStyle(
                                    color: Colors.white, fontSize: 12),
                              ),
                            ],
                          ),
                          Switch(
                            activeColor: Colors.green,
                            value: audioManager.isLoopbackEnabled,
                            onChanged: audioManager.selectedDeviceId == null
                                ? null
                                : (val) {
                                    audioManager.setLoopback(val);
                                    widget.showToast(val
                                        ? s.toastAudioMonitorEnabled
                                        : s.toastAudioMonitorDisabled);
                                    setState(() {});
                                  },
                          ),
                        ],
                      ),
                      const SizedBox(height: 8),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Row(
                            children: [
                              Icon(
                                Icons.shuffle,
                                color: audioManager.isDeinterleaveEnabled
                                    ? Colors.tealAccent
                                    : Colors.grey,
                                size: 18,
                              ),
                              const SizedBox(width: 8),
                              Column(
                                crossAxisAlignment:
                                    CrossAxisAlignment.start,
                                children: [
                                  Text(
                                    s.stereoCorrectionLabel,
                                    style: const TextStyle(
                                        color: Colors.white, fontSize: 12),
                                  ),
                                  Text(
                                    s.stereoCorrectionDesc,
                                    style: const TextStyle(
                                        color: Colors.grey, fontSize: 9),
                                  ),
                                ],
                              ),
                            ],
                          ),
                          Switch(
                            activeColor: Colors.tealAccent,
                            value: audioManager.isDeinterleaveEnabled,
                            onChanged: (val) {
                              audioManager.setDeinterleave(val);
                              widget.showToast(val
                                  ? s.toastStereoOn
                                  : s.toastStereoOff);
                              setState(() {});
                            },
                          ),
                        ],
                      ),
                      const SizedBox(height: 6),
                      // VU Meter
                      AnimatedBuilder(
                        animation: audioManager,
                        builder: (context, child) {
                          final vol = audioManager.peakVolume;
                          return Column(
                            crossAxisAlignment: CrossAxisAlignment.stretch,
                            children: [
                              Row(
                                mainAxisAlignment:
                                    MainAxisAlignment.spaceBetween,
                                children: [
                                  Text(s.inputLevelLabel,
                                      style: const TextStyle(
                                          color: Colors.grey, fontSize: 10)),
                                  Text('${(vol * 100).toInt()}%',
                                      style: const TextStyle(
                                          color: Colors.grey, fontSize: 10)),
                                ],
                              ),
                              const SizedBox(height: 4),
                              ClipRRect(
                                borderRadius: BorderRadius.circular(2),
                                child: LinearProgressIndicator(
                                  value: vol,
                                  minHeight: 4,
                                  backgroundColor: Colors.white12,
                                  valueColor:
                                      AlwaysStoppedAnimation<Color>(
                                    vol > 0.8
                                        ? Colors.redAccent
                                        : vol > 0.5
                                            ? Colors.amberAccent
                                            : Colors.greenAccent,
                                  ),
                                ),
                              ),
                            ],
                          );
                        },
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 8),

                // Footer Actions
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    TextButton.icon(
                      icon: const Icon(Icons.exit_to_app,
                          color: Colors.redAccent, size: 16),
                      label: Text(s.closeAppLabel,
                          style: const TextStyle(
                              color: Colors.redAccent, fontSize: 12)),
                      onPressed: _confirmExitApp,
                    ),
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
}
