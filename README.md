<div align="center">

# 🏃 CorridorRunner

### 3D Corridor Runner for ESP32

*An embedded 3D endless runner game running entirely on an ESP32 with an ST7735 TFT display — zero external assets.*

<br>

![Platform](https://img.shields.io/badge/Platform-ESP32-00599C?logo=espressif&logoColor=white)
![Framework](https://img.shields.io/badge/Framework-Arduino-00979D?logo=arduino&logoColor=white)
![Language](https://img.shields.io/badge/Language-C++-00599C?logo=cplusplus&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green)

</div>

---

## Overview

CorridorRunner is a **3D perspective corridor runner** game ported from the [R4BB1T FHC](https://github.com) screensaver project. Every frame — the scrolling corridor, the running character, the obstacles — is **rendered mathematically in real time** with no sprite sheets or image assets. The entire game fits in a single 1000-line C++ source file.

### Features

- **Real-time 3D corridor rendering** with perspective projection and trigonometric curves
- **Animated stick-figure runner** drawn procedurally (lines, circles, rectangles)
- **Obstacle dodging** across 3 lanes (left, center, right) with pillar obstacles
- **3 difficulty levels**: Easy, Normal, Hard
- **Progressive difficulty** — speed increases as you survive longer
- **High score tracking** persisted across sessions
- **Pixel-art menu system** with "R4BB1T" logo and game over screen
- **Serial screen mirror** — stream your gameplay to a PC in real time via Python

---

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32 Dev Module (`esp32dev`) |
| Display | ST7735 TFT, 128×160 px, SPI |
| Input | 3 tactile buttons (Left, Right, Select) |
| Connectivity | USB Serial (CH340) for screen mirroring |

### Pin Mapping

| Function | GPIO |
|----------|------|
| Button Left | 14 |
| Button Right | 27 |
| Button Select | 26 |
| TFT MOSI | 23 |
| TFT SCLK | 18 |
| TFT CS | 5 |
| TFT DC | 17 |
| TFT RST | 16 |
| TFT Backlight | 21 |

---

## Getting Started

### Prerequisites

- [PlatformIO CLI](https://platformio.org/install/cli) or the PlatformIO VS Code extension
- Python 3 + `pyserial` + `Pillow` (for the screen mirror)

### Build & Flash

```bash
# Clone the repository
git clone https://github.com/your-username/CorridorRunner.git
cd CorridorRunner

# Build the firmware
pio run

# Flash to ESP32
pio run --target upload

# Open serial monitor
pio device monitor
```

### Screen Mirror (Optional)

```bash
# Connect a second terminal while the game is running
python mirror.py
```

This launches a Tkinter window that displays the game framebuffer in real time over USB serial at 2 Mbps. Edit `SERIAL_PORT` and `BAUD_RATE` at the top of `mirror.py` to match your setup.

---

## Controls

| Button | Game | Menu |
|--------|------|------|
| **Left** (GPIO 14) | Move left | Navigate left |
| **Right** (GPIO 27) | Move right | Navigate right |
| **Select** (GPIO 26) | Jump | Confirm / Start |

---

## Project Structure

```
CorridorRunner/
├── platformio.ini          # PlatformIO build configuration
├── mirror.py               # Python screen-mirror viewer
├── src/
│   ├── main.cpp            # Game logic, rendering, menus (~1021 lines)
│   └── Config.h            # Hardware pins, screen dims, color palette
└── .vscode/                # VS Code / PlatformIO IDE config
```

### Architecture

- **Single-file game engine** — all rendering, physics, collision, and UI live in `src/main.cpp`
- **Dual-core FreeRTOS** — Core 1 runs the game loop; Core 0 handles serial frame streaming
- **TFT_eSPI** library for hardware-accelerated SPI display output
- **No external assets** — every visual element is computed from math at 60 fps

---

## Configuration

Edit `src/Config.h` to customize:

| Constant | Default | Description |
|----------|---------|-------------|
| `SCR_W` / `SCR_H` | 128 / 160 | Display resolution |
| `DEBOUNCE_MS` | 180 | Button debounce delay (ms) |
| `C_*` | RGB565 | Color palette constants |

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---

<div align="center">

*Built with ❤️ for the ESP32 community*

</div>
