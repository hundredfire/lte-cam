// #define TINY_GSM_MODEM_A7670  //already defined in utilities.h
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

#include <Arduino.h>
#include <Wire.h>                    
#include <time.h>
#include "utilities.h"               
#include "esp_camera.h"
#include <driver/gpio.h>
#include <TinyGsmClient.h>
#include <esp32-hal-adc.h>
#include "sleep_math.h"

// Load our private credentials
#include "secrets.h"

// ==========================================
//               GENERAL SETTINGS
// ==========================================
const bool DEBUG_MODE = false;           
const int  DEBUG_SLEEP_SECONDS = 120;   
// Schedule times in HH:mm format
const char* schedules[] = {"10:00", "17:00"};

// Timezone handling using POSIX standard (Paris: CET/CEST)
// See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for more
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";
// ==========================================

#define SerialMon Serial
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);

void sendPhotoViaBuiltInHTTP(camera_fb_t * fb);
bool waitModemResponse(int timeoutMs, String expectedToken = "OK");
bool setCameraPower(bool enable);
bool syncTime(int *year, int *month, int *day, int *hour, int *min, int *sec); // Removed timezone float
bool manualNtpSync(int *year, int *month, int *day, int *hour, int *min, int *sec);
bool checkInternet();
uint32_t readBatteryVoltage();
void sendTelegramMessage(String text);

void powerOnModem() {
    SerialMon.println("Powering on Modem (Robust Sequence)...");

#ifdef BOARD_POWERON_PIN
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
#endif

    delay(2000);

#ifdef MODEM_RESET_PIN
    // Release reset GPIO hold if it was held during sleep
    gpio_hold_dis((gpio_num_t)MODEM_RESET_PIN);

    // Pulse Reset Pin (Hardware Reset)
    SerialMon.println("Pulsing MODEM_RESET_PIN...");
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    delay(100);
    digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
    delay(2600);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
#endif

#ifdef MODEM_DTR_PIN
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, LOW);
#endif

#ifdef MODEM_FLIGHT_PIN
    pinMode(MODEM_FLIGHT_PIN, OUTPUT);
    digitalWrite(MODEM_FLIGHT_PIN, HIGH);
#endif

    SerialMon.println("Pulsing BOARD_PWRKEY_PIN...");
    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(MODEM_POWERON_PULSE_WIDTH_MS);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);

    delay(10000);
}

void enterDeepSleep(int hour, int min, int sec) {
    SerialMon.println("Preparing for Deep Sleep (SMS Wakeup Mode)...");

    // Enable Sleep Mode on Modem
    // AT+CSCLK=1 : Enable sleep mode. Modem sleeps when DTR is high.
    SerialAT.println("AT+CSCLK=1");
    if (waitModemResponse(2000)) {
        SerialMon.println("Modem sleep mode enabled (CSCLK=1).");
    }

#ifdef MODEM_DTR_PIN
    // Pull DTR HIGH to let modem sleep
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, HIGH);
    gpio_hold_en((gpio_num_t)MODEM_DTR_PIN); // Hold DTR High during sleep
    gpio_deep_sleep_hold_en();
#endif

#ifdef BOARD_POWERON_PIN
    // Ensure Modem Power remains ON
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
    gpio_hold_en((gpio_num_t)BOARD_POWERON_PIN);
    gpio_deep_sleep_hold_en();
#endif

    // Ensure RI (Ring Indicator) is configured to wake us up
    // MODEM_RING_PIN should be connected to an RTC GPIO
#ifdef MODEM_RING_PIN
    pinMode(MODEM_RING_PIN, INPUT_PULLUP);
    // Configure wakeup on LOW (RI goes low on incoming SMS/Call)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)MODEM_RING_PIN, 0);
    SerialMon.printf("Configured wakeup on PIN %d (RI)\n", MODEM_RING_PIN);
#endif

    SerialMon.println("Disabling camera power...");
    setCameraPower(false);

    SerialMon.println("Disabling I2C...");
    Wire.end();

#ifdef MODEM_RESET_PIN
    // Keep it low (inactive) during the sleep period to prevent accidental resets
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    gpio_hold_en((gpio_num_t)MODEM_RESET_PIN);
    gpio_deep_sleep_hold_en();
