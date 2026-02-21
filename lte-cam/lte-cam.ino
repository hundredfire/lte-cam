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

// Load our private credentials
#include "secrets.h"

// ==========================================
//               GENERAL SETTINGS
// ==========================================
const bool DEBUG_MODE = false;           
const int  DEBUG_SLEEP_SECONDS = 120;   
// Schedule times in HH:mm format
const char* schedules[] = {"10:00", "15:00"};

// Timezone offset in hours (e.g., 2.0 for GMT+2, -5.0 for GMT-5)
// Default to 1.0 (e.g., Paris/CET)
const float TIMEZONE_OFFSET = 1.0;
// ==========================================

#define SerialMon Serial
TinyGsm modem(SerialAT);

int calculateSleepSecondsFromSchedules(int currentHour, int currentMin, int currentSec);
void sendPhotoViaBuiltInHTTP(camera_fb_t * fb);
void waitModemResponse(int timeoutMs, String expectedToken = "OK");
bool setCameraPower(bool enable);
bool syncTime(int *year, int *month, int *day, int *hour, int *min, int *sec, float *timezone);
bool manualNtpSync(int *year, int *month, int *day, int *hour, int *min, int *sec, float *timezone);

void setup() {
    // === MOVED VARIABLES TO THE TOP TO FIX GOTO SCOPE ERROR ===
    camera_fb_t * fb = nullptr; 
    int year = 0, month = 0, day = 0, hour = -1, min = 0, sec = 0;
    float timezone = 0;
    bool gotIP = false; 

    SerialMon.begin(115200);
    delay(1000);

#ifdef MODEM_RESET_PIN
    // Release reset GPIO hold if it was held during sleep
    gpio_hold_dis((gpio_num_t)MODEM_RESET_PIN);
#endif
    
    SerialMon.println("\n--- Telegram LTE Camera Starting [BUILT-IN HTTP MODE] ---");

#ifdef BOARD_POWERON_PIN
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
    delay(500);
#endif

#ifdef BOARD_POWER_SAVE_MODE_PIN
    pinMode(BOARD_POWER_SAVE_MODE_PIN, OUTPUT);
    digitalWrite(BOARD_POWER_SAVE_MODE_PIN, HIGH);
#endif

    SerialMon.println("Powering on Camera PMIC...");
    if (!setCameraPower(true)) {
        SerialMon.println("The camera PMIC failed to start! Going to sleep.");
        goto sleep_routine;
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
        goto sleep_routine;
    }

#ifdef MODEM_RESET_PIN
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
#endif
#ifdef MODEM_FLIGHT_PIN
    pinMode(MODEM_FLIGHT_PIN, OUTPUT);
    digitalWrite(MODEM_FLIGHT_PIN, HIGH);
#endif
#ifdef MODEM_DTR_PIN
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, LOW);
#endif

    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(1000); 
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    SerialMon.println("Initializing modem...");
    
    if (!modem.init()) { 
        SerialMon.println("Modem init failed!");
        goto sleep_routine;
    }

    if (String(GSM_PIN) != "") {
        if (modem.getSimStatus() != 3) modem.simUnlock(GSM_PIN);
    }

    modem.setNetworkMode(MODEM_NETWORK_AUTO);

    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork(60000L)) goto sleep_routine;
    SerialMon.println(" OK");

    // === UPDATED SMART APN LOGIC ===
    SerialMon.print("Connecting to APN...");
    modem.gprsConnect(apn, gprsUser, gprsPass); 
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
        SerialMon.println("Network failure: Could not get IP. Going to sleep.");
        goto sleep_routine;
    }

    // Loop until we get a valid time
    while (!syncTime(&year, &month, &day, &hour, &min, &sec, &timezone)) {
        SerialMon.println("Failed to sync time! Retrying in 5 seconds...");
        delay(5000);

        // Check network status and reconnect if needed
        if (!modem.isNetworkConnected()) {
            SerialMon.println("Network disconnected. Reconnecting...");
            if (!modem.waitForNetwork(60000L)) {
                 SerialMon.println("Network connection failed.");
                 continue;
            }
        }

        // Check GPRS status (bearer)
        if (!modem.isGprsConnected()) {
             SerialMon.println("GPRS disconnected. Reconnecting...");
             if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
                 SerialMon.println("GPRS connection failed.");
                 continue;
             }
        }
    }

    SerialMon.printf("Current Time: %04d-%02d-%02d %02d:%02d:%02d (Timezone: %.1f)\n",
                     year, month, day, hour, min, sec, timezone);

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
    }

