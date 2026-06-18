# VDisPlay 📺🔊

[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)](#)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](#)
[![Tech Stack](https://img.shields.io/badge/stack-Flutter%20%7C%20C%20%7C%20FFI-orange.svg)](#)

O **VDisPlay** é uma aplicação desktop de alta performance para Windows desenvolvida em **Flutter** com um kernel nativo em **C**. Seu principal propósito é servir como uma janela de monitoramento ultraleve e de baixíssima latência (<100ms) para placas de captura de vídeo/áudio HDMI (como consoles de videogame) e webcams, no estilo "preview" do OBS Studio, porém sem o peso ou a complexidade de uma suíte de gravação.

---

## 🚀 Como Funciona (Arquitetura)

O VDisPlay utiliza uma abordagem híbrida para maximizar o desempenho e minimizar a sobrecarga de CPU/GPU:

* **Captura de Vídeo (Media Foundation):** O backend nativo em C utiliza a API do Windows Media Foundation (`IMFSourceReader`) para obter quadros de vídeo em formato bruto na maior resolução e taxa de FPS disponíveis no hardware.
* **Exibição de Alta Performance (External Textures):** Para evitar cópias lentas de memória no Dart GC, os quadros RGBA gerados no kernel C são renderizados diretamente no pipeline do Flutter usando a API de texturas externas nativas do Windows (`PixelBuffer`).
* **Áudio de Baixa Latência (WASAPI):** A captura e monitoramento de áudio são feitos via WASAPI compartilhado. O fluxo de áudio é direcionado diretamente aos alto-falantes com tratamento de resampling linear integrado e bypass de melhorias de áudio do sistema (Modo RAW), garantindo som limpo e sem lag.
* **Correção de Áudio Estéreo (MS2109 Fix):** Inclui um algoritmo de desintercalação em tempo real para corrigir placas de captura baratas que o Windows detecta incorretamente como "mono de 96kHz". Quando ativado, o kernel C reconstrói os canais Esquerdo/Direito a 48kHz estéreo.

---

## ⌨️ Teclas de Atalho (Hotkeys)

Controle a aplicação rapidamente utilizando os seguintes atalhos globais de teclado:

| Tecla | Ação | Descrição |
| :---: | :--- | :--- |
| **`M`** | **Menu de Dispositivos** | Abre/fecha o painel flutuante de seleção de vídeo, áudio, monitoramento e correção estéreo. |
| **`H`** | **Ajuda / Sobre** | Abre o painel com créditos e resumo das teclas de atalho. |
| **`R`** | **Resolução e FPS** | Abre o menu flutuante para alterar a resolução de vídeo e limitar o FPS da captura. |
| **`F`** ou **`F11`** | **Tela Cheia** | Alterna entre o modo janela e tela cheia completa (borderless). |
| **`ESC`** | **Sair / Fechar** | Fecha menus ativos (Dispositivos, Ajuda ou Resolução) ou sai de tela cheia. |
| **`T`** | **Sempre no Topo** | Fixa a janela do VDisPlay sobre todas as outras janelas do sistema (Always on Top). |
| **`S`** | **Capturar Tela** | Salva um screenshot instantâneo em formato PNG de alta fidelidade na pasta do executável. |
| **`+`** | **Aumentar Brilho** | Aumenta o nível de compensação de brilho do buffer de imagem em tempo real. |
| **`-`** | **Diminuir Brilho** | Diminui o nível de compensação de brilho do buffer de imagem em tempo real. |
| **`1` a `9`** | **Trocar Vídeo** | Seleciona rapidamente o dispositivo de vídeo correspondente ao índice numérico (0 a 8). |

---

## 🛠️ Tecnologias e Compilação

O projeto é compilado combinando os builders do C++ e do Flutter:

* **Backend:** Código C puro compilado em `native_library.dll` utilizando CMake e MSVC.
* **Frontend:** Interface gráfica construída em Flutter, utilizando a biblioteca `ffi` para mapeamento de ponteiros de memória e chamadas nativas em C.
* **Persistência:** Uso de `shared_preferences` para lembrar suas escolhas de dispositivos, brilho e estados de áudio nas próximas sessões de uso.

### Script de Build
Para compilar a DLL nativa em modo Release e, em seguida, compilar o app Flutter completo, execute a partir do diretório raiz:
```powershell
.\build.bat
```
O executável final gerado estará localizado em:
`build\windows\x64\runner\Release\vdisplay.exe`

---

## ⚙️ Principais Características
* **Inicialização Segura:** O aplicativo não tenta reabrir a última câmera automaticamente se ela estiver com problemas físicos de driver, iniciando de forma segura em tela limpa.
* **Design Premium:** Menu overlay com efeito de desfoque de vidro (*BackdropFilter*) e tema dark esteticamente polido para dar destaque total ao conteúdo.
* **Matching Inteligente:** O aplicativo tenta parear o áudio de forma inteligente com base no nome do dispositivo de vídeo selecionado (ex: parear microfone da webcam ao abrir a webcam).