#endif

    int sleepSeconds = (DEBUG_MODE) ? DEBUG_SLEEP_SECONDS : ((hour != -1) ? calculateSleepSecondsFromSchedules(hour, min, sec, schedules, sizeof(schedules) / sizeof(schedules[0])) : 3600);
    SerialMon.printf("Going to deep sleep for %d seconds...\n", sleepSeconds);

    // Enable Timer Wakeup (for schedule)
    esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);

    delay(200);
    esp_deep_sleep_start();
    SerialMon.println("This will never be printed");
}

void setup() {
    camera_fb_t * fb = nullptr; 
    int year = 0, month = 0, day = 0, hour = -1, min = 0, sec = 0;
    bool gotIP = false; 

    SerialMon.begin(115200);
    delay(1000);

    // Set Timezone rules
    setenv("TZ", TZ_INFO, 1);
    tzset();
    
    SerialMon.println("\n--- Telegram LTE Camera Starting [BUILT-IN HTTP MODE] ---");
    SerialMon.println("Firmware Version: TimeSync-Fix-v13-RobustRetry");

    // Initialize ADC for Battery Reading
    analogSetAttenuation(ADC_11db);
    analogReadResolution(12);
#if CONFIG_IDF_TARGET_ESP32
    analogSetWidth(12);
#endif

#ifdef BOARD_POWER_SAVE_MODE_PIN
    pinMode(BOARD_POWER_SAVE_MODE_PIN, OUTPUT);
    digitalWrite(BOARD_POWER_SAVE_MODE_PIN, HIGH);
#endif

    SerialMon.println("Powering on Camera PMIC...");
    if (!setCameraPower(true)) {
        SerialMon.println("The camera PMIC failed to start! Going to sleep.");
        enterDeepSleep(hour, min, sec);
        return;
    }

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAMERA_Y2_PIN;
    config.pin_d1 = CAMERA_Y3_PIN;
    config.pin_d2 = CAMERA_Y4_PIN;
    config.pin_d3 = CAMERA_Y5_PIN;
    config.pin_d4 = CAMERA_Y6_PIN;
    config.pin_d5 = CAMERA_Y7_PIN;
    config.pin_d6 = CAMERA_Y8_PIN;
    config.pin_d7 = CAMERA_Y9_PIN;
    config.pin_xclk = CAMERA_XCLK_PIN;
    config.pin_pclk = CAMERA_PCLK_PIN;
    config.pin_vsync = CAMERA_VSYNC_PIN;
    config.pin_href = CAMERA_HREF_PIN;
    config.pin_sccb_sda = CAMERA_SIOD_PIN;
    config.pin_sccb_scl = CAMERA_SIOC_PIN;
    config.pin_pwdn = CAMERA_PWDN_PIN;
    config.pin_reset = CAMERA_RESET_PIN;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_SVGA; 
    config.pixel_format = PIXFORMAT_JPEG;  
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    if (esp_camera_init(&config) != ESP_OK) {
        SerialMon.println("Camera init failed! Going to sleep.");
        enterDeepSleep(hour, min, sec);
        return;
    }

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    // --- MAIN RETRY LOOP ---
    int retryCount = 0;
    const int MAX_RETRIES = 5;
    bool success = false;
    bool skipPowerOn = false;
    bool shouldTakeCapture = false;

    // Check wakeup reason and try to reuse modem state if possible
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        SerialMon.println("Woke up from Power On/Reset. Scheduling capture.");
        shouldTakeCapture = true;
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        SerialMon.println("Woke up from Timer (Scheduled). Scheduling capture.");
        shouldTakeCapture = true;
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        SerialMon.println("Woke up from EXT0 (Modem RI). Checking for triggers...");
        // For now, we don't automatically capture on RI unless we find a specific SMS
        // but the user says it sends a photo. We should check if we should capture.
    }

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
         SerialMon.println("Checking modem status...");

#ifdef MODEM_DTR_PIN
         gpio_hold_dis((gpio_num_t)MODEM_DTR_PIN);
         pinMode(MODEM_DTR_PIN, OUTPUT);
         digitalWrite(MODEM_DTR_PIN, LOW); // Wake modem from sleep
         delay(100);
#endif
#ifdef BOARD_POWERON_PIN
         gpio_hold_dis((gpio_num_t)BOARD_POWERON_PIN);
         pinMode(BOARD_POWERON_PIN, OUTPUT);
         digitalWrite(BOARD_POWERON_PIN, HIGH);
