import 'dart:ui';
import 'package:flutter/material.dart';

class HelpOverlay extends StatelessWidget {
  final VoidCallback onClose;

  const HelpOverlay({
    Key? key,
    required this.onClose,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Center(
      child: ClipRRect(
        borderRadius: BorderRadius.circular(16),
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 15, sigmaY: 15),
          child: Container(
            width: 500,
            height: 580,
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
            padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Header
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    const Row(
                      children: [
                        Icon(Icons.help_outline, color: Colors.blueAccent, size: 22),
                        SizedBox(width: 8),
                        Text(
                          "Ajuda do VDisPlay",
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                            letterSpacing: 0.2,
                          ),
                        ),
                      ],
                    ),
                    IconButton(
                      icon: const Icon(Icons.close, color: Colors.white54),
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
                    border: Border.all(color: Colors.white.withOpacity(0.05)),
                  ),
                  child: const Column(
                    children: [
                      Text(
                        "VDisPlay V1.0",
                        style: TextStyle(
                          color: Colors.white,
                          fontSize: 15,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      SizedBox(height: 4),
                      Text(
                        "By Sandro Benigno (2026)",
                        style: TextStyle(
                          color: Colors.grey,
                          fontSize: 12,
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),

                // Shortcut list header
                const Text(
                  "TECLAS DE ATALHO",
                  style: TextStyle(
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
                      _buildShortcutRow("M", "Menu de Dispositivos", "Abre ou fecha o painel de seleção de vídeo e áudio."),
                      _buildShortcutRow("H", "Ajuda / Atalhos", "Exibe este painel de informações e atalhos."),
                      _buildShortcutRow("R", "Resolução e FPS", "Abre o menu para ajustar a resolução e o limite de FPS."),
                      _buildShortcutRow("F ou F11", "Alternar Tela Cheia", "Entra ou sai do modo tela cheia (borderless)."),
                      _buildShortcutRow("ESC", "Fechar / Voltar", "Fecha menus ativos ou sai do modo tela cheia."),
                      _buildShortcutRow("T", "Sempre no Topo", "Fixa ou desafixa a janela sobre todas as outras."),
                      _buildShortcutRow("S", "Captura de Tela", "Salva um screenshot PNG instantâneo da captura."),
                      _buildShortcutRow("+ / -", "Ajustar Brilho", "Aumenta ou diminui o brilho da imagem em tempo real."),
                      _buildShortcutRow("F12", "Monitorar Áudio", "Liga/desliga a saída de som da placa no sistema."),
                      _buildShortcutRow("1 a 9", "Trocar Vídeo", "Seleciona rapidamente o dispositivo de vídeo pelo índice."),
                    ],
                  ),
                ),
                const SizedBox(height: 12),
                
                // Footer hint
                Center(
                  child: Text(
                    "Pressione [ESC] ou clique fora para fechar",
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
  }

  Widget _buildShortcutRow(String keyText, String title, String description) {
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
                  fontFamily: "Courier",
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
