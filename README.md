# Smart Water Monitoring System using ESP32

## Overview
This project is an ESP32-based smart water monitoring and automation system designed to monitor water quality parameters in real time.

The system measures:
- Temperature
- pH level
- Turbidity
- TDS (Total Dissolved Solids)

Based on sensor readings, the system automatically controls a water pump and provides alerts using LEDs, buzzer, and LCD display.

---

# Features
- Real-time water quality monitoring
- Automatic pump control
- LCD display output
- Buzzer warning alerts
- LED status indication
- Multi-sensor integration
- ESP32-based embedded system
- Sensor data processing and smoothing

---

# Components Used
- ESP32
- DS18B20 Temperature Sensor
- pH Sensor
- Turbidity Sensor
- TDS Sensor
- Relay Module
- LCD Display (I2C)
- LEDs
- Buzzer
- Water Pump

---

# Technologies Used
- Embedded C++
- Arduino IDE
- ESP32 Programming
- Sensor Integration
- IoT Concepts
- Automation Logic

---

# Working Principle
1. Sensors continuously monitor water quality parameters.
2. ESP32 processes sensor data.
3. LCD displays live readings.
4. LEDs and buzzer indicate water condition.
5. Pump automatically turns ON/OFF based on predefined thresholds.
```text
eee-flow.ino   -> Main ESP32 source code
README.md      -> Project documentation
