import 'dart:ui';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'video_preview.dart';
import 'audio_manager.dart';
import 'native_bindings.dart';

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

  @override
  void initState() {
    super.initState();
    _refreshAudioDevices();
  }

  void _refreshAudioDevices() {
    _audioDevices.clear();
    final count = NativeBindings.enumAudioDevices();
    for (int i = 0; i < count; i++) {
      _audioDevices.add({
        'name': NativeBindings.getAudioDeviceName(i),
        'id': NativeBindings.getAudioDeviceId(i),
      });
    }
    setState(() {});
  }

  void _autoMatchAudio(String cameraName) {
    final audioManager = AudioManager();
    final cleanCamName = cameraName.toLowerCase();
    
    _refreshAudioDevices();

    String? matchedAudioId;
    String? matchedAudioName;

    for (final device in _audioDevices) {
      final aName = device['name']!;
      final aId = device['id']!;
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
        setState(() {});
      });
    }
  }

  void _confirmExitApp() {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF1E1E1E),
        title: const Text("Sair do VDisPlay?", style: TextStyle(color: Colors.white)),
        content: const Text(
          "Tem certeza de que deseja fechar o aplicativo?",
          style: TextStyle(color: Colors.grey),
        ),
        actions: [
          TextButton(
            child: const Text("Cancelar", style: TextStyle(color: Colors.grey)),
            onPressed: () => Navigator.pop(context),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: Colors.redAccent),
            child: const Text("Sair", style: TextStyle(color: Colors.white)),
            onPressed: () {
              Navigator.pop(context);
              SystemNavigator.pop();
            },
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final videoManager = VideoPreviewManager();
    final audioManager = AudioManager();

    return Center(
      child: ClipRRect(
        borderRadius: BorderRadius.circular(16),
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 15, sigmaY: 15),
          child: Container(
            width: 480,
            height: 540,
            decoration: BoxDecoration(
              color: const Color(0xEC121212), // Dark transparent background
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
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Header
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    const Row(
                      children: [
                        Icon(Icons.tune, color: Colors.blueAccent, size: 20),
                        SizedBox(width: 8),
                        Text(
                          "Dispositivos e Ajustes",
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 16,
                            fontWeight: FontWeight.bold,
                            letterSpacing: 0.2,
                          ),
                        ),
                      ],
                    ),
                    IconButton(
                      icon: const Icon(Icons.close, color: Colors.grey, size: 20),
                      onPressed: widget.onClose,
                      hoverColor: Colors.white10,
                      splashRadius: 20,
                    )
                  ],
                ),
                 const Divider(color: Colors.white12, height: 8),

                // Video Section
                const Text(
                  "VÍDEO (Pressione 1-9)",
                  style: TextStyle(
                    color: Colors.blueAccent,
                    fontSize: 10,
                    fontWeight: FontWeight.bold,
                    letterSpacing: 1.0,
                  ),
                ),
                const SizedBox(height: 6),
                Expanded(
                  child: videoManager.videoDevices.isEmpty
                      ? const Center(
                          child: Text(
                            "Nenhum dispositivo de vídeo disponível",
                            style: TextStyle(color: Colors.grey, fontSize: 12),
                          ),
                        )
                      : ListView.builder(
                          itemCount: videoManager.videoDevices.length,
                          itemBuilder: (context, index) {
                            final device = videoManager.videoDevices[index];
                            final id = device['id']!;
                            final name = device['name']!;
                            final isSelected = videoManager.selectedDeviceId == id;

                            return Container(
                              margin: const EdgeInsets.symmetric(vertical: 1.0),
                              child: InkWell(
                                onTap: () {
                                  videoManager.startPreview(id, name).then((_) {
                                    widget.showToast("Vídeo: $name");
                                    _autoMatchAudio(name);
                                    setState(() {});
                                  });
                                },
                                borderRadius: BorderRadius.circular(8),
                                child: Container(
                                  padding: const EdgeInsets.symmetric(
                                      horizontal: 10, vertical: 6),
                                  decoration: BoxDecoration(
                                    color: isSelected
                                        ? Colors.blueAccent.withOpacity(0.20)
                                        : Colors.white.withOpacity(0.08),
                                    borderRadius: BorderRadius.circular(8),
                                    border: Border.all(
                                      color: isSelected
                                          ? Colors.blueAccent.withOpacity(0.60)
                                          : Colors.white.withOpacity(0.12),
                                    ),
                                  ),
                                  child: Row(
                                    children: [
                                      Container(
                                        width: 18,
                                        height: 18,
                                        decoration: BoxDecoration(
                                          color: isSelected
                                              ? Colors.blueAccent
                                              : Colors.white10,
                                          borderRadius: BorderRadius.circular(4),
                                        ),
                                        child: Center(
                                          child: Text(
                                            "${index + 1}",
                                            style: TextStyle(
                                              color: isSelected
                                                  ? Colors.white
                                                  : Colors.grey[400],
                                              fontSize: 10,
                                              fontWeight: FontWeight.bold,
                                            ),
                                          ),
                                        ),
                                      ),
                                      const SizedBox(width: 10),
                                      Expanded(
                                        child: Text(
                                          name,
                                          style: TextStyle(
                                            color: isSelected
                                                ? Colors.white
                                                : Colors.grey[300],
                                            fontWeight: isSelected
                                                ? FontWeight.bold
                                                : FontWeight.normal,
                                            fontSize: 13,
                                          ),
                                          maxLines: 1,
                                          overflow: TextOverflow.ellipsis,
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                              ),
                            );
                          },
                        ),
                ),
                const SizedBox(height: 8),
                const Divider(color: Colors.white12, height: 1),
                const SizedBox(height: 8),

                // Audio Section
                const Text(
                  "ÁUDIO",
                  style: TextStyle(
                    color: Colors.blueAccent,
                    fontSize: 10,
                    fontWeight: FontWeight.bold,
                    letterSpacing: 1.0,
                  ),
                ),
                const SizedBox(height: 6),
                Expanded(
                  child: _audioDevices.isEmpty
                      ? const Center(
                          child: Text(
                            "Nenhum dispositivo de áudio disponível",
                            style: TextStyle(color: Colors.grey, fontSize: 12),
                          ),
                        )
                      : ListView.builder(
                          itemCount: _audioDevices.length,
                          itemBuilder: (context, index) {
                            final device = _audioDevices[index];
                            final id = device['id']!;
                            final name = device['name']!;
                            final isSelected = audioManager.selectedDeviceId == id;

                            return Container(
                              margin: const EdgeInsets.symmetric(vertical: 1.0),
                              child: InkWell(
                                onTap: () {
                                  audioManager.startCapture(id, audioManager.isLoopbackEnabled).then((channels) {
                                    if (channels == 1) {
                                      widget.showToast("Áudio MONO! Ative Estéreo nas propriedades do Windows.");
                                    } else {
                                      widget.showToast("Áudio: $name");
                                    }
                                    setState(() {});
                                  });
                                },
                                borderRadius: BorderRadius.circular(8),
                                child: Container(
                                  padding: const EdgeInsets.symmetric(
                                      horizontal: 10, vertical: 6),
                                  decoration: BoxDecoration(
                                    color: isSelected
                                        ? Colors.teal.withOpacity(0.20)
                                        : Colors.white.withOpacity(0.08),
                                    borderRadius: BorderRadius.circular(8),
                                    border: Border.all(
                                      color: isSelected
                                          ? Colors.teal.withOpacity(0.60)
                                          : Colors.white.withOpacity(0.12),
                                    ),
                                  ),
                                  child: Row(
                                    children: [
                                      Icon(
                                        Icons.mic,
                                        color: isSelected ? Colors.teal : Colors.grey,
                                        size: 16,
                                      ),
                                      const SizedBox(width: 10),
                                      Expanded(
                                        child: Text(
                                          name,
                                          style: TextStyle(
                                            color: isSelected
                                                ? Colors.white
                                                : Colors.grey[300],
                                            fontWeight: isSelected
                                                ? FontWeight.bold
                                                : FontWeight.normal,
                                            fontSize: 13,
                                          ),
                                          maxLines: 1,
                                          overflow: TextOverflow.ellipsis,
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                              ),
                            );
                          },
                        ),
                ),
                const SizedBox(height: 8),

                // Audio Monitoring Toggle and VU Meter
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: Colors.white.withOpacity(0.02),
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(color: Colors.white.withOpacity(0.04)),
                  ),
                  child: Column(
                    children: [
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Row(
                            children: [
                              Icon(
                                audioManager.isLoopbackEnabled ? Icons.volume_up : Icons.volume_off,
                                color: audioManager.isLoopbackEnabled ? Colors.green : Colors.grey,
                                size: 18,
                              ),
                              const SizedBox(width: 8),
                              const Text(
                                "Monitorar Áudio (Alto-falantes)",
                                style: TextStyle(color: Colors.white, fontSize: 12),
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
                                        ? "Monitoramento de Áudio Ativado"
                                        : "Monitoramento de Áudio Desativado");
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
                                color: audioManager.isDeinterleaveEnabled ? Colors.tealAccent : Colors.grey,
                                size: 18,
                              ),
                              const SizedBox(width: 8),
                              const Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text(
                                    "Correção Estéreo (Placa HDMI)",
                                    style: TextStyle(color: Colors.white, fontSize: 12),
                                  ),
                                  Text(
                                    "Para placas de 96kHz mono (MS2109)",
                                    style: TextStyle(color: Colors.grey, fontSize: 9),
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
                                  ? "Correção Estéreo Ativada"
                                  : "Correção Estéreo Desativada");
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
                                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                                children: [
                                  const Text("Nível de Entrada",
                                      style: TextStyle(color: Colors.grey, fontSize: 10)),
                                  Text("${(vol * 100).toInt()}%",
                                      style: const TextStyle(color: Colors.grey, fontSize: 10)),
                                ],
                              ),
                              const SizedBox(height: 4),
                              ClipRRect(
                                borderRadius: BorderRadius.circular(2),
                                child: LinearProgressIndicator(
                                  value: vol,
                                  minHeight: 4,
                                  backgroundColor: Colors.white12,
                                  valueColor: AlwaysStoppedAnimation<Color>(
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
                      icon: const Icon(Icons.exit_to_app, color: Colors.redAccent, size: 16),
                      label: const Text("Fechar App", style: TextStyle(color: Colors.redAccent, fontSize: 12)),
                      onPressed: _confirmExitApp,
                    ),
                    ElevatedButton(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.blueAccent,
                        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                      ),
                      onPressed: widget.onClose,
                      child: const Text("Pronto", style: TextStyle(color: Colors.white, fontSize: 12)),
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
