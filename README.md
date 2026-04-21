# Low-Cost-Embedded-Crack-Detection-System

 Intelligent Concrete Crack Detection and Progressive Mapping System

## Project Overview
This project implements an edge-computing solution for structural health monitoring. It utilizes the Arduino UNO Q for real-time image acquisition and Fast Fourier Transform (FFT) analysis to detect discrete crack regions in concrete surfaces. The system features a multi-tier communication architecture that relays processed data through a centralized web server to a VEGA RISC-V development board for high-speed TFT visualization, while simultaneously triggering hardware interrupts on an STM32 microcontroller.

## Hardware Architecture

### 1. Arduino UNO Q (The Edge Processor)
* **Role:** Acts as the primary control unit and vision processor.
* **Tasks:** Handles debounced hardware interrupts from physical triggers, captures high-definition frames via USB webcam, and executes the crack detection algorithm.
* **Communication:** Relays processed image data over Wi-Fi via HTTP POST requests and sends hardware synchronization pulses to the STM32.

### 2. VEGA RISC-V Board (The Visualization Engine)
* **Role:** Dedicated display driver for the system.
* **Tasks:** Receives optimized RGB565 image buffers over a high-speed serial interface (500,000 baud) and renders the progressive crack map on a 240x320 TFT display.
* **Handshaking:** Utilizes a custom Ready/Acknowledge (R/K) protocol to ensure frame synchronization and prevent data corruption.

### 3. STM32 Microcontroller (The Peripheral Controller)
* **Role:** Executes secondary hardware actions based on detection results.
* **Tasks:** Monitors a dedicated interrupt line from the UNO Q. Upon detection, it performs real-time tasks such as mechanical stop sequences or external logging.

### 4. Centralized Hub (Laptop Server)
* **Role:** Middleman for data persistence and protocol translation.
* **Components:** * **Flask Web Server:** Receives and logs JPEG-encoded detection frames to local storage.
    * **Serial Driver:** Monitors the local repository and streams the latest mapping data to the VEGA board via USB-to-UART.

## Algorithm and Logic

### Detection Pipeline
1.  **FFT High-Pass Filtering:** Removes low-frequency surface textures (shading, lighting gradients) to isolate high-frequency anomalies (cracks).
2.  **Otsu’s Thresholding:** Dynamically calculates optimal binary contrast levels to adapt to varying concrete colors and lighting conditions.
3.  **Morphological Processing:** Employs Closing operations to merge fragmented pixels into discrete, quantifiable regions.
4.  **Spatial Sorting:** Sorts detected regions by coordinate priority: Bottom-to-Top and Left-to-Right.

### State-Machine Mapping Sequence
The system operates on a 7-click progressive reveal cycle to improve diagnostic clarity:
* **Click 1:** Captures a new base frame and transmits the raw image to the display (Baseline).
* **Clicks 2 - 6:** Progressively reveals red markers for each discrete crack found during the initial scan.
* **Click 7:** Resets the internal state, clears memory, and initiates a fresh capture sequence.

## Software Requirements
* **Python 3.12+:** Required for the Laptop Server and Serial Driver.
* **Libraries:** `opencv-python`, `numpy`, `flask`, `pyserial`, `requests`.
* **C++ (Arduino/STM32):** For low-level GPIO management and RPC bridge communications.

## Installation and Deployment

### 1. Server Setup
Initialize the Flask server on the host laptop:
```bash
python laptop_server.py
