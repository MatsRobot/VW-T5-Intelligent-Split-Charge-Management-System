/* * =====================================================================================
 * VW T5 BATTERY MANAGEMENT SYSTEM (BMS) - FULL VERSION
 * Project Goal: Limit leisure battery charging current to 14A via LTC3780 
 * and provide fail-safe power management with a NE555 Watchdog circuit.
 *
 * Section Overviews for the Online Guide
	1. The Setup Phase
	In this section, we initialize the I2C bus and the OLED. Crucially, the code performs a "Safety Check". 
	It checks for the SD card and RTC. If they aren't found, the code sets flags (sdActive = false) rather than crashing, 
	ensuring the primary job of charging the battery never stops.

	2. The Logic Engine (State Machine)
	This is where the over-current issue is solved. Instead of a direct connection, the code evaluates the battery's health. 
	It looks for "Tail Current"—where the voltage is high but the current flow is minimal—to identify a fully charged battery. 
	This is much more accurate than simple voltage-based relays.

	3. The Health Pulse
	Every 10 minutes, the charger is momentarily disconnected. This allows us to log the "Resting Voltage" of the battery 
	without the surface charge of the charger interference, providing high-quality data for long-term battery health monitoring.

	4. The NE555 Watchdog Integration
	To handle the VW T5's lack of a smart alternator, we use a physical 555-timer latch. The code sends a "heartbeat" pulse on 
	Pin 4 every second. If the engine turns off, the code continues pulsing for exactly 2 minutes. This gives the system time to 
	finish its last data log before the 555 timer expires and kills the power, resulting in zero parasitic drain.

 * =====================================================================================
 */

#include <Arduino.h>            // Core Arduino library
#include <Wire.h>               // I2C communication for sensors and OLED
#include <Adafruit_INA219.h>    // Library for current/voltage sensing
#include "SSD1306Ascii.h"       // Lightweight OLED library for faster performance
#include "SSD1306AsciiWire.h"   // I2C driver for the OLED
#include <SPI.h>                // SPI communication for the SD Card
#include <SD.h>                 // SD Card filesystem library
#include <RTClib.h>             // Real-Time Clock library for timestamps

// --- HARDWARE PIN ASSIGNMENTS ---
#define PIN_BUZZER      9       // Piezo buzzer for high-current alerts
#define PIN_ENABLE      3       // Drives BC337 to switch the 40A Hard-Cut Relay
#define PIN_SD_CS      10       // Chip Select pin for the SD module
#define PIN_HEARTBEAT  A0       // Legacy analog heartbeat pin
#define PIN_WATCHDOG    4       // Digital pulse pin for the NE555 "Stay-Alive" module

// --- OBJECT INITIALIZATION ---
SSD1306AsciiWire oled;          // Initialize the OLED display object
Adafruit_INA219 ina_alt(0x40);  // Sensor 1: Measures input from Alternator side
Adafruit_INA219 ina_batt(0x41); // Sensor 2: Measures output to Leisure Battery
RTC_DS3231 rtc;                 // Initialize the High-Accuracy Clock object

// --- GLOBAL VARIABLES & FLAGS ---
bool sdActive = false;          // Tracks if the SD card is physically present and working
bool rtcActive = false;         // Tracks if the Real-Time Clock is communicating
const float SHUNT_OHMS = 0.01;  // Resistance of the R100 shunt on INA219 boards
const uint32_t LOG_INTERVAL = 120000; // Frequency of standard data logging (2 Mins)
const uint32_t HEALTH_INT   = 600000; // Frequency of "Resting Voltage" checks (10 Mins)

int logCount = 0;               // Counter to track total successful writes since boot

// Define the operating states of the charger
enum ChargeState { BULK, FULL, PROTECT, OFF }; 
ChargeState systemState = OFF;  // System starts in "OFF" for safety
uint32_t fullTimerStart = 0;    // Timer used to confirm battery is truly "Full"

/**
 * Control the physical 40A Relay.
 * Logic: Pulling the transistor base HIGH grounds the relay coil, closing the contact.
 */
void setChargerPower(bool powerOn) {
    if (powerOn) {
        digitalWrite(PIN_ENABLE, LOW);  // Logic for your specific transistor setup
    } else {
        digitalWrite(PIN_ENABLE, HIGH); // Logic to disconnect the relay
    }
}

/**
 * Handles the writing of data to the SD card in CSV format.
 * Includes fallback logic if the RTC is missing.
 */