sleep_routine:
    SerialMon.println("Enter modem power off!");
    if (modem.poweroff()) {
        SerialMon.println("Modem power off command sent!");
    } else {
        SerialMon.println("Modem power off command failed!");
    }

    // Wait for modem to actually shutdown
    delay(5000);

    SerialMon.println("Check modem response...");
    while (modem.testAT()) {
        SerialMon.print(".");
        delay(500);
    }
    SerialMon.println("\nModem is not responding, power off confirmed!");

    setCameraPower(false); 
    Wire.end();            

#ifdef BOARD_POWERON_PIN
    // Turn on DC boost to power off the modem
    digitalWrite(BOARD_POWERON_PIN, LOW); 
#endif

#ifdef MODEM_RESET_PIN
    // Keep it low during the sleep period.
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    gpio_hold_en((gpio_num_t)MODEM_RESET_PIN);
    gpio_deep_sleep_hold_en();
#endif

    int sleepSeconds = (DEBUG_MODE) ? DEBUG_SLEEP_SECONDS : ((hour != -1) ? calculateSleepSecondsFromSchedules(hour, min, sec) : 3600);
    SerialMon.printf("Going to deep sleep for %d seconds...\n", sleepSeconds);
    esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);
    delay(200);
    esp_deep_sleep_start();
    SerialMon.println("This will never be printed");
}

void loop() {}

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

void waitModemResponse(int timeoutMs, String expectedToken) {
    long start = millis();
    while(millis() - start < timeoutMs) {
        if(SerialAT.available()) {
            String res = SerialAT.readStringUntil('\n');
            res.trim();
            if(res.length() > 0) SerialMon.println("  [Modem] " + res);
            if(res.indexOf(expectedToken) != -1) break;
        }
    }
}

bool manualNtpSync(int *year, int *month, int *day, int *hour, int *min, int *sec, float *timezone) {
    SerialMon.println("Attempting Manual UDP NTP Sync...");

    // Server list to try
    const char* ntpServers[] = {"pool.ntp.org", "time.google.com", "time.nist.gov"};
    int numServers = 3;

    // Ensure manual receive mode
    SerialAT.println("AT+CIPRXGET=1");
    waitModemResponse(1000, "OK");

    for (int i = 0; i < numServers; i++) {
        SerialMon.print("Connecting to UDP: ");
        SerialMon.println(ntpServers[i]);

        // Close any previous socket 0 just in case
        SerialAT.println("AT+CIPCLOSE=0");
        waitModemResponse(500);

        SerialAT.print("AT+CIPOPEN=0,\"UDP\",\"");
        SerialAT.print(ntpServers[i]);
        SerialAT.println("\",123");

        bool connected = false;
        long start = millis();
        while(millis() - start < 10000) {
            if (SerialAT.available()) {
                String line = SerialAT.readStringUntil('\n');
                line.trim();
                if (line.length() > 0) SerialMon.println("  [UDP] " + line);

                if (line.indexOf("+CIPOPEN: 0,0") != -1) {
                    connected = true;
                    break;
                }
                if (line.indexOf("ERROR") != -1) break;
            }
        }

        if (!connected) {
            SerialMon.println("UDP Connection failed. Trying next server...");
            continue;
        }

        // Send NTP Request
        byte ntpPacket[48];
        memset(ntpPacket, 0, 48);
        ntpPacket[0] = 0x1B; // LI=0, VN=3, Mode=3 (Client)

        SerialAT.print("AT+CIPSEND=0,48");
        SerialAT.println();

        // Wait for prompt >
        start = millis();
        bool prompt = false;
        while(millis() - start < 3000) {
            if (SerialAT.available()) {
                char c = SerialAT.read();
                if (c == '>') {
                    prompt = true;
                    break;
                }
            }
        }

        if (prompt) {
            SerialAT.write(ntpPacket, 48);

            // Wait for data
            start = millis();
            bool dataReceived = false;
            while(millis() - start < 5000) {
                 if (SerialAT.available()) {
                    String line = SerialAT.readStringUntil('\n');
                    line.trim();
                    if (line.length() > 0) SerialMon.println("  [UDP] " + line);

                    if (line.indexOf("+CIPRXGET: 1") != -1) {
                        dataReceived = true;
                        break;
                    }
                 }
            }

            if (dataReceived) {
                SerialAT.println("AT+CIPRXGET=2,0,48");

                // We need to parse this manually and robustly.
                // Read until we find the header line: +CIPRXGET: 2,0,48,0
                start = millis();
                bool headerFound = false;
                while(millis() - start < 2000) {
                    if (SerialAT.available()) {
                        String line = SerialAT.readStringUntil('\n');
                        if (line.indexOf("+CIPRXGET: 2,") != -1) {
                            headerFound = true;
                            break;
                        }
                    }
                }

                if (headerFound) {
                    // Now read 48 bytes
                    byte buffer[48];
                    int bytesRead = 0;
                    start = millis();
                    while (bytesRead < 48 && millis() - start < 3000) {
                        if (SerialAT.available()) {
                            buffer[bytesRead++] = SerialAT.read();
                        }
                    }

                    if (bytesRead == 48) {
                        // Success!
                        SerialAT.println("AT+CIPCLOSE=0");
                        waitModemResponse(1000);

                        // Parse timestamp
                        unsigned long highWord = (unsigned long)buffer[40] << 8 | buffer[41];
                        unsigned long lowWord = (unsigned long)buffer[42] << 8 | buffer[43];
                        unsigned long secsSince1900 = highWord << 16 | lowWord;
                        unsigned long seventyYears = 2208988800UL;
                        unsigned long epoch = secsSince1900 - seventyYears;

                        // Apply timezone
                        epoch += (long)(TIMEZONE_OFFSET * 3600);

                        struct tm *ptm = gmtime((time_t*)&epoch);

                        *year = ptm->tm_year + 1900;
                        *month = ptm->tm_mon + 1;
                        *day = ptm->tm_mday;
                        *hour = ptm->tm_hour;
                        *min = ptm->tm_min;
                        *sec = ptm->tm_sec;
                        *timezone = TIMEZONE_OFFSET;

                        SerialMon.println("NTP Sync Successful!");
                        return true;
                    } else {
                        SerialMon.println("Failed to read full NTP packet.");
                    }
                } else {
                     SerialMon.println("Failed to find data header.");
                }
            } else {
                 SerialMon.println("No data received.");
            }
        } else {
             SerialMon.println("No Prompt received.");
        }

        SerialAT.println("AT+CIPCLOSE=0");
        waitModemResponse(1000);
    }

    return false;
}

bool syncTime(int *year, int *month, int *day, int *hour, int *min, int *sec, float *timezone) {
    // Attempt 1: Get Network Time directly (NITZ)
    SerialMon.println("Attempting to read network time...");
    modem.getNetworkTime(year, month, day, hour, min, sec, timezone);

    // Verbose logging of raw network time
    SerialMon.printf("Network time raw: %04d-%02d-%02d %02d:%02d:%02d (Timezone: %.1f)\n",
                     *year, *month, *day, *hour, *min, *sec, *timezone);

    // Strict validation: year must be between 2024 and 2035 to be considered valid
    if (*year >= 2024 && *year <= 2035) {
        SerialMon.println("Time valid (NITZ/Saved).");
        return true;
    }

    SerialMon.printf("Time invalid or not set (Year: %d). Attempting NTP sync...\n", *year);

    if (manualNtpSync(year, month, day, hour, min, sec, timezone)) {
        // Double-check the year from NTP is reasonable.
        if (*year < 2024 || *year > 2035) {
            SerialMon.printf("NTP Sync succeeded but returned invalid year: %d\n", *year);
            return false;
        }

        // Update Modem Internal Clock
        char buffer[50];
        int tz_quarters = (int)(*timezone * 4);
        sprintf(buffer, "AT+CCLK=\"%02d/%02d/%02d,%02d:%02d:%02d%+03d\"",
                *year % 100, *month, *day, *hour, *min, *sec, tz_quarters);
        SerialAT.println(buffer);
        // Wait for OK, but we don't strictly care if it fails
        long start = millis();
        while(millis() - start < 1000) {
             if (SerialAT.available()) {
                String line = SerialAT.readStringUntil('\n');
                if (line.indexOf("OK") != -1) break;
             }
        }
        return true;
    }

    SerialMon.println("NTP Sync Failed.");
    return false;
}

int calculateSleepSecondsFromSchedules(int currentHour, int currentMin, int currentSec) {
    int currentSecondsOfDay = (currentHour * 3600) + (currentMin * 60) + currentSec;
    int minDiff = 24 * 3600 + 1; // Start with a value larger than a day
    int earliestSchedule = 24 * 3600 + 1;
    int numSchedules = sizeof(schedules) / sizeof(schedules[0]);

    for (int i = 0; i < numSchedules; i++) {
        String timeStr = String(schedules[i]);
        int h = timeStr.substring(0, timeStr.indexOf(':')).toInt();
        int m = timeStr.substring(timeStr.indexOf(':') + 1).toInt();
        int scheduleSeconds = h * 3600 + m * 60;

        if (scheduleSeconds < earliestSchedule) {
            earliestSchedule = scheduleSeconds;
        }

        if (scheduleSeconds > currentSecondsOfDay) {
            int diff = scheduleSeconds - currentSecondsOfDay;
            if (diff < minDiff) {
                minDiff = diff;
            }
        }
    }

    if (minDiff > 24 * 3600) {
        // No future schedule today, wrap to the earliest schedule tomorrow
        return (24 * 3600 - currentSecondsOfDay) + earliestSchedule;
    }

    return minDiff;
}