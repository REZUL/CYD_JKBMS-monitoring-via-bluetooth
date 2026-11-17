# CYD JK-BMS Monitoring via Bluetooth

An autonomous battery monitoring system based on ESP32 and CYD (Cheap Yellow Display), designed to retrieve battery data from JK-BMS via Bluetooth Low Energy (BLE) and display real-time information on a TFT screen.

## 1. Project Overview

CYD JK-BMS Monitoring is an embedded monitoring system that communicates directly with a JK-BMS using Bluetooth Low Energy.

The main goal of this project is to provide a compact, standalone battery monitoring device without requiring a smartphone, computer, or external monitoring software.

The system retrieves battery information from the BMS and displays important parameters on the CYD screen.

## 2. Hardware

The project uses the following hardware:

- ESP32 (ESP-WROOM-32)
- CYD (Cheap Yellow Display)
- ILI9341 TFT display
- Display resolution: 240 × 320 pixels
- JK-BMS with Bluetooth Low Energy support

### Hardware Connection

The ESP32 controller and TFT display are integrated into the CYD board.

Communication with the JK-BMS is performed wirelessly using Bluetooth Low Energy (BLE).

## 3. Software and Development Environment

The project is developed using:

- Visual Studio Code
- PlatformIO IDE
- Arduino Framework for ESP32

### Libraries

- **NimBLE** — Bluetooth Low Energy communication.
- **TFT_eSPI** — TFT display control and graphics rendering.

## 4. JK-BMS Communication

The system communicates with the JK-BMS using Bluetooth Low Energy.

### BLE Parameters

| Parameter | Value |
|---|---|
| Communication protocol | Bluetooth Low Energy |
| Service UUID | `FFE0` |
| Characteristic UUID | `FFE1` |
| Device information request | `0x97` |
| Cell information request | `0x96` |
| Packet header | `55 AA EB 90` |

The received BLE packets are parsed and processed by the ESP32.

### Monitored Parameters

The system is designed to retrieve the following battery information:

- Total battery voltage
- Battery current (charging/discharging)
- Battery power
- State of Charge (SOC)
- Individual cell voltages
- Temperature readings
- Cell balancing status
- Additional BMS parameters

The monitoring interface supports displaying information for up to 16 battery cells.

## 5. Project Structure

```text
CYD_JKBMS-monitoring-via-bluetooth/
│
├── include/
├── lib/
├── src/
│   └── main.cpp
│
├── test/
│
├── platformio.ini
├── .gitignore
└── README.md
```

## 6. Installation and Compilation

### Step 1. Clone the Repository

Open a terminal and execute:

```bash
git clone https://github.com/REZUL/CYD_JKBMS-monitoring-via-bluetooth.git
```

### Step 2. Open the Project

Open the downloaded project folder in Visual Studio Code with PlatformIO IDE installed.

### Step 3. Install Dependencies

PlatformIO uses the `platformio.ini` configuration file to define the target platform, framework, and required libraries.

Wait for PlatformIO to install the project dependencies.

### Step 4. Compile the Firmware

Select **Build** in the PlatformIO toolbar.

Alternatively, use the terminal:

```bash
pio run
```

### Step 5. Upload the Firmware

Connect the CYD board to your computer using USB and execute:

```bash
pio run -t upload
```

## 7. Configuration

Before using the device, check the following settings:

- ILI9341 display configuration.
- TFT_eSPI library configuration.
- BLE connection parameters.
- JK-BMS MAC address, if specified in the firmware.

## 8. How It Works

1. The ESP32 initializes the graphical user interface.
2. The BLE module is initialized.
3. The device scans for and connects to the JK-BMS.
4. The firmware sends requests to retrieve BMS information.
5. Incoming data packets are validated and processed.
6. Battery parameters are displayed on the TFT screen.

## 9. Troubleshooting

| Problem | Possible Solution |
|---|---|
| JK-BMS is not detected | Check whether Bluetooth is enabled and verify the MAC address |
| Connection cannot be established | Check BLE Service and Characteristic UUIDs |
| No data is received | Check request commands and BMS response format |
| Display does not work | Verify TFT_eSPI configuration |
| Compilation errors | Check dependencies in `platformio.ini` |

## 10. License

License: GPL-3.0 license

---

**Author:** REZUL

**GitHub Repository:** https://github.com/REZUL/CYD_JKBMS-monitoring-via-bluetooth

