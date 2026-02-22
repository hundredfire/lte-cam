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

// Timezone handling using POSIX standard (Paris: CET/CEST)
// See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for more
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";
// ==========================================

#define SerialMon Serial
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);

int calculateSleepSecondsFromSchedules(int currentHour, int currentMin, int currentSec);
void sendPhotoViaBuiltInHTTP(camera_fb_t * fb);
void waitModemResponse(int timeoutMs, String expectedToken = "OK");
bool setCameraPower(bool enable);
bool syncTime(int *year, int *month, int *day, int *hour, int *min, int *sec); // Removed timezone float
bool manualNtpSync(int *year, int *month, int *day, int *hour, int *min, int *sec);
bool checkInternet();

void setup() {
    // === MOVED VARIABLES TO THE TOP TO FIX GOTO SCOPE ERROR ===
    camera_fb_t * fb = nullptr; 
    int year = 0, month = 0, day = 0, hour = -1, min = 0, sec = 0;
    bool gotIP = false; 

    SerialMon.begin(115200);
    delay(1000);

    // Set Timezone rules
    setenv("TZ", TZ_INFO, 1);
    tzset();

#ifdef MODEM_RESET_PIN
    // Release reset GPIO hold if it was held during sleep
    gpio_hold_dis((gpio_num_t)MODEM_RESET_PIN);
#endif
    
    SerialMon.println("\n--- Telegram LTE Camera Starting [BUILT-IN HTTP MODE] ---");
    SerialMon.println("Firmware Version: TimeSync-Fix-v9");

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
    while (!syncTime(&year, &month, &day, &hour, &min, &sec)) {
        SerialMon.println("Failed to sync time! Retrying in 30 seconds... (Loop active)");

        // Only ensure Network is attached (CS/PS domain)
        if (!modem.isNetworkConnected()) {
            SerialMon.println("Network disconnected. Waiting for network...");
            if (!modem.waitForNetwork(60000L)) {
                 SerialMon.println("Network connection failed.");
                 continue;
            }
        }

        // Ensure GPRS context is active IF it reports as disconnected.
        if (!modem.isGprsConnected()) {
             SerialMon.println("GPRS disconnected. Attempting to connect...");
             if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
                 SerialMon.println("GPRS connection failed.");
                 continue;
             }
        }

        // Check Internet Connectivity - Informational
        if (!checkInternet()) {
            SerialMon.println("Internet check failed. Waiting for connectivity to stabilize...");
        }

        delay(30000);
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
    }

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

bool manualNtpSync(int *year, int *month, int *day, int *hour, int *min, int *sec) {
    SerialMon.println("Attempting Modem Native NTP Sync (AT+CNTP)...");

    // Server list to try
    const char* ntpServers[] = {"pool.ntp.org", "time.google.com", "time.nist.gov"};
    int numServers = 3;

    for (int i = 0; i < numServers; i++) {
        SerialMon.print("Trying NTP Server: ");
        SerialMon.println(ntpServers[i]);

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
            // Parse the time string: +CNTP: 0,"yy/MM/dd,HH:mm:ss"
            int firstQuote = response.indexOf('"');
            int lastQuote = response.lastIndexOf('"');
            if (firstQuote != -1 && lastQuote != -1) {
                String timeStr = response.substring(firstQuote + 1, lastQuote);
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

                        time_t t_utc = mktime(&tm_utc);

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
            SerialMon.println("Failed to parse NTP response string.");
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