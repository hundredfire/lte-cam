# LTE-CAM

LTE-CAM is an open-source project for capturing photos at scheduled times or on demand (via SMS/call) using a microcontroller (we're using an ESP32-S3) and sending them to a Telegram chat or bot via LTE connectivity. Typical use-cases include remote wildlife monitoring, off-grid time-lapse photography, or any scenario where you need to automatically capture photos and receive them remotely using the Telegram messaging platform.

## Features

- Automates camera captures at user-defined times (e.g., 10 AM and 5 PM by default).
- Triggers on-demand captures by sending an SMS or calling the device's phone number.
- Sends captured photos via Telegram bot to your personal account or a group.
- Sends battery voltage reading for battery monitoring.

## Getting Started

### Hardware Requirements

- A supported microcontroller (e.g., ESP32 or equivalent) with camera support and LTE connectivity (we only tested the lilygo ESP32-S3 A7670E).
- Supported camera module (e.g., OV2640).
- SIM card with data plan.

### Software Prerequisites

- PlatformIO or Arduino IDE installed.
- Required libraries for LTE communication. For the lilygo A7670X, we're using a custom fork of tinyGSM by Lewis He that is [here](https://github.com/lewisxhe/TinyGSM-fork/tree/master)
- This repository (the project files are located in the folder `lte-cam`) cloned to your local development environment (you can discard the "example projects" folder).

### Configuration Steps

#### 1. Set Up Your Telegram Bot

1. **Create a new bot using BotFather**  
    - Start a chat with [@BotFather](https://t.me/BotFather) on Telegram.
    - Send `/newbot` and follow the prompts to receive your **Bot Token**.
    - Save this token for the `secrets.h` file.

2. **Get your Telegram User ID**
    - Start a chat with [@IDbot](https://t.me/myidbot).
    - Send `/start`—you will receive your numeric user ID.
    - Use this ID in your `secrets.h` file (for the `TELEGRAM_CHAT_ID`).

#### 2. Rename & Edit .h Files

1. **Rename** `secrets_example.h` to `secrets.h`:
    ```
    mv secrets_example.h secrets.h
    ```
2. **Edit `secrets.h`** and replace the example credentials with your own:
    - LTE APN, username, and password (for your SIM provider)
    - Telegram Bot Token
    - Telegram User ID (the chat where photos will be sent)

    > ⚠️ ** If you fork this project, do NOT commit your actual `secrets.h` file to public repositories!**
    
   **Edit `utilities.h`** and uncomment the definition that mentions your board (for example, we have uncommented the line mentioning "LILYGO_A7670X_S3_STAN" for the lilygo ESP32-S3 A7670E)

#### 3. Customize parameters (wake up time and more)

- In the "General settings" section of the code, you can define at what time the board will wake up.
- Timezones are defined with TZ_INFO (see https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for more)
- The internal clock of the lilygo A7670E tends to stray by a few minutes and the sleep duration can be off. We added WAKEUP_GRACE_PERIOD and WAKE_TOLERANCE_MINUTES as separate tolerance margins (for late and early wakes respectively) that can be defined (in seconds). This might not be necessary in boards with an internal crystal.

#### 4. Build & Flash

- Use your IDE or PlatformIO to build and upload the firmware to your microcontroller.
- Make sure to follow the manufacturer's instructions to get this right (We followed this documentation for our board: [ESP32S3-A7670](https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/blob/main/docs/en/esp32s3/a7670x-s3-standard/REAMDE.MD)

#### 5. Power Up

- Insert your SIM card (if applicable).
- Connect your camera module.
- Power the device: it should connect to LTE, initialize the camera (takes one photo to set auto-exposure and a second one that will be sent), send a photo to the Telegram group, read the battery voltage reading and send it as a second message, then go to sleep. It will then wake up and do the same routine at the scheduled times or when triggered by an SMS or phone call.

## Troubleshooting
- Enable DEBUG_MODE in the initial parameters to bypass the schedule and do the routine every 2 minutes. Make sure to enable the serial monitor when flashing the board. The 2 minutes delay can be changed with DEBUG_SLEEP_SECONDS.
- Ensure your SIM has active data and that your LTE antenna is properly connected. The camera ribbon cable can be a bit fiddly. Make sure it is properly seated.
- Double-check your `secrets.h` credentials.
- Make sure you’re using the correct microcontroller and firmware board configuration during flashing (notably pay attention to the RAM type, the programmer). Follow your board manufacturer’s guides for flashing and hardware setup.

## Future Features

* **Image Enhancements:**
  - Parameter to change photo orientation
  - Increase photo resolution
* **SMS Remote Control:**
  - Change schedule
  - Update photo parameters (resolution/orientation)
* **Implement satellite cameras**

