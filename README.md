# LTE-CAM

LTE-CAM is an open-source project for capturing photos at scheduled times using a microcontroller (we're using an ESP32-S3) and sending them to a Telegram chat or bot via LTE connectivity. Typical use-cases include remote wildlife monitoring, off-grid time-lapse photography, or any scenario where you need to automatically capture photos and receive them remotely using the Telegram messaging platform.

## Features

- Automates camera captures at user-defined times (e.g., 10 AM and 3 PM by default).
- Sends captured photos via Telegram bot to your personal account or a group.
- Secure storage and flexible configuration for your Telegram credentials and LTE details via a secrets.h file.

## Getting Started

### Hardware Requirements

- A supported microcontroller (e.g., ESP32, ESP8266, or equivalent) with camera support and LTE connectivity (we only tested on ESP32-S3).*
- Supported camera module (e.g., OV2640 for ESP32-CAM boards).
- SIM card with data plan (if using LTE module).

*The required hardware may differ based on the specific code; refer to your board/module's documentation for compatibility.

### Software Prerequisites

- PlatformIO or Arduino IDE installed.
- Required libraries for camera (esp-camera), LTE. For the A7670X, we're using a custom fork of tinyGSM by Lewsis Xhe that is [here](https://github.com/lewisxhe/TinyGSM-fork/tree/master)
- This repository (`lte-cam`) cloned to your local development environment.

### Configuration Steps

#### 1. Rename & Edit Secrets File

1. **Rename** `secrets_example.h` to `secrets.h`:
    ```
    mv secrets_example.h secrets.h
    ```
2. **Edit `secrets.h`** and replace the example credentials with your own:
    - LTE APN, username, and password (for your SIM provider)
    - Telegram Bot Token
    - Telegram User ID (the chat where photos will be sent)

    > ⚠️ **Do NOT commit your actual `secrets.h` file to public repositories!**

#### 2. Set Up Your Telegram Bot

1. **Create a new bot using BotFather**  
    - Start a chat with [@BotFather](https://t.me/BotFather) on Telegram.
    - Send `/newbot` and follow the prompts to receive your **Bot Token**.
    - Save this token for the `secrets.h` file.

2. **Get your Telegram User ID**
    - Start a chat with [@IDbot](https://t.me/myidbot).
    - Send `/start`—you will receive your numeric user ID.
    - Use this ID in your `secrets.h` file (for the `TELEGRAM_CHAT_ID`).

#### 3. Customize Photo Times

Within the source code, you will find the following lines near the end:

```cpp
int target1 = 10 * 3600; // 10:00 (10 AM)
int target2 = 15 * 3600; // 15:00 (3 PM)
```

- `target1` and `target2` represent the **seconds since midnight** when a photo should be taken.
- `10 * 3600` = 36,000 (i.e., 10 AM).
- `15 * 3600` = 54,000 (i.e., 3 PM).
- To change capture times, modify the `10` or `15` values to your desired **hour** (in 24-hour format).  
  For example, for 8 AM and 6 PM:  
  `int target1 = 8 * 3600;`  
  `int target2 = 18 * 3600;`

#### 4. Build & Flash

- Use your IDE or PlatformIO to build and upload the firmware to your microcontroller.
- Make sure to follow the manufacturer's instructions to get this right (We followed this documentation for our board: [ESP32S3-A7670](https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/blob/main/docs/en/esp32s3/a7670x-s3-standard/REAMDE.MD)

#### 5. Power Up

- Insert your SIM card (if applicable).
- Connect your camera module.
- Power the device: it should connect to LTE, initialize the camera, send a photo, then go to sleep. It will then wake up and send photos at the scheduled times to your Telegram group.

## Troubleshooting
- Enable DEBUG_MODE in the initial parameters if needed
- Ensure your SIM has active data and that your LTE antenna is properly connected.
- Double-check your `secrets.h` credentials.
- Make sure you’re using the correct microcontroller and firmware board configuration.
- Follow your board manufacturer’s guides for flashing and hardware setup.

