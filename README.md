# Low-Cost Embedded Crack Detection System

> **Intelligent Concrete Crack Detection and Progressive Mapping System**

A multi-board edge-computing solution for real-time structural health monitoring of concrete surfaces. The system captures images, applies computer-vision algorithms on an Arduino UNO Q, relays results through a Flask web server, and progressively renders a colour-coded crack map on a VEGA RISC-V TFT display — while simultaneously driving hardware interrupts on an STM32 microcontroller.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [System Architecture Diagram](#system-architecture-diagram)
3. [Hardware Components](#hardware-components)
4. [Algorithm and Logic](#algorithm-and-logic)
5. [Repository Structure](#repository-structure)
6. [Software Requirements](#software-requirements)
7. [Installation and Deployment](#installation-and-deployment)
8. [Usage Guide](#usage-guide)
9. [Wiring and Pin Connections](#wiring-and-pin-connections)
10. [Troubleshooting](#troubleshooting)
11. [Contributing](#contributing)
12. [License](#license)

---

## Project Overview

Concrete cracking is one of the earliest indicators of structural degradation in buildings, bridges, and infrastructure. Manual inspection is time-consuming, subjective, and often unsafe. This project implements a **low-cost, embedded-first** alternative that runs entirely on commodity hardware:

| Capability | Detail |
|---|---|
| Image acquisition | USB webcam attached to Arduino UNO Q |
| On-device CV pipeline | FFT → Otsu threshold → Morphological closing |
| Crack visualization | Progressive 7-step reveal on 240 × 320 TFT |
| Secondary actuation | STM32 hardware interrupt for relay / buzzer / logger |
| Data persistence | Flask server stores JPEG frames on the host laptop |

---

## System Architecture Diagram

```
  ┌─────────────────────────────────────────────────────────────┐
  │                        HOST LAPTOP                          │
  │  ┌──────────────────┐        ┌──────────────────────────┐  │
  │  │  Flask Web Server│        │  Serial Driver (Python)  │  │
  │  │  (POST /upload)  │──────▶│  Reads saved frames &    │  │
  │  │  Saves JPEG      │        │  streams RGB565 over UART│  │
  │  └──────────────────┘        └──────────┬───────────────┘  │
  └──────────────────────────────────────────┼──────────────────┘
          ▲  HTTP POST (Wi-Fi)               │ USB-to-UART (500 kbaud)
          │                                  ▼
  ┌───────┴──────────┐              ┌─────────────────────┐
  │  Arduino UNO Q   │              │  VEGA RISC-V Board  │
  │  (Edge Processor)│   GPIO INT   │  (Visualization)    │
  │  USB Webcam      │─────────────▶│  240×320 TFT        │
  │  CV Pipeline     │              │  R/K Handshake      │
  │  Wi-Fi HTTP POST │              └─────────────────────┘
  └───────┬──────────┘
          │ Digital OUT (interrupt line)
          ▼
  ┌─────────────────────┐
  │  STM32 MCU          │
  │  (Peripheral Ctrl)  │
  │  Relay / Buzzer /   │
  │  External Logger    │
  └─────────────────────┘
```

---

## Hardware Components

### 1. Arduino UNO Q — The Edge Processor

| Feature | Details |
|---|---|
| **Role** | Primary control unit and vision processor |
| **Connectivity** | Wi-Fi (HTTP POST to Flask server), UART, GPIO |
| **Vision input** | USB webcam (MJPEG stream) |
| **Key tasks** | Debounced hardware interrupt handling, frame capture, CV pipeline execution, crack-coordinate serialisation |

### 2. VEGA RISC-V Board — The Visualisation Engine

| Feature | Details |
|---|---|
| **Role** | Dedicated TFT display driver |
| **Display** | 240 × 320 SPI TFT (ILI9341 or compatible) |
| **Serial speed** | 500,000 baud (USB-to-UART) |
| **Protocol** | Custom Ready / Acknowledge (R/K) frame-sync handshake |
| **Key tasks** | Buffer incoming RGB565 pixel data, render progressive crack overlay |

### 3. STM32 Microcontroller — The Peripheral Controller

| Feature | Details |
|---|---|
| **Role** | Secondary hardware actuation |
| **Trigger source** | Digital interrupt line from Arduino UNO Q |
| **Key tasks** | Mechanical stop sequences, buzzer alert, external data logging |
| **Firmware** | STM32 HAL (see `main.c`, `stm32f4xx_hal_msp.c`) |

### 4. Host Laptop — The Centralised Hub

| Feature | Details |
|---|---|
| **Role** | Data persistence and protocol translation |
| **Flask server** | Receives JPEG frames via HTTP POST, saves to `./saved_frames/` |
| **Serial driver** | Monitors saved frames and streams latest map to VEGA |

---

## Algorithm and Logic

### Detection Pipeline

```
Raw Frame (USB Webcam)
        │
        ▼
1. Greyscale Conversion
        │
        ▼
2. FFT High-Pass Filter  ──▶  Removes low-frequency surface texture
        │                       (lighting gradients, colour variation)
        ▼
3. Otsu's Thresholding   ──▶  Dynamic binary segmentation adapted to
        │                       concrete colour and ambient lighting
        ▼
4. Morphological Closing ──▶  Merges fragmented pixels into discrete,
        │                       measurable crack regions
        ▼
5. Connected-Component Labelling
        │
        ▼
6. Spatial Sorting       ──▶  Bottom-to-Top, Left-to-Right priority
        │
        ▼
7. Bounding Boxes + Centroids  ──▶  Stored in crack region list
```

### State-Machine Mapping Sequence (7-Click Cycle)

| Click | Action |
|---|---|
| **1** | Capture new base frame → transmit raw image to TFT (Baseline) |
| **2 – 6** | Progressively reveal one red bounding-box marker per crack region |
| **7** | Reset state machine, clear frame buffer, begin fresh capture |

This progressive reveal gives the inspector a step-by-step view of each individual crack rather than a single cluttered overlay.

---

## Repository Structure

```
Low-Cost-Embedded-Crack-Detection-System/
│
├── Arduino UNO Q Crack Detection/
│   └── embed-proj/                  # Arduino sketch (.ino)
│       ├── embed-proj.ino           # Main application (button ISR, webcam, CV, Wi-Fi POST)
│       └── ...
│
├── Crack Detection Software Implementation/
│   └── Embed Proj Software Implementation/
│       ├── crack_detection.py       # Standalone PC test of the CV pipeline
│       └── ...
│
├── laptop_web server/               # Run on host laptop
│   ├── laptop_server.py             # Flask server — receives & saves frames
│   └── serial_driver.py            # Reads saved frames, streams RGB565 to VEGA
│
├── part_wise_codes_test_each block_seperataely/
│   │                                # Unit-test sketches for individual subsystems
│   ├── test_fft.py
│   ├── test_otsu.py
│   ├── test_serial_vega.ino
│   └── ...
│
├── vega_tft_web_camera_display_properly_working/
│   │                                # VEGA RISC-V firmware
│   ├── vega_main.c                  # Main display loop with R/K handshake
│   └── ...
│
├── main.c                           # STM32 HAL main application
├── stm32f4xx_hal_msp.c              # STM32 HAL MSP initialisation
├── stm32f4xx_it.c                   # STM32 interrupt handlers
├── syscalls.c                       # Newlib syscall stubs
├── sysmem.c                         # Heap memory configuration
├── system_stm32f4xx.c               # System clock configuration
└── README.md
```

---

## Software Requirements

### Python (Host Laptop)

| Package | Purpose |
|---|---|
| `Python 3.12+` | Runtime |
| `opencv-python` | Frame capture, FFT, Otsu, morphology |
| `numpy` | Numerical operations |
| `flask` | HTTP server for frame ingestion |
| `pyserial` | UART streaming to VEGA |
| `requests` | HTTP POST from Arduino (tested via PC first) |

Install all Python dependencies in one step:

```bash
pip install opencv-python numpy flask pyserial requests
```

### Embedded Toolchains

| Target | IDE / Toolchain |
|---|---|
| Arduino UNO Q | Arduino IDE 2.x or PlatformIO |
| VEGA RISC-V | VEGA SDK / RISC-V GCC |
| STM32 | STM32CubeIDE or arm-none-eabi-gcc + OpenOCD |

---

## Installation and Deployment

### Step 1 — Clone the Repository

```bash
git clone https://github.com/daddydembla007/Low-Cost-Embedded-Crack-Detection-System.git
cd Low-Cost-Embedded-Crack-Detection-System
```

### Step 2 — Start the Flask Server on the Host Laptop

```bash
cd "laptop_web server"
python laptop_server.py
```

The server starts on `http://0.0.0.0:5000` by default and saves incoming JPEG frames to `./saved_frames/`.

### Step 3 — Start the Serial Driver on the Host Laptop

Open a second terminal:

```bash
cd "laptop_web server"
python serial_driver.py --port COM3 --baud 500000
# On Linux/macOS:
# python serial_driver.py --port /dev/ttyUSB0 --baud 500000
```

The driver monitors `./saved_frames/`, converts the latest frame to RGB565, and streams it to the VEGA board via UART.

### Step 4 — Flash the VEGA RISC-V Board

1. Open `vega_tft_web_camera_display_properly_working/` in your VEGA SDK workspace.
2. Build and flash the firmware to the board.
3. Connect the TFT display (see [Wiring](#wiring-and-pin-connections)).
4. Connect the VEGA board to the laptop via USB-to-UART adapter.

### Step 5 — Flash the STM32 Microcontroller

1. Open STM32CubeIDE and import the project from the repository root (contains `main.c`, `stm32f4xx_hal_msp.c`, etc.).
2. Build and flash via ST-LINK.
3. Connect the interrupt line from the Arduino UNO Q (see [Wiring](#wiring-and-pin-connections)).

### Step 6 — Upload the Arduino Sketch

1. Open `Arduino UNO Q Crack Detection/embed-proj/embed-proj.ino` in Arduino IDE 2.x.
2. Edit the configuration section at the top of the sketch:
   ```cpp
   const char* WIFI_SSID     = "YOUR_SSID";
   const char* WIFI_PASSWORD = "YOUR_PASSWORD";
   const char* SERVER_IP     = "192.168.x.x";   // Host laptop IP
   const int   SERVER_PORT   = 5000;
   ```
3. Select **Arduino UNO Q** as the target board, then **Upload**.

---

## Usage Guide

1. **Power on** all boards and ensure the host laptop server is running.
2. **Connect the USB webcam** to the Arduino UNO Q.
3. **Point the webcam** at the concrete surface under inspection.
4. **Press the hardware button** (connected to the UNO Q interrupt pin) to begin the 7-click cycle:

   | Click | Display |
   |---|---|
   | 1st press | Raw baseline image appears on TFT |
   | 2nd – 6th press | Each press reveals the next detected crack region (red bounding box) |
   | 7th press | Screen clears; system ready for a new inspection |

5. The **STM32** fires its interrupt action (buzzer / relay) whenever a new crack detection result is transmitted by the UNO Q.
6. Captured JPEG frames are automatically saved in `laptop_web server/saved_frames/` for audit trails.

---

## Wiring and Pin Connections

### Arduino UNO Q → STM32

| Arduino UNO Q Pin | STM32 Pin | Signal |
|---|---|---|
| D7 (Digital OUT) | PA0 (EXTI) | Crack-detected interrupt line |
| GND | GND | Common ground |

### VEGA RISC-V Board → TFT Display (SPI)

| VEGA Pin | TFT Pin | Signal |
|---|---|---|
| SPI MOSI | SDA / MOSI | Data |
| SPI CLK | SCL / CLK | Clock |
| GPIO CS | CS | Chip select |
| GPIO DC | DC / RS | Data / Command |
| GPIO RST | RESET | Hardware reset |
| 3.3 V | VCC | Power |
| GND | GND | Ground |

### USB Webcam → Arduino UNO Q

Connect via the UNO Q's USB-A host port (no additional wiring required).

### Host Laptop → VEGA RISC-V Board

Connect via USB-to-UART adapter at **500,000 baud, 8N1**.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| Flask server not receiving frames | Arduino not connected to Wi-Fi | Verify SSID/password in sketch; check `Serial Monitor` for IP assignment |
| TFT shows garbled image | Baud rate mismatch | Confirm both serial driver and VEGA firmware use 500,000 baud |
| TFT shows nothing | R/K handshake stalled | Reset VEGA board; check UART TX/RX crossover wiring |
| STM32 does not trigger | Interrupt line floating | Add 10 kΩ pull-down resistor on the STM32 EXTI pin |
| No cracks detected on clear surface | Threshold too aggressive | Lower the morphological kernel size or adjust Otsu pre-processing |
| `pyserial` port not found | Wrong COM/tty path | Run `python -m serial.tools.list_ports` to identify the correct port |

---

## Contributing

Contributions, bug reports, and feature requests are welcome!

1. **Fork** the repository.
2. Create a feature branch: `git checkout -b feature/your-feature-name`
3. Commit your changes: `git commit -m "feat: add your feature description"`
4. Push to the branch: `git push origin feature/your-feature-name`
5. Open a **Pull Request** against `main`.

Please follow the existing code style and include comments for any new embedded firmware.

---

## License

This project is released under the [MIT License](https://opensource.org/licenses/MIT).

```
MIT License

Copyright (c) 2025 daddydembla007

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