#endif
#ifdef MODEM_RESET_PIN
         gpio_hold_dis((gpio_num_t)MODEM_RESET_PIN);
#endif

         // Attempt to talk to modem
         SerialAT.println("AT");
         delay(100);
         if (modem.testAT()) {
             SerialMon.println("Modem is ALIVE! Skipping power-on sequence.");
             skipPowerOn = true;
         } else {
             SerialMon.println("Modem not responding. Will power on normally.");
         }
    }

    while (retryCount < MAX_RETRIES) {
        SerialMon.printf("\n=== Connection Attempt %d/%d ===\n", retryCount + 1, MAX_RETRIES);

        if (!skipPowerOn) {
            powerOnModem();
        } else {
             // If we skipped powerOn, ensure we only try once; if it fails, next loop will powerOn
             skipPowerOn = false;
        }

        SerialMon.println("Initializing modem...");
        if (!modem.init()) {
            SerialMon.println("Modem init failed!");
            retryCount++;
            continue;
        }

        // Configure RI pin for SMS notification behavior
        SerialAT.println("AT+CFGRI=1"); // RI pin will go low when URC received
        waitModemResponse(1000);
        SerialAT.println("AT+CNMI=2,1"); // New message indication
        waitModemResponse(1000);

        // Clean up SMS storage to ensure we can receive new triggers
        SerialAT.println("AT+CMGD=1,4");
        waitModemResponse(2000);

        if (String(GSM_PIN) != "") {
            if (modem.getSimStatus() != 3) modem.simUnlock(GSM_PIN);
        }

        modem.setNetworkMode(MODEM_NETWORK_AUTO);

        SerialMon.print("Waiting for network...");
        if (!modem.waitForNetwork(60000L)) {
            SerialMon.println(" Network wait failed.");
            retryCount++;
            continue;
        }
        SerialMon.println(" OK");

        SerialMon.print("Connecting to APN...");
        if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
            SerialMon.println(" GPRS Connect failed.");
            retryCount++;
            continue;
        }
        SerialMon.println(" done.");

        SerialMon.print("Waiting for IP address...");
        gotIP = false;
        for (int i = 0; i < 60; i++) {
            IPAddress local = modem.localIP();
            if (local[0] != 0 && local[0] != 255) {
                SerialMon.print(" OK! IP: ");
                SerialMon.println(local);
                gotIP = true;
                break;
            }
            delay(1000);
            SerialMon.print(".");
        }
        SerialMon.println();

        if (!gotIP) {
            SerialMon.println("Network failure: Could not get IP.");
            retryCount++;
            continue;
        }

        // Attempt Time Sync (Only once per retry loop iteration)
        if (!syncTime(&year, &month, &day, &hour, &min, &sec)) {
            SerialMon.println("Failed to sync time!");

            // Optional: If time sync fails, maybe we don't want to fail the whole process if we can get time from system?
            // But system time is set by syncTime.
            // If we fail time sync, we might not know when to sleep next.
            // But let's assume we retry.
            retryCount++;
            continue;
        }

        // Re-fetch time after setup to ensure we have the latest adjusted local time
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            year = timeinfo.tm_year + 1900;
            month = timeinfo.tm_mon + 1;
            day = timeinfo.tm_mday;
            hour = timeinfo.tm_hour;
            min = timeinfo.tm_min;
            sec = timeinfo.tm_sec;
            SerialMon.printf("Current Local Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                            year, month, day, hour, min, sec);
        } else {
            SerialMon.println("Failed to obtain local time from system clock.");
            // If we can't get local time, we might still proceed but sleep calculation will be wrong.
        }

        // If we woke up from EXT0, check for "CAPTURE" SMS
        if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
            SerialMon.println("Checking for 'CAPTURE' SMS trigger...");
            SerialAT.println("AT+CMGL=\"REC UNREAD\"");
            long start = millis();
            while (millis() - start < 5000) {
                if (SerialAT.available()) {
                    String line = SerialAT.readString();
                    line.toUpperCase();
                    if (line.indexOf("CAPTURE") != -1) {
                        SerialMon.println("SMS 'CAPTURE' trigger found!");
                        shouldTakeCapture = true;
                        break;
                    }
                }
            }
            // Mark all read or delete triggers
            SerialAT.println("AT+CMGD=1,4");
            waitModemResponse(2000);
        }

        success = true;
        break;
    }

    if (!success) {
        SerialMon.println("\nFATAL: Failed to connect after multiple retries.");
        SerialMon.println("Hard Resetting System (ESP.restart)...");
        delay(2000);
        ESP.restart();
    }

    if (shouldTakeCapture) {
        // --- START OF WARM-UP LOOP ---
        SerialMon.println("Warming up sensor for auto-exposure...");
        for (int i = 0; i < 5; i++) {
            fb = esp_camera_fb_get();
            if (fb) {
                esp_camera_fb_return(fb); // Immediately release the overexposed frame
                fb = nullptr;
                delay(200); // Short delay to let the sensor adjust its AGC/AEC
            }
        }
        // --- END OF WARM-UP LOOP ---

        fb = esp_camera_fb_get();
        if (fb) {
            SerialMon.printf("Photo taken! Size: %zu bytes. Uploading to Telegram...\n", fb->len);
            sendPhotoViaBuiltInHTTP(fb);
            esp_camera_fb_return(fb);

            // Send Battery Status
            uint32_t vol = readBatteryVoltage();
            if (vol > 0) {
                String msg = "Battery Voltage: " + String(vol) + " mV";
                sendTelegramMessage(msg);
            }
        }
    } else {
        SerialMon.println("Skipping capture for this wakeup (Unsolicited modem trigger).");
    }

    enterDeepSleep(hour, min, sec);
}

