# Android Mirror (`android-mirror`)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)](https://kernel.org)
[![Android](https://img.shields.io/badge/Android-5.0%20--%2016%2B-brightgreen.svg)](https://source.android.com)

A high-performance, ultra-low-latency Android screen mirroring and remote control client engineered in **C++20 (SDL2 + FFmpeg)** and **Java (AOSP reflection)**.

`android-mirror` provides standalone self-orchestration without needing external bash wrappers. It automatically resolves connected devices via ADB, deploys the isolated server daemon, provisions bi-directional socket tunnels, streams hardware-accelerated H.264 video, and handles mouse and keyboard input with sub-frame response latency.

---

## Features

- **Ultra-Low Latency Streaming**: Hardware-accelerated H.264 encoding via Android `MediaCodec` with non-blocking baseline video demuxing and decoding via FFmpeg `libavcodec`.
- **Standalone Self-Orchestration**: Direct ADB device resolution, server staging, reverse/forward socket configuration, and dual-socket binary handshake directly from the C++ binary.
- **Universal Android Compatibility**: Seamless operation across **Android 5.0 (API 21) through Android 16+ (API 36+)**:
  - Android 14–16+ static `DisplayManager.createVirtualDisplay` integration.
  - Android 10–13 `SurfaceControl.Transaction` support.
  - Android 5–9 legacy `SurfaceControl` transaction fallback.
  - Boot-time `VMRuntime.setHiddenApiExemptions` to bypass OEM reflection blocks (Xiaomi HyperOS / MIUI, Samsung OneUI, Oppo ColorOS).
- **Responsive Input Mapping**:
  - Touch simulation with 8px anti-jitter touch-slop filtering.
  - Mouse wheel scrolling (vertical & horizontal).
  - Keyboard injection mapped directly to Android keycodes.
  - Desktop-to-device clipboard synchronization (`Ctrl+V` / `Cmd+V`).
- **Guaranteed Clean Teardown**: Automatic session recovery and teardown via POSIX signal handlers (`SIGINT`, `SIGTERM`) and `std::atexit`. Dangling tunnels and remote background daemons are terminated cleanly.
- **Production Packaging**: Native CMake system installation (`GNUInstallDirs`), FreeDesktop `.desktop` launcher integration, and standalone Debian (`.deb`) package generation via CPack.

---

## Architecture Overview

```
┌─────────────────────────────────┐                 ┌─────────────────────────────────┐
│     Desktop Client (C++20)      │                 │     Android Server (Daemon)     │
│                                 │                 │                                 │
│  ┌───────────────────────────┐  │                 │  ┌───────────────────────────┐  │
│  │   AndroidAdbSession       │  │── ADB Tunnel ──►│  │   app_process Daemon      │  │
│  └─────────────┬─────────────┘  │   (TCP 27183)   │  └─────────────┬─────────────┘  │
│                │                │                 │                │                │
│  ┌─────────────▼─────────────┐  │  Video Channel  │  ┌─────────────▼─────────────┐  │
│  │ Video Pipeline            │◄─┼─────────────────┼──│ MediaCodec (H.264 / AVC)  │  │
│  │  - Demuxer                │  │  (80-byte Hdr)  │  │ DisplayManager / Surface  │  │
│  │  - FFmpeg Decoder         │  │                 │  └───────────────────────────┘  │
│  │  - SDL2 Texture Renderer  │  │                 │                                 │
│  └───────────────────────────┘  │                 │                                 │
│  ┌───────────────────────────┐  │ Control Channel │  ┌───────────────────────────┐  │
│  │ Input Mapper              │──┼─────────────────┼─►│ InputManager Injection    │  │
│  │  - Touch / Mouse Drag     │  │  (Binary Pkts)  │  │  - Touch & Motion Events  │  │
│  │  - Keycodes & Clipboard   │  │                 │  │  - KeyEvents & Clipboard  │  │
│  └───────────────────────────┘  │                 │  └───────────────────────────┘  │
└─────────────────────────────────┘                 └─────────────────────────────────┘
```

---

## Prerequisites

### 1. Host Dependencies (Linux)

Install the required C++20 build tools, SDL2, FFmpeg, and ADB:

#### Debian / Ubuntu / Mint:
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libsdl2-dev \
    libavcodec-dev \
    libavutil-dev \
    libavformat-dev \
    adb
```

#### Arch Linux / Manjaro:
```bash
sudo pacman -S --needed base-devel cmake pkgconf sdl2 ffmpeg android-tools
```

#### Fedora / RHEL:
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    pkgconf-pkg-config \
    SDL2-devel \
    ffmpeg-free-devel \
    android-tools
```

### 2. Android Device Setup

1. Open **Settings** on your phone.
2. Go to **About Phone** and tap **Build Number** 7 times to enable **Developer Options**.
3. Go to **Settings > System / Developer Options** and enable **USB Debugging**.
4. *(Xiaomi / HyperOS / MIUI devices only)*: Enable both **USB Debugging** and **USB Debugging (Security settings)** to allow touch and key injection.
5. Connect your device via USB cable and authorize the prompt: *"Always allow from this computer"*.
6. Verify your phone is recognized:
   ```bash
   adb devices
   ```

---

## Installation & Quick Start

### Option A: Direct Standalone Build & Run (No Root Required)

Clone the repository and build the desktop client:

```bash
git clone https://github.com/Microsoftened-Nair/android-mirror.git
cd android-mirror

# Configure and compile with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run client immediately (auto-detects connected phone)
./build/android-mirror
```

*(Convenience symlinks `./build/desktop-client` and `./build/MirrorDesktop` are also generated).*

---

### Option B: Install via Debian Package (`.deb`)

Generate and install the pre-packaged `.deb` package:

```bash
# Generate the .deb package
cpack --config build/CPackConfig.cmake -B build/packages

# Install package system-wide
sudo dpkg -i build/packages/android-mirror-1.0.0-Linux.deb

# Launch anytime from terminal or your app launcher:
android-mirror
```

---

### Option C: System-Wide Installation via CMake

```bash
sudo cmake --install build

# Run directly from any terminal
android-mirror
```

---

## Usage & Controls

### Command-Line Arguments

```text
Usage: android-mirror [options]

Options:
  -s, --serial <serial>       Target Android device serial (required if multiple devices connected)
  -p, --port <port>           Local TCP port for tunnel (default: 27183)
  --max-size <dimension>      Maximum streaming resolution bound (default: 1920)
  --bit-rate <bps>            Target H.264 video bitrate (default: 8000000 = 8 Mbps)
  --stay-awake <true|false>   Keep Android device awake while connected (default: true)
  --title <title>             Window title override (default: device model name)
  --server-path <path>        Custom path to server.jar artifact override
  --socket-name <name>        Abstract Unix domain socket name override (default: mirror)
  -h, --help                  Show help message
```

#### Examples:
```bash
# Connect to a specific phone when multiple devices are plugged in
android-mirror --serial RZCW61VM46H

# Limit resolution to 1080p and lower bitrate for wireless/USB 2.0 connections
android-mirror --max-size 1080 --bit-rate 4000000

# Mirror with custom title and keep phone screen awake
android-mirror --title "My Work Phone" --stay-awake true
```

---

### Mouse & Keyboard Shortcuts

| Input / Shortcut | Android Action |
| :--- | :--- |
| **Left Click + Drag** | Touch, tap, and drag gesture (with anti-jitter deadzone) |
| **Right Click** | **BACK** button (`AKEYCODE_BACK`) |
| **Middle Click** | **HOME** button (`AKEYCODE_HOME`) |
| **Mouse Wheel** | Vertical & horizontal scroll |
| **`Escape`** | **BACK** button (`AKEYCODE_BACK`) |
| **`Ctrl + P` / `Cmd + P`** | **POWER** button (toggle screen on/off) |
| **`Ctrl + V` / `Cmd + V`** | Paste desktop clipboard text into active Android input field |
| **Standard Typing** | Standard keyboard input mapped directly to Android key events |

---

## Building the Android Server (`server.jar`)

The repository includes pre-built server bytecode at `build/server.jar`. If you modify Java sources in `android-server/`:

### Prerequisites for Server Build:
- JDK 11 or higher
- Android SDK (`platforms/android-34/android.jar` and `build-tools/34.0.0/d8`)
- Ensure `ANDROID_HOME` is set:
  ```bash
  export ANDROID_HOME="$HOME/android-sdk"
  ```

### Build Command:
```bash
./scripts/build_server.sh
```
This compiles the Java 11 sources with `javac` and packages them into dexed bytecode via `d8`.

---

## Running Automated Tests

The client includes an extensive CTest verification suite covering socket handshakes, protocol serialization, frame queues, video demuxing, decoding, and input coordinate transforms:

```bash
ctest --test-dir build --output-on-failure
```

```text
      Start  1: handshake_layout
 1/11 Test  #1: handshake_layout .................   Passed    0.00 sec
      Start  2: adb_tunnel
 2/11 Test  #2: adb_tunnel .......................   Passed    0.00 sec
      Start  3: socket_handshake
 3/11 Test  #3: socket_handshake .................   Passed    0.06 sec
      Start  4: video_demuxer
 4/11 Test  #4: video_demuxer ....................   Passed    0.00 sec
      Start  5: control_packet
 5/11 Test  #5: control_packet ...................   Passed    0.00 sec
      Start  6: atomic_frame_queue
 6/11 Test  #6: atomic_frame_queue ...............   Passed    0.01 sec
      Start  7: video_decoder
 7/11 Test  #7: video_decoder ....................   Passed    0.01 sec
      Start  8: video_pipeline
 8/11 Test  #8: video_pipeline ...................   Passed    0.02 sec
      Start  9: video_renderer
 9/11 Test  #9: video_renderer ...................   Passed    0.17 sec
      Start 10: input_mapper
10/11 Test #10: input_mapper .....................   Passed    0.00 sec
      Start 11: client_app
11/11 Test #11: client_app .......................   Passed    0.13 sec

100% tests passed, 0 tests failed out of 11
```

---

## Uninstalling

### If Installed via Debian Package:
```bash
sudo apt remove android-mirror
# or
sudo dpkg -r android-mirror
```

### If Installed via `cmake --install`:
```bash
sudo xargs rm -f < build/install_manifest.txt
```

### Cleaning Local Build Files:
```bash
rm -rf build bin/classes bin/dist
```

---

## Troubleshooting

1. **`No authorized Android devices detected via adb`**:
   - Run `adb devices` in terminal.
   - If device shows `unauthorized`, unlock your phone and tap **Allow USB Debugging**.
   - If no device is listed, check your USB cable or toggle Developer Options off and on.

2. **Keys or clicks do not register on Xiaomi / HyperOS / MIUI**:
   - Go to **Developer Options** and enable **USB debugging (Security settings)**. This allows Android to accept touch and keyboard events simulated via ADB.

3. **Port collision (`Address already in use`)**:
   - Run `android-mirror --port 28000` to allocate an alternate TCP tunnel port.

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