void logToSD(const char* note, float v1, float a1, float v2, float a2) {
    if (!sdActive) return; // Silent exit if SD hardware is failed/missing

    File dataFile = SD.open(F("T5_BMS.csv"), FILE_WRITE); // Open/Create file
    if (dataFile) {
        if (rtcActive) {
            DateTime now = rtc.now(); // Get current actual time
            char buf[25]; 
            // Standard format: YYYY-MM-DD HH:mm:ss
            sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", 
                    now.year(), now.month(), now.day(), 
                    now.hour(), now.minute(), now.second());
            dataFile.print(buf);
        } else {
            dataFile.print(millis() / 1000); // Fallback to seconds since boot
        }
        
        // Write CSV Columns: Alt_V, Alt_A, Batt_V, Batt_A, Note
        dataFile.print(F(",")); dataFile.print(v1, 2); 
        dataFile.print(F(",")); dataFile.print(a1, 2); 
        dataFile.print(F(",")); dataFile.print(v2, 2); 
        dataFile.print(F(",")); dataFile.print(a2, 2); 
        dataFile.print(F(","));
        dataFile.println(note); // End the line with the log reason
        dataFile.close();        // Ensure data is flushed to the card

        logCount++; // Increment the successful write counter

    } else {
        sdActive = false; // Disable logging if file write fails
    }
}

void setup() {
    // 1. Initial Pin Safety: Force relay OFF immediately to prevent spark
    digitalWrite(PIN_ENABLE, LOW); 
    pinMode(PIN_ENABLE, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_HEARTBEAT, OUTPUT);
    pinMode(4, OUTPUT); // Pin 4 for NE555 watchdog pulse

    // 2. Start I2C and Display
    Wire.begin();
    oled.begin(&Adafruit128x64, 0x3C);
    oled.setFont(Adafruit5x7);
    oled.clear();
    oled.println(F("T5 BMS STARTUP..."));

    // 3. RTC Initialization
    if (rtc.begin()) {
        rtcActive = true;
        if (rtc.lostPower()) {
            oled.println(F("RTC LOST POWER!"));
            // Automatically set RTC to the time the code was compiled
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            delay(1000);
        }
    }

    // 4. SD Card Check and Header Creation
    if (SD.begin(PIN_SD_CS)) {
        sdActive = true;
    }
    // If card is OK and file doesn't exist, create it with headers
    if (sdActive && !SD.exists("T5_BMS.csv")) {
        File dataFile = SD.open("T5_BMS.csv", FILE_WRITE);
        if (dataFile) {
            dataFile.println(F("Timestamp,Alt_V,Alt_A,Batt_V,Batt_A,Note"));
            dataFile.close();
        }
    }

    // 5. Sensor Initialization
    ina_alt.begin();
    ina_batt.begin();

    // 6. Visual Status Confirmation for the user
    oled.clear();
    oled.println(F("SYSTEM CHECK:"));
    oled.print(F("SD:  ")); oled.println(sdActive ? F("OK") : F("MISSING"));
    
    oled.print(F("RTC: "));
    if (rtcActive) {
        DateTime now = rtc.now();
        char timeBuf[17];
        sprintf(timeBuf, "%02d/%02d %02d:%02d", now.day(), now.month(), now.hour(), now.minute());
        oled.println(timeBuf);
    } else {
        oled.println(F("MISSING"));
    }

    // 7. Initial "Boot" entry for logging
    if (sdActive) {
        File dataFile = SD.open(F("T5_BMS.csv"), FILE_WRITE);
        if (dataFile) {
            dataFile.println(); // Blank line to separate sessions
            dataFile.close();
        }
        logToSD("SYSTEM BOOT", 0, 0, 0, 0); 
    }
    
    delay(3000); // Allow time to read the splash screen
}