void loop() {}

bool checkInternet() {
    SerialMon.print("Checking Internet connectivity (TCP -> google.com:80)... ");
    if (client.connect("google.com", 80)) {
        SerialMon.println("OK");
        client.stop();
        return true;
    } else {
        SerialMon.println("Failed");
        return false;
    }
}

bool setCameraPower(bool enable) {
    static bool started = false;
    if (!started) {
        Wire.begin(BOARD_SDA_PIN, BOARD_SCL_PIN);
        Wire.beginTransmission(0x28);
        if (Wire.endTransmission() != 0) {
            Serial.println("Camera power chip not found!");
            return false;
        }
    }
    started = true;

    uint8_t vdd[] = {
        0x03,   
        0x7C,   
        0x7C,   
        0xCA,   
        0xB1    
    };
    Wire.beginTransmission(0x28);
    Wire.write(vdd, sizeof(vdd) / sizeof(vdd[0]));
    Wire.endTransmission();

    uint8_t control[] = {0x0E, 0x0F};
    if (!enable) {
        control[1] = 0x00;
    }
    Wire.beginTransmission(0x28);
    Wire.write(control, sizeof(control) / sizeof(control[0]));
    Wire.endTransmission();
    
    if (enable) {
        control[1] = 0x00;
        Wire.beginTransmission(0x28);
        Wire.write(control, sizeof(control) / sizeof(control[0]));
        Wire.endTransmission();
        delay(300);
        control[1] = 0x0F;
        Wire.beginTransmission(0x28);
        Wire.write(control, sizeof(control) / sizeof(control[0]));
        Wire.endTransmission();
        delay(100);
    }
    return true;
}

