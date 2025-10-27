# Project Overview

This repository contains the firmware for an Energy-Efficient ESP32 IoT Controller built on the ESP-IDF framework and FreeRTOS. It leverages Firebase Realtime Database for seamless, low-latency, two-way communication. The core functionality focuses on reliable monitoring and impulse-based remote control.

---

## Key Features

* **Real-Time Impulse Control**: Utilizes Firebase Streaming (Server-Sent Events) to provide near-instantaneous control of a relay, which is configured for momentary/toggle actions (e.g., PC power switch, stairwell light).

* **Energy Efficiency**: Implements Wi-Fi Modem Sleep (WIFI_PS_MAX_MODEM) to significantly reduce power consumption while maintaining network connectivity.

* **Dual Control Interface**: Supports both remote control (via Firebase) and local control (via a physical button with hardware interrupt-based debouncing).

* **Data Monitoring**: Periodically reads data from a DHT11 sensor and sends temperature/humidity updates to Firebase using secure HTTPS PUT requests.

* **Robust Network Initialization**: Features a custom Wi-Fi Provisioning captive portal, ensuring the device only starts application tasks (Firebase PUT/Stream) once a stable network connection (GOT_IP event) is established.

---

## RTOS Architecture

The application is structured around three dedicated FreeRTOS tasks to ensure stability and responsiveness:

| Task Name | Priority | Stack Size (Bytes) | Role |
| :--- | :--- | :--- | :--- |
| **`ButtonHandler`** | 10 (Highest) | 4096 | Immediate processing of hardware interrupts (debouncing) and sending the toggle command to Firebase. |
| **`FirebaseStream`** | 7 (High) | 8192 | Maintains the persistent, open connection to Firebase, listens for remote commands, and triggers the relay impulse. |
| **`DHT11_Firebase`** | 5 (Low) | 8192 | Handles periodic sensor reading, formatting data, and making HTTPS PUT requests. |

---

## Wiring & Configuration Notes

To ensure proper functionality, particularly the correct operation of the impulse relay, note the following:

* **Relay Pin Choice**: The code is configured to use a GPIO 22 that defaults to LOW at boot to prevent accidental activation.

* **Button Wiring**: The button is configured for an internal pull-up and must be wired between the specified GPIO pin and GND.
