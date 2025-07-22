# ESP32 Firebase Irrigation Control System

## Project Overview
This project provides an Arduino sketch for an ESP32 microcontroller, enabling remote control and monitoring of an irrigation system through the Firebase Realtime Database. It supports controlling multiple irrigation valves, managing motor start/stop operations, and switching between automatic and manual modes.

## Key Features

* **Firebase Integration:** Real-time synchronization of device states and comprehensive logging with Google Firebase.
* **Remote Control:** Easily operate irrigation valves, start/stop the motor, and switch between 'Auto' and 'Manual' irrigation modes via the Firebase console or a custom application.
* **Timer Functionality:** Each valve can be configured with an individual timer, allowing it to automatically turn off after a specified duration.
* **Motor Status Monitoring:** Detects whether the irrigation motor is running and identifies 'dry run' conditions, providing immediate feedback.
* **Automatic Reconnection:** Robust handling of WiFi and Firebase disconnections, ensuring continuous operation.
* **Status Reporting:** Responds to status check requests from Firebase, providing the current device time and operational status.
* **Detailed Logging:** Logs all significant events (device ON/OFF, motor status changes, fault detections) directly to Firebase for historical tracking and debugging.
* **Visual Indicators:** Dedicated LEDs on the ESP32 provide immediate visual feedback on WiFi and Firebase connection status.

## Hardware Requirements

To implement this project, you will need the following components:

* **ESP32 Development Board:** The core microcontroller.
* **Relay Module:** Essential for controlling high-power devices like the motor and valves.
    * One relay for `MOTOR_START_RELAY`
    * One relay for `MOTOR_STOP_RELAY`
    * One relay for `AUTO` mode activation
    * One relay for `MANUAL` mode activation
    * Ten relays for `V1` through `V10` valves
* **Dry Run Sensor:** Connected to `MOTOR_STATUS_PIN`. This sensor should output a `LOW` signal when the motor is actively running (or adjust code based on your sensor's logic).
* **LEDs:**
    * One for `WIFI_LED_PIN` (WiFi connection status)
    * One for `FIREBASE_LED_PIN` (Firebase connection status)
* **Resistors:** Appropriate resistors for the LEDs.
* **Power Supply:** A stable power supply for both the ESP32 and the relay module.

## Software Requirements

Before compiling and uploading the sketch, ensure you have:

* **Arduino IDE:** The development environment.
* **ESP32 Board Package:** Installed in your Arduino IDE (`Tools > Board > Boards Manager`).
* **Required Libraries:**
    * `WiFi.h` (Standard Arduino WiFi Library - usually comes with the ESP32 package)
    * `Firebase_ESP_Client` by Mobizt (Version 4.2.0 or higher is recommended)
        * Install via Arduino IDE: `Sketch > Include Library > Manage Libraries...` and search for "Firebase ESP32 Client".
    * `time.h` (Standard C library for time functions - part of the ESP32 core)

## Configuration Steps

**IMPORTANT:** You **must** configure your WiFi credentials and Firebase project details within the sketch.

### 1. Firebase Project Setup

Follow these steps in the [Firebase Console](https://console.firebase.google.com/):

* **Create Project:** Create a new project or select an existing one.
* **Enable Realtime Database:** Navigate to 'Build' -> 'Realtime Database' and enable it.
    * Note your **`DATABASE_URL`** (e.g., `https://your-project-id-default-rtdb.asia-southeast1.firebasedatabase.app/`).
* **Get API Key:** Go to 'Project settings' -> 'General' -> 'Web API Key'. Copy this as your **`API_KEY`**.
* **Database Structure:** You *must* create the following initial data structure in your Realtime Database. The ESP32 relies on these specific paths:

    ```json
    {
      "control": {
        "AUTO": {
          "on": false,
          "timer": 0,
          "timerenabled": false
        },
        "MANUAL": {
          "on": false,
          "timer": 0,
          "timerenabled": false
        },
        "V1": {
          "on": false,
          "timer": 0,
          "timerenabled": false
        },
        "V2": {
          "on": false,
          "timer": 0,
          "timerenabled": false
        },
        "V3": { "on": false, "timer": 0, "timerenabled": false },
        "V4": { "on": false, "timer": 0, "timerenabled": false },
        "V5": { "on": false, "timer": 0, "timerenabled": false },
        "V6": { "on": false, "timer": 0, "timerenabled": false },
        "V7": { "on": false, "timer": 0, "timerenabled": false },
        "V8": { "on": false, "timer": 0, "timerenabled": false },
        "V9": { "on": false, "timer": 0, "timerenabled": false },
        "V10": { "on": false, "timer": 0, "timerenabled": false }
      },
      "logs": {
        // Log entries will be automatically pushed here by the ESP32.
      },
      "status": {
        "check": false,   // Set to 'true' from your app to request a status update.
        "response": "",   // ESP32 will update this with the current time when 'check' is true.
        "dryrun": false   // 'true' if a dry run condition is detected.
      }
    }
    ```
* **Firebase Rules (CRITICAL FOR SECURITY):**
    For initial testing, you might use very permissive rules. **However, for production, secure your rules immediately.**

    **For Testing (INSECURE - DO NOT USE IN PRODUCTION):**
    ```json
    {
      "rules": {
        ".read": true,
        ".write": true
      }
    }
    ```
    **For Production (Example - Adjust to your needs):**
    You would typically limit write access only to authenticated users and specific paths.
    Refer to Firebase Security Rules documentation for best practices.

### 2. Arduino Sketch Modifications

Open the `ESP32_Firebase_Irrigation.ino` (or whatever you name your `.ino` file) in the Arduino IDE and modify the following lines:

```c++
// WiFi Credentials
#define WIFI_SSID "YOUR_WIFI_NAME"        // Replace with your WiFi network name
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"  // Replace with your WiFi password

// Firebase Project Details
#define API_KEY "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL "YOUR_FIREBASE_DATABASE_URL" // Example: "[https://your-project-id-default-rtdb.asia-southeast1.firebasedatabase.app/](https://your-project-id-default-rtdb.asia-southeast1.firebasedatabase.app/)"