void sendPhotoViaBuiltInHTTP(camera_fb_t * fb) {
    String boundary = "----LilyGoBoundary7MA4YWxkTrZu0gW";
    
    String bodyHead = "--" + boundary + "\r\n";
    bodyHead += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
    bodyHead += chatId + "\r\n";
    bodyHead += "--" + boundary + "\r\n";
    bodyHead += "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n";
    bodyHead += "Content-Type: image/jpeg\r\n\r\n";
    
    String bodyTail = "\r\n--" + boundary + "--\r\n";
    
    uint32_t totalLen = bodyHead.length() + fb->len + bodyTail.length();

    SerialAT.println("AT+HTTPINIT");
    waitModemResponse(2000);

    String url = "https://api.telegram.org/bot" + botToken + "/sendPhoto";
    SerialAT.println("AT+HTTPPARA=\"URL\",\"" + url + "\"");
    waitModemResponse(2000);

    SerialAT.println("AT+HTTPPARA=\"CONTENT\",\"multipart/form-data; boundary=" + boundary + "\"");
    waitModemResponse(2000);

    SerialAT.print("AT+HTTPDATA=");
    SerialAT.print(totalLen);
    SerialAT.println(",60000"); 
    
    long start = millis();
    bool readyToUpload = false;
    String responseBuffer = "";
    
    SerialMon.print("Waiting for modem prompt: ");
    while(millis() - start < 10000) {
        while(SerialAT.available()) {
            char c = SerialAT.read();
            responseBuffer += c;
            SerialMon.print(c); 
            if(responseBuffer.indexOf("DOWNLOAD") != -1) {
                readyToUpload = true;
                break;
            }
        }
        if(readyToUpload || responseBuffer.indexOf("ERROR") != -1) break;
    }
    SerialMon.println(); 

    if(readyToUpload) {
        SerialMon.println("Modem ready. Pushing binary data...");
        
        SerialAT.print(bodyHead);
        
        uint8_t *buf = fb->buf;
        size_t len = fb->len;
        size_t chunkSize = 1024;
        for (size_t i = 0; i < len; i += chunkSize) {
            size_t toSend = (len - i > chunkSize) ? chunkSize : (len - i);
            SerialAT.write(buf + i, toSend);
            delay(5); 
        }
        
        SerialAT.print(bodyTail);
        
        waitModemResponse(10000, "OK"); 

        SerialMon.println("Data loaded. Executing HTTPS POST...");
        SerialAT.println("AT+HTTPACTION=1"); 
        
        start = millis();
        while(millis() - start < 60000) {
            if(SerialAT.available()) {
                String res = SerialAT.readStringUntil('\n');
                SerialMon.print(res);
                if(res.indexOf("+HTTPACTION: 1,200") != -1) {
                    SerialMon.println("\nSUCCESS: Photo uploaded to Telegram!");
                    break;
                } else if (res.indexOf("+HTTPACTION: 1,") != -1) {
                    SerialMon.println("\nFAILED: Telegram rejected the request. Code: " + res);
                    break;
                }
            }
        }
    } else {
        SerialMon.println("Modem failed to prepare for upload.");
    }

    SerialAT.println("AT+HTTPTERM");
    waitModemResponse(2000);
}

bool waitModemResponse(int timeoutMs, String expectedToken) {
    long start = millis();
    while(millis() - start < timeoutMs) {
        if(SerialAT.available()) {
            String res = SerialAT.readStringUntil('\n');
            res.trim();
            if(res.length() > 0) SerialMon.println("  [Modem] " + res);
            if(res.indexOf(expectedToken) != -1) return true;
        }
    }
    return false;
}

bool manualNtpSync(int *year, int *month, int *day, int *hour, int *min, int *sec) {
    SerialMon.println("Attempting Modem Native NTP Sync (AT+CNTP)...");

    // Server list to try
    const char* ntpServers[] = {"pool.ntp.org", "time.google.com", "time.nist.gov"};
    int numServers = 3;

    for (int i = 0; i < numServers; i++) {
        SerialMon.print("Trying NTP Server: ");
        SerialMon.println(ntpServers[i]);

        // Ensure NITZ doesn't overwrite our time
        SerialAT.println("AT+CTZU=0");
        waitModemResponse(2000, "OK");

        // Ensure correct PDP context for NTP
        SerialAT.println("AT+CNTPCID=1");
        waitModemResponse(2000, "OK");

        // 1. Configure NTP Server (0 offset, we handle TZ locally)
        // Command: AT+CNTP=<server>,<timezone_offset_quarter_hours>
        SerialAT.print("AT+CNTP=\"");
        SerialAT.print(ntpServers[i]);
        SerialAT.println("\",0");
        waitModemResponse(2000, "OK");

        // 2. Start NTP Sync
        // Command: AT+CNTP
        SerialAT.println("AT+CNTP");

        // Wait for URC response: +CNTP: <err>,<ip/str>,<time>
        // Timeout can be up to 60s
        long start = millis();
        bool success = false;
        String response = "";

        while(millis() - start < 60000) {
            if (SerialAT.available()) {
                String line = SerialAT.readStringUntil('\n');
                line.trim();
                if (line.length() > 0) {
                    SerialMon.println("  [NTP] " + line);
                    // Expected: +CNTP: 0,"2024/05/20,12:34:56" (Format may vary slightly by firmware)
                    // Error codes: 1=Network error, 61=DNS error, etc.
                    if (line.startsWith("+CNTP: 0")) { // 0 = Success
                        response = line;
                        success = true;
                        break;
                    } else if (line.startsWith("+CNTP:")) {
                        // Failure code received
                        break;
                    }
                }
            }
        }

        if (success) {
            String timeStr = "";

            // Check if the response contains the time string directly: +CNTP: 0,"yy/MM/dd,HH:mm:ss"
            int firstQuote = response.indexOf('"');
            int lastQuote = response.lastIndexOf('"');

            if (firstQuote != -1 && lastQuote != -1) {
                timeStr = response.substring(firstQuote + 1, lastQuote);
            } else {
                // Some firmware versions only return "+CNTP: 0" on success and update the internal clock silently.
                // In this case, we need to read the time from AT+CCLK?
                SerialMon.println("NTP Success reported (0), but no time string. Checking AT+CCLK...");

                // Wait a bit for the modem to update its internal clock if needed
                delay(2000);

                SerialAT.println("AT+CCLK?");
                long cclkStart = millis();
                while(millis() - cclkStart < 2000) {
                     if (SerialAT.available()) {
                        String line = SerialAT.readStringUntil('\n');
                        line.trim();
                        if (line.startsWith("+CCLK: \"")) {
                            SerialMon.println("  [CCLK Raw] " + line);
                            int q1 = line.indexOf('"');
                            int q2 = line.lastIndexOf('"');
                            if (q1 != -1 && q2 != -1) {
                                timeStr = line.substring(q1 + 1, q2);
                                // CCLK format might include timezone: "yy/MM/dd,HH:mm:ss+zz"
                                // We need to strip the timezone part if present for parsing
                                int plusSign = timeStr.indexOf('+');
                                int minusSign = timeStr.lastIndexOf('-'); // Watch out for date separators vs timezone

                                // Timezone part is usually at the end, e.g. +08 or -05.
                                // The date uses '/', time uses ':'.
                                if (plusSign > 10) {
                                    timeStr = timeStr.substring(0, plusSign);
                                } else if (minusSign > 13) { // Date uses '-', wait A7670 uses '/' usually.
                                     timeStr = timeStr.substring(0, minusSign);
                                }
                                break;
                            }
                        }
                     }
                }
            }

            if (timeStr.length() > 0) {
                // Expected format: "24/05/20,12:34:56" (Year is 2-digit)
                // Or "2024/05/20,12:34:56" (Year is 4-digit) - Check first slash

                int firstSlash = timeStr.indexOf('/');
                if (firstSlash != -1) {
                    String yearStr = timeStr.substring(0, firstSlash);
                    *year = yearStr.toInt();
                    // Fix 2-digit year
                    if (*year < 100) *year += 2000;

                    // Parse the rest manually
                    int secondSlash = timeStr.indexOf('/', firstSlash + 1);
                    int comma = timeStr.indexOf(',');
                    int firstColon = timeStr.indexOf(':');
                    int secondColon = timeStr.indexOf(':', firstColon + 1);

                    if (secondSlash != -1 && comma != -1 && firstColon != -1 && secondColon != -1) {
                        *month = timeStr.substring(firstSlash + 1, secondSlash).toInt();
                        *day = timeStr.substring(secondSlash + 1, comma).toInt();
                        *hour = timeStr.substring(comma + 1, firstColon).toInt();
                        *min = timeStr.substring(firstColon + 1, secondColon).toInt();
                        *sec = timeStr.substring(secondColon + 1).toInt();

                        // Construct UTC time struct
                        struct tm tm_utc;
                        tm_utc.tm_year = *year - 1900;
                        tm_utc.tm_mon = *month - 1;
                        tm_utc.tm_mday = *day;
                        tm_utc.tm_hour = *hour;
                        tm_utc.tm_min = *min;
                        tm_utc.tm_sec = *sec;
                        tm_utc.tm_isdst = 0; // UTC has no DST

                        // FIX: Temporarily switch to UTC to interpret tm_utc correctly
                        String oldTz = getenv("TZ");
                        setenv("TZ", "UTC0", 1);
                        tzset();

                        time_t t_utc = mktime(&tm_utc);

                        // Restore User Timezone
                        setenv("TZ", oldTz.c_str(), 1);
                        tzset();

                        // Set system time (UTC)
                        struct timeval tv;
                        tv.tv_sec = t_utc;
                        tv.tv_usec = 0;
                        settimeofday(&tv, NULL);

                        SerialMon.println("Native NTP Sync Successful & System Time Set!");
                        return true;
                    }
                }
            }
            SerialMon.println("Failed to parse NTP/CCLK response string: " + timeStr);
        } else {
            SerialMon.println("NTP Sync failed (Timeout or Error code).");
        }

        delay(2000); // Wait before trying next server
    }

    return false;
}

bool syncTime(int *year, int *month, int *day, int *hour, int *min, int *sec) {
    float timezone = 0;
    // Attempt 1: Get Network Time directly (NITZ) - informative only
    SerialMon.println("Attempting to read network time...");
    modem.getNetworkTime(year, month, day, hour, min, sec, &timezone);

    // Verbose logging of raw network time
    SerialMon.printf("Network time raw (modem): %04d-%02d-%02d %02d:%02d:%02d\n",
                     *year, *month, *day, *hour, *min, *sec);

    // Force NTP sync regardless of NITZ
    if (manualNtpSync(year, month, day, hour, min, sec)) {
        // Double-check the year from NTP is reasonable.
        if (*year < 2024 || *year > 2035) {
            SerialMon.printf("NTP Sync succeeded but returned invalid year: %d\n", *year);
            return false;
        }

        // Note: We don't necessarily need to update the modem's internal clock (AT+CCLK)
        // if we are using the ESP32's system clock now.
        return true;
    }

    SerialMon.println("NTP Sync Failed.");
    return false;
}

uint32_t readBatteryVoltage() {
    uint32_t v = 0;
#ifdef BOARD_BAT_ADC_PIN
    v = analogReadMilliVolts(BOARD_BAT_ADC_PIN) * 2;
    SerialMon.printf("Battery Voltage: %u mV\n", v);
#endif
    return v;
}

void sendTelegramMessage(String text) {
    SerialMon.println("Sending Telegram Message: " + text);

    // Ensure HTTP is clean before starting
    SerialAT.println("AT+HTTPTERM");
    waitModemResponse(2000);
    delay(1000);

    SerialAT.println("AT+HTTPINIT");
    if (!waitModemResponse(2000)) {
        SerialMon.println("HTTPINIT failed. Retrying...");
        delay(1000);
        SerialAT.println("AT+HTTPTERM");
        waitModemResponse(2000);
        delay(1000);
        SerialAT.println("AT+HTTPINIT");
        if (!waitModemResponse(2000)) {
            SerialMon.println("HTTPINIT failed again. Aborting message.");
            return;
        }
    }

    String url = "https://api.telegram.org/bot" + botToken + "/sendMessage";
    SerialAT.println("AT+HTTPPARA=\"URL\",\"" + url + "\"");
    waitModemResponse(2000);

    SerialAT.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
    waitModemResponse(2000);

    // Escape characters for JSON if needed
    String escapedText = text;
    escapedText.replace("\\", "\\\\");
    escapedText.replace("\"", "\\\"");
    escapedText.replace("\n", "\\n");

    String payload = "{\"chat_id\":\"" + chatId + "\", \"text\":\"" + escapedText + "\"}";

    SerialAT.print("AT+HTTPDATA=");
    SerialAT.print(payload.length());
    SerialAT.println(",10000");

    long start = millis();
    bool readyToSend = false;
    while (millis() - start < 10000) {
        if (SerialAT.available()) {
            String line = SerialAT.readStringUntil('\n');
            line.trim();
            if (line.indexOf("DOWNLOAD") != -1) {
                readyToSend = true;
                break;
            }
        }
    }

    if (readyToSend) {
        SerialAT.print(payload);
        waitModemResponse(10000, "OK");

        SerialAT.println("AT+HTTPACTION=1");

        start = millis();
        while (millis() - start < 20000) {
            if (SerialAT.available()) {
                String res = SerialAT.readStringUntil('\n');
                SerialMon.print(res);
                if (res.indexOf("+HTTPACTION: 1,200") != -1) {
                    SerialMon.println("\nMessage sent successfully.");
                    break;
                } else if (res.indexOf("+HTTPACTION: 1,") != -1) {
                    SerialMon.println("\nMessage send failed: " + res);
                    break;
                }
            }
        }
    } else {
        SerialMon.println("Failed to get DOWNLOAD prompt for message.");
    }

    SerialAT.println("AT+HTTPTERM");
    waitModemResponse(2000);
}
