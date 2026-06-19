# VDisPlay 📺🔊

🇺🇸 English | 🇧🇷 [Português](README.pt-BR.md)

[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue.svg)](#)
[![Latency](https://img.shields.io/badge/latency-sub--100ms-success.svg)](#)
[![Backend](https://img.shields.io/badge/backend-C-critical.svg)](#)
[![Frontend](https://img.shields.io/badge/frontend-Flutter-54C5F8.svg)](#)
[![License: GPL v3](https://img.shields.io/badge/license-GNU%20GPL%20v3-blue.svg)](LICENSE)

**VDisPlay** is a lightweight real-time video and audio monitor with **sub-100ms latency**, designed for developers, technicians, and gamers.

Built for HDMI USB capture cards and webcams, it provides a fast and direct way to inspect video output from game consoles, SBCs, embedded systems, DSLR cameras, and other streaming devices — without the complexity of full recording suites like OBS.

![Gameplay screen](imgs/vdisplay_game.jpg)
---

## 🙋 Why VDisPlay?

Most capture software is designed for recording or streaming.

VDisPlay was built for a different purpose:

* **Debugging video output**
* **Monitoring hardware behavior in real time**
* **Testing embedded systems**
* **Inspecting console boot sequences**
* **Low-overhead HDMI preview**

Its focus is speed, simplicity, and minimal system overhead.

---

## ⚡ Core Features

* **Sub-100ms end-to-end latency**
* **Minimal fullscreen preview interface**
* **Real-time audio monitoring**
* **Dynamic resolution/FPS switching**
* **Native GPU-accelerated rendering**
* **Always-on-top mode**
* **Instant screenshots**
* **Stereo reconstruction fix for low-cost MS2109 capture cards**
* **Brightness adjustment in real time**
* **Safe startup mode for unstable capture devices**
* **Bilingual interface (English / Português)**

---

## 🖱️ Typical Use Cases

VDisPlay is especially useful for:

* Debugging HDMI output from FPGA and embedded systems
* Monitoring SBC boot output (Raspberry Pi, Orange Pi, etc.)
* Inspecting retro console output
* Previewing DSLR or HDMI camera feeds
* Testing capture devices
* Monitoring external devices without streaming overhead

---

## 🛠 Architecture

VDisPlay uses a hybrid architecture optimized for low latency.

### 🎨 Frontend (Flutter)

The UI layer is built in Flutter and uses native external textures to render video directly on the GPU.

This avoids expensive memory copies and reduces pressure on Dart's garbage collector.

### ⚙️ Native Backend (C)

The native kernel handles all critical I/O:

#### 🖥️ Video — Media Foundation

* Direct capture through `IMFSourceReader`
* Native frame negotiation
* Hardware-side color format negotiation (BGRA→RGBA, optimized 32-bit swap)
* Thread-safe frame delivery via critical sections

#### 🔊 Audio — WASAPI

* Low-latency shared-mode monitoring (MTA threading)
* RAW mode support (bypasses Windows DSP)
* Branchless VU metering via function pointer dispatch
* Float-precision linear resampling

#### 🔧 MS2109 Stereo Fix

Some low-cost capture devices expose stereo audio incorrectly as:

`96kHz mono`

VDisPlay can reconstruct:

`48kHz stereo (L/R)`

in real time.

---

## ⌨️ Keyboard Shortcuts

| Key         | Action                        |
| ----------- | ----------------------------- |
| `M`         | Device menu                   |
| `H`         | Help / About                  |
| `R`         | Resolution / FPS menu         |
| `F` / `F11` | Toggle fullscreen             |
| `ESC`       | Close menus / exit fullscreen |
| `T`         | Toggle always-on-top          |
| `S`         | Capture screenshot            |
| `+`         | Increase brightness           |
| `-`         | Decrease brightness           |
| `F12`       | Toggle audio monitoring       |
| `1–9`       | Quick switch video devices    |

---

## 🚀 Build

### 📋 Requirements

* Windows 10/11 (x64)
* Flutter SDK (desktop enabled)
* Visual Studio 2022
* CMake

### Quick Build

```powershell
.\build.bat
```

Final executable:

```text
build\windows\x64\runner\Release\vdisplay.exe
```

### Development

```powershell
flutter run
```

---

## Project Structure

```text
lib/                    → UI, overlays, FFI bindings, i18n
native/
  include/common.h      → shared utilities, GUIDs, device helpers
  video_capture.c       → Media Foundation capture kernel
  audio_capture.c       → WASAPI capture + loopback kernel
  device_manager.c      → device enumeration + COM init
windows/runner/         → native texture bridge
build.bat
CMakeLists.txt
```

---

## Philosophy

VDisPlay follows a simple principle:

**A monitoring tool should stay out of the way.**

No timelines.
No scenes.
No recording pipeline.
No streaming abstractions.

Just your signal — fast, direct, and visible.
