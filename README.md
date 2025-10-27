# Wetlands Automation System

An IoT-based automation system for monitoring and controlling wetland reactors in environmental research applications.

## Overview

This system implements a complete automation solution for wetland reactor monitoring using a distributed sensor network. The architecture consists of a master ESP32 unit with embedded HMI (LCD display and joystick) that coordinates data collection from up to 9 slave units via ESP-NOW wireless protocol. The master unit manages local control, real-time visualization, alarm monitoring, and fault-tolerant data logging to SD card.

## System Architecture

The system uses a master-slave architecture where the master ESP32 coordinates periodic data collection from distributed slave units. Communication is handled via ESP-NOW protocol for low-power wireless operation. The master unit features an interactive HMI for field operation, stores timestamped data with fault redundancy using an RTC module, and implements comprehensive alarm management for sensor anomalies and communication timeouts.

## Hardware Components

**Master Unit:**
- ESP32 microcontroller
- ST7735 LCD display (SPI interface)
- Analog joystick for menu navigation
- DS1307 RTC module (I2C)
- SD card module for data logging
- 3x RGB LED indicators for system status
- Custom PCB designed in KiCAD
- 3D-printed enclosure (SolidWorks)

**Slave Units:**
- ESP32 microcontroller
- Voltage and temperature sensors
- Wireless communication via ESP-NOW

## Software Implementation

The firmware is written in C/C++ using the Arduino framework. Key features include:

- **ESP-NOW Communication:** Implements request-response protocol with ACK for reliable data transfer between master and slaves
- **Real-time Clock:** DS1307 RTC provides accurate timestamps for all data points
- **Data Collection Cycle:** Configurable 30-minute intervals for automated sensor polling
- **Buffered Writing:** Collects data from all slaves before writing to SD card to minimize file operations
- **Alarm System:** Monitors voltage thresholds, temperature ranges, sensor failures, and communication timeouts
- **Interactive HMI:** Multi-screen menu system for viewing live data, historical logs, system status, and alarms
- **Fault Tolerance:** Redundant write verification and error recovery mechanisms

### Libraries Used

- **ESP-NOW & WiFi** version & version 1.2.7, Wireless communication stack
- **RTClib** version 2.1.4, DS1307 real-time clock interface
- **SD** version 1.3.0, SD card file system management
- **Adafruit_GFX & Adafruit_ST7735** version 1.12.1 & version 1.11.0, Display graphics and ST7735 driver
- **Wire** I2C communication for RTC

## Key Features

- Automatic data collection at configurable intervals (default: 30 minutes)
- Support for up to 9 distributed slave nodes
- Real-time monitoring with timestamped data
- Comprehensive alarm system with visual and logged notifications
- Field-accessible HMI with joystick navigation
- CSV data format for easy post-processing
- System health monitoring with LED status indicators
- Communication timeout detection and alerts

## Getting Started

### Hardware Setup

1. Assemble the master PCB with ESP32, display, RTC, SD module, and joystick
2. Install components in the 3D-printed enclosure
3. Program each slave unit with unique MAC address
4. Deploy slave units at monitoring locations
5. Power on the system and verify all slaves are detected

### Software Installation

```bash
git clone https://github.com/Danntav/wetlands-automation.git
cd wetlands-automation
```

Install required libraries in Arduino IDE, configure ESP32 board settings, and update slave MAC addresses in the master firmware before uploading.

## Project Structure

```
wetlands-automation/
├── firmware/
│   ├── master/
│   │   ├── master.ino
│   │   ├── MenuManager.h
│   │   ├── LedAnimations.h
│   │   └── DataStructures.h
│   └── slave/
├── hardware/
│   ├── pcb/              # KiCAD PCB files
│   └── enclosure/        # SolidWorks 3D models
└── docs/
```

## HMI Operation

The embedded Human-Machine Interface provides:
- **Main Menu:** System overview with active slave count and last update times
- **Board Data:** Individual slave readings with voltage and temperature
- **Alarms:** Real-time alerts for sensor anomalies and communication failures
- **Error Log:** Historical system errors with timestamps
- **System Info:** RTC status, SD card status, and overall health

Navigation is performed using the analog joystick (up/down/left/right/click).

## Data Logging

Data is logged in CSV format with the following structure:
```
Date,Time,BoardID,Voltage,Temperature
```

The system implements:
- Buffered writes to minimize SD card wear
- Automatic file creation with headers
- Timestamp verification via RTC
- Error logging for failed operations
- Visual feedback via LED indicators

## Alarm System

The system monitors and alerts for:
- Voltage readings outside safe range (configurable thresholds)
- Temperature anomalies beyond expected limits
- Sensor hardware failures (invalid readings)
- Communication timeouts (no response from slaves)

Alarms are displayed on the HMI, stored in memory, and indicated via blinking red LED.

## Configuration

System parameters can be adjusted in the firmware:
- `COLLECTION_INTERVAL_MS`: Data collection frequency (default: 30 minutes)
- `VOLTAGE_MIN/MAX`: Voltage alarm thresholds
- `TEMP_MIN/MAX`: Temperature alarm thresholds
- `COMM_TIMEOUT_MS`: Communication timeout duration
- `TOTAL_SLAVES`: Number of connected slave units

## Contributing

Contributions, issues, and feature requests are welcome. Feel free to check the issues page or submit pull requests.

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Author

**Daniel Oliveira Tavares**

LinkedIn: [daniel-otavares](https://www.linkedin.com/in/daniel-otavares)

Email: danieloftavares@gmail.com

GitHub: [@Danntav](https://github.com/Danntav)

## Future Improvements

- [ ] Cloud connectivity for remote monitoring via MQTT or HTTP
- [ ] Mobile app integration for real-time alerts
- [ ] Additional sensor types (pH, dissolved oxygen, flow rate)
- [ ] Solar power system with battery management
- [ ] Web dashboard for data visualization and historical analysis
- [ ] Machine learning for predictive maintenance
- [ ] LoRa support for extended range communication