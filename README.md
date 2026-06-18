# VDisPlay - Monitor de Captura & Preview Minimalista

O **VDisPlay** é um programa de visualização e monitoramento de vídeo e áudio em tempo real com baixíssima latência (<100ms) estilo "janela de preview do OBS", projetado com um visual elegante e tema escuro.

Ele é ideal para desenvolvedores, técnicos e gamers que utilizam placas de captura HDMI USB ou Webcams para depurar a saída de consoles de videogame, placas embarcadas (SBCs/SOCs como Raspberry Pi), saídas de câmeras reflex e outros dispositivos de streaming de forma simples, direta e leve.

---

## 🛠️ Arquitetura e Tecnologias

A aplicação é dividida em duas camadas de alta performance:
1. **Frontend (UI em Flutter/Dart)**: Uma interface minimalista que exibe o vídeo usando **Texturas Externas Nativas (Pixel Buffer)**, proporcionando rendering acelerado via GPU sem pressão sobre o Dart Garbage Collector (GC).
2. **Backend/Kernel (C Nativo)**: Captura de quadros em segundo plano usando **Media Foundation (Source Reader)** com conversão de cor automática para RGB32 (BGRA) no chip de vídeo, e processamento/loopback de áudio de latência mínima usando **WASAPI** com resampling linear integrado.

---

## ⌨️ Atalhos de Teclado (Controles)

Como o layout é 100% minimalista (apenas o vídeo ocupando a janela), todo o controle é feito via teclado:

| Tecla | Ação |
|---|---|
| **`M`** | Abre / fecha o menu de configurações (overlay semitransparente de seleção de dispositivos). |
| **`H`** | Abre / fecha o painel de **Ajuda** com créditos do app e resumo de todos os atalhos. |
| **`R`** | Abre / fecha o menu de **Resolução e FPS** para ajustar o formato de vídeo da captura. |
| **`1` a `9`** | Alterna rapidamente para os primeiros 9 canais de vídeo detectados pelo sistema. |
| **`S`** | Tira uma captura de tela (Screenshot) em PNG do frame atual e salva no diretório de execução. |
| **`T`** | Alterna o modo **Sempre no Topo** (Always on Top) da janela. |
| **`+`** / **`-`** | Ajusta o brilho da imagem (offset do buffer nativo, de -100% a +100%). |
| **`F12`** | Liga / desliga o monitoramento do áudio nos alto-falantes (Loopback). |
| **`F`** / **`F11`** | Alterna entre modo Janela e Tela Cheia (Fullscreen). |
| **`ESC`** | Fecha qualquer menu/overlay ativo (Dispositivos, Ajuda ou Resolução) ou sai do modo Tela Cheia. |

### 🎛️ Menu de Resolução e FPS (Tecla R)
O menu de Resolução e FPS permite ajustar o formato de vídeo dinamicamente durante a captura, sem salvar preferências (o app sempre inicia com a detecção automática do melhor formato nativo disponível):
- **Resolução**: Lista todas as resoluções únicas suportadas pela placa de captura, ordenadas da maior para a menor.
- **FPS**: Oferece três opções — **Nativo** (máximo suportado), **30 FPS** e **60 FPS**. Opções incompatíveis com a resolução ativa são exibidas como indisponíveis.

---

## 🚀 Como Compilar e Executar

### Pré-requisitos
* **Windows 10 ou 11** (arquitetura x64).
* **Flutter SDK** (com suporte a desktop habilitado).
* **Visual Studio 2022** (com a carga de trabalho de desenvolvimento para C++ Desktop habilitada) e **CMake** instalados.

### Compilação Rápida
Basta executar o script em lote na raiz do projeto:
```cmd
.\build.bat
```
O script irá:
1. Gerar e compilar a biblioteca nativa `native_library.dll` na pasta `native/build/Release`.
2. Executar o build do projeto Flutter Desktop em modo Release.
3. Copiar a DLL nativa compilada automaticamente para a pasta final do executável.

O executável final estará pronto em:
`build\windows\x64\runner\Release\vdisplay.exe`

### Desenvolvimento (`flutter run`)
O arquivo `windows/runner/CMakeLists.txt` foi configurado para compilar a `native_library` como parte do projeto do runner e copiar a DLL compilada automaticamente no pós-build. Sendo assim, para desenvolver basta rodar:
```cmd
flutter run
```

---

## 📁 Estrutura do Projeto

* `lib/`: Código-fonte em Dart (UI, atalhos, binds FFI e lógica de controle).
  * `main.dart`: Tela principal, layout, orquestração dos overlays.
  * `key_shortcuts.dart`: Mapeamento e lógica de todas as teclas de atalho.
  * `video_preview.dart`: Gerenciador de preview e resolução/FPS em tempo real.
  * `audio_manager.dart`: Gerenciador de captura e loopback de áudio.
  * `native_bindings.dart`: Bindings FFI para a biblioteca nativa em C.
  * `device_selector.dart`: Overlay do menu de dispositivos (Tecla M).
  * `help_overlay.dart`: Overlay de ajuda com créditos e atalhos (Tecla H).
  * `resolution_selector.dart`: Overlay de seleção de resolução e FPS (Tecla R).
  * `window_utils.dart`: Utilitários Win32 para fullscreen e always-on-top.
* `native/`: Código-fonte em C nativo (captura de vídeo e áudio via Media Foundation e WASAPI).
* `windows/runner/`: Código C++ do hospedeiro Windows que faz a ponte de renderização de Textura.
* `CMakeLists.txt`: Script CMake raiz da biblioteca nativa.
* `build.bat`: Script automatizador de build.
* `config.example.json`: Exemplo de arquivo de preferências locais criado pelo app.