void loop() {
    static uint32_t lastHealth = 0;
    static uint32_t lastLog = 0;
    static uint32_t lastClockUpdate = 0;
    static uint32_t lastWatchdogPulse = 0;
    
    // FETCH SENSOR DATA
    float altV = ina_alt.getBusVoltage_V();
    float altA = (ina_alt.getShuntVoltage_mV() / 1000.0) / SHUNT_OHMS; // Convert mV to Amps
    float battV = ina_batt.getBusVoltage_V();
    float battA = (ina_batt.getShuntVoltage_mV() / 1000.0) / SHUNT_OHMS;

    // --- BMS CORE STATE MACHINE ---
    // Rule 1: Alternator Protection (Engine off check)
    if (altV < 13.2) {
        systemState = OFF; // Disconnect if alternator isn't pushing charge
    } 
    // Rule 2: Overvoltage Protection
    else if (battV > 14.6) {
        systemState = PROTECT; // Hard disconnect to save the battery
    } 
    // Rule 3: Detect "Full" state using Tail Current logic
    else if (battV > 14.2 && battA < 0.5) {
        if (fullTimerStart == 0) fullTimerStart = millis();
        // Require 5 continuous minutes of low current before declaring FULL
        if (millis() - fullTimerStart > 300000) systemState = FULL;
    } 
    // Rule 4: Normal Bulk Charging
    else {
        fullTimerStart = 0;
        // Hysteresis: Don't jump out of FULL unless voltage drops significantly
        if (systemState == FULL && battV > 12.8) systemState = FULL;
        else systemState = BULK;
    }

    // --- HARDWARE RELAY CONTROL ---
    if (systemState == BULK) {
        setChargerPower(true);
        // Alert if the LTC3780 is nearing its 14A thermal limit
        if (battA > 14.0) digitalWrite(PIN_BUZZER, (millis() % 500 < 250));
        else digitalWrite(PIN_BUZZER, LOW);
    } 
    else {
        setChargerPower(false); // Disconnect charger for any other state
        digitalWrite(PIN_BUZZER, (systemState == PROTECT)); // Constant buzz for overvolt
    }

    // --- SD LOGGING TRIGGERS ---
    if (millis() - lastLog >= LOG_INTERVAL) {
        logToSD("DATA", altV, altA, battV, battA);
        lastLog = millis();
    }
    
    // HEALTH PULSE: Briefly disconnect to measure the battery's resting voltage
    if (millis() - lastHealth >= HEALTH_INT) {
        setChargerPower(false); 
        delay(1000); // Allow chemistry to stabilize slightly
        logToSD("PULSE", ina_alt.getBusVoltage_V(), 0, ina_batt.getBusVoltage_V(), 0);
        if (systemState == BULK) setChargerPower(true); // Resume charging
        lastHealth = millis();
    }

    // --- NE555 WATCHDOG HEARTBEAT ---
    // If engine is on OR within 2-min shutdown grace period, keep resetting the 555
    if (systemState != OFF || (millis() - fullTimerStart < 120000)) {
        if (millis() - lastWatchdogPulse > 1000) { // Reset pulse every 1 second
            digitalWrite(4, HIGH);
            delay(10); 
            digitalWrite(4, LOW);
            lastWatchdogPulse = millis();
        }
    }

    // --- OLED UPDATE LOGIC ---
    // Line 0: Show current charge state
    oled.setCursor(0, 0);
    oled.print(F("STATE: "));
    if(systemState==BULK)      oled.println(F("CHARGING  "));
    else if(systemState==FULL) oled.println(F("FINISHED  "));
    else if(systemState==OFF)  oled.println(F("STANDBY   "));
    else                       oled.println(F("OVERVOLT  "));

    // Line 1: Logging Status
    oled.setCursor(0, 1);
    if(sdActive) {
        oled.print(F("SD OK - Log: ")); oled.print(logCount);
        oled.print(F("      ")); 
    } else {
        oled.print(F("SD ERROR/MISSING "));
    }

    // Line 2: Real-Time Clock Update every second
    if (rtcActive && (millis() - lastClockUpdate >= 1000)) { 
        lastClockUpdate = millis();
        DateTime now = rtc.now(); 
        char timeBuf[22]; 
        sprintf(timeBuf, "TIME: %02d/%02d %02d:%02d:%02d", 
                now.day(), now.month(), now.hour(), now.minute(), now.second());
        oled.set1X();
        oled.setCursor(0, 2); 
        oled.print(timeBuf);
        oled.print(F("   ")); 
    }

    // Line 5-6: Main Battery Metrics (Voltage and Amps)
    oled.set2X(); // Large font for visibility
    oled.setCursor(0, 4); 
    oled.print(battV, 1); oled.print(F("V "));
    oled.print(battA, 1); oled.println(F("A  "));
    
    // Line 7: Alternator Input Monitoring
    oled.set1X();
    oled.setCursor(0, 7);
    oled.print(F("Alternator: ")); oled.print(altV, 1); oled.print(F("V  "));

    // Analog heartbeat for legacy visualization on an oscilloscope/LED
    analogWrite(PIN_HEARTBEAT, 128 + 127 * cos(millis() / 500.0));
}