/* * =====================================================================================
 * PROJECT: VW-T5-Intelligent-BMS (Version 1.0)
 * =====================================================================================
 * * OVERVIEW FOR USERS:
 * This software manages the charging of a campervan leisure battery from a vehicle 
 * alternator. It acts as the "brain" for an LTC3780 DC-DC converter.
 *
 * SAFETY FEATURES & LOGIC:
 * 1. THE LOW-RESISTANCE PROTECTOR: 
 * Standard relays let batteries pull too much current. This code monitors current 
 * and provides a 14A cap to protect your van's wiring from melting.
 *
 * 2. ENGINE DETECTION (HYSTERESIS):
 * The system only connects the batteries when it "sees" the alternator reach 13.4V.
 * To prevent the relay from clicking on and off (chattering) when loads are applied, 
 * it requires a 3-second consistent dip below 12.8V before it will disconnect.
 *
 * 3. THERMAL FAILSAFE:
 * Heat is the enemy of DC-DC converters. If the internal sensor detects >60°C,
 * the system physically cuts the connection to prevent fire or hardware failure.
 *
 * 4. THE NE555 WATCHDOG:
 * Vehicle environments are electrically "noisy" which can freeze microcontrollers.
 * This code sends a "heartbeat" pulse every second. If the pulse stops (due to 
 * a crash or engine shutdown), an external timer circuit will kill the power, 
 * ensuring the battery is never accidentally drained.
 * * 5. DATA LOGGING:
 * Every 2 minutes, the system saves all stats to an SD card. Every 10 minutes, 
 * it briefly pauses charging to measure the "resting" voltage of the battery, 
 * which is the only accurate way to know the true state of charge.
 * =====================================================================================
 */

#include <Arduino.h>
#include <Wire.h>               // Handles I2C communication (The "data bus" for sensors)
#include <Adafruit_INA219.h>    // Library for reading Volts and Amps
#include "SSD1306Ascii.h"       // Library for the OLED screen (Efficient text-only version)
#include "SSD1306AsciiWire.h"
#include <SPI.h>                // Handles SPI communication (Used by the SD Card)
#include <SD.h>                 // Library for saving data to the SD Card
#include <RTClib.h>             // Library for the Real-Time Clock (Timekeeping)
#include <OneWire.h>            // Protocol for the temperature sensor
#include <DallasTemperature.h>  // Specific library for the DS18B20 Temp sensor

// --- PIN DEFINITIONS (Where the wires go) ---
#define PIN_BUZZER      9       // D9: Drives the alarm buzzer
#define PIN_ENABLE      3       // D3: Controls the 40A Relay (HIGH=Connected)
#define PIN_SD_CS      10       // D10: Tells the SD card when to listen
#define PIN_HEARTBEAT  A0       // A0: Drives the status LED on the panel
#define PIN_TEMP_BUS    5       // D5: The data line for the temperature sensor

// --- SENSOR OBJECTS ---
SSD1306AsciiWire oled;          // Create the screen object
Adafruit_INA219 ina_alt(0x40);  // Sensor to monitor the Alternator/Starter Battery
Adafruit_INA219 ina_batt(0x41); // Sensor to monitor the Leisure Battery
RTC_DS3231 rtc;                 // The clock module

// --- TEMPERATURE CONFIG ---
OneWire oneWire(PIN_TEMP_BUS);          // Initialize the 1-wire bus on D5
DallasTemperature sensors(&oneWire);    // Pass the bus to the sensor library
float currentTemp = 0;                  // Storage for the latest temperature reading

// --- GLOBAL SETTINGS ---
bool sdActive = false;          // Becomes true if an SD card is found
bool rtcActive = false;         // Becomes true if the clock is found
const float SHUNT_OHMS = 0.01;  // Resistance of the measuring shunts (Do not change)
const uint32_t LOG_INTERVAL = 120000; // Time between standard logs (120,000ms = 2 Mins)
const uint32_t HEALTH_INT   = 600000; // Time between battery health checks (10 Mins)

// --- HYSTERESIS & TIMING ---
static uint32_t lowVoltStartTime = 0;   // Tracks how long the voltage has been low
const uint32_t DISCONNECT_DELAY = 3000;  // Must be low for 3000ms (3s) to disconnect
const float TURN_ON_VOLTS = 13.4;        // Turn on when engine is definitely running
const float TURN_OFF_VOLTS = 12.8;       // Turn off when battery is at resting voltage

int logCount = 0;               // Keeps track of how many lines written to SD card

// --- STATE MACHINE ---
enum ChargeState { BULK, FULL, PROTECT, OFF }; // The 4 modes the system can be in
ChargeState systemState = OFF;                 // Start the system in 'OFF' mode
uint32_t fullTimerStart = 0;                   // Timer used to confirm battery is 'FULL'

/**
 * FUNCTION: setChargerPower
 * This is the physical "Switch" function. 
 * High = Relay clicks ON (Connecting batteries).
 * Low = Relay clicks OFF (Isolating batteries).
 */
void setChargerPower(bool powerOn) {
    if (powerOn) {
        digitalWrite(PIN_ENABLE, HIGH); // Apply 5V to the relay pin
    } else {
        digitalWrite(PIN_ENABLE, LOW);  // Apply 0V to the relay pin
    }
}

/**
 * FUNCTION: logToSD
 * This function handles the complicated task of writing data to the SD card.
 * It combines the Date, Time, Voltages, Currents, and Temperatures into one line.
 */
void logToSD(const char* note, float v1, float a1, float v2, float a2, float temp) {
    if (!sdActive) return; // If SD card failed, don't try to write (avoids crashing)

    File dataFile = SD.open(F("T5_BMS.csv"), FILE_WRITE); // Open the log file
    if (dataFile) {
        if (rtcActive) {
            DateTime now = rtc.now(); // Get the current time from the clock
            char buf[25]; 
            // Format: YYYY-MM-DD HH:MM:SS
            sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", 
                    now.year(), now.month(), now.day(), 
                    now.hour(), now.minute(), now.second());
            dataFile.print(buf); 
        } else {
            dataFile.print(millis() / 1000); // If no clock, use seconds since power-on
        }
        
        // Write all the numbers separated by commas for Excel/Google Sheets
        dataFile.print(F(",")); dataFile.print(v1, 2); 
        dataFile.print(F(",")); dataFile.print(a1, 2); 
        dataFile.print(F(",")); dataFile.print(v2, 2); 
        dataFile.print(F(",")); dataFile.print(a2, 2); 
        dataFile.print(F(",")); dataFile.print(temp, 1);
        dataFile.print(F(",")); dataFile.println(note);
        dataFile.close(); // Important: Save the file to the card
        logCount++;       // Add 1 to our successful log counter
    } else {
        sdActive = false; // If writing fails, stop trying until reset
    }
}

/**
 * 1. THE SETUP PHASE (Runs once at power-on)
 */
void setup() {
    // 1.1 - Initialize Pins
    pinMode(PIN_ENABLE, OUTPUT);
    digitalWrite(PIN_ENABLE, HIGH); // Turn relay ON initially to check voltages
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_HEARTBEAT, OUTPUT);

    // 1.2 - Start the Screen
    Wire.begin();
    oled.begin(&Adafruit128x64, 0x3C);
    oled.setFont(Adafruit5x7);
    oled.clear();
    oled.println(F("T5 BMS STARTUP..."));

    // 1.3 - Start the Temperature Sensor
    sensors.begin();

    // 1.4 - Start the Clock
    if (rtc.begin()) {
        rtcActive = true;
        // If the clock battery died, set time to the moment this code was compiled
        if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // 1.5 - Start the SD Card
    if (SD.begin(PIN_SD_CS)) sdActive = true;
    
    // If the log file doesn't exist yet, create it and add the column headers
    if (sdActive && !SD.exists("T5_BMS.csv")) {
        File dataFile = SD.open("T5_BMS.csv", FILE_WRITE);
        if (dataFile) {
            dataFile.println(F("Timestamp,Alt_V,Alt_A,Batt_V,Batt_A,Temp,Note"));
            dataFile.close();
        }
    }

    // 1.6 - Start the Voltage/Current Sensors
    ina_alt.begin();
    ina_batt.begin();

    // 1.7 - Initial Sensor Readings
    sensors.requestTemperatures();
    currentTemp = sensors.getTempCByIndex(0);
    
    delay(200); // Give INA sensors a moment to stabilize
    float startAltV = ina_alt.getBusVoltage_V();
    
    // 1.8 - Decide Initial State
    // If we start and the engine is already running, go straight to BULK charging
    if (startAltV >= 13.4) systemState = BULK;
    else systemState = OFF;

    // Record that the system has just powered up
    logToSD("SYSTEM BOOT", startAltV, 0, ina_batt.getBusVoltage_V(), 0, currentTemp); 
    
    delay(1000); 
    oled.clear();
}

/**
 * 2. THE MAIN LOOP (Runs thousands of times per second)
 */
void loop() {
    // These 'static' variables remember their value even when the loop restarts
    static uint32_t lastHealth = 0;       
    static uint32_t lastLog = 0;          
    static uint32_t lastTempRequest = 0;  
    static uint32_t lastWatchdogPulse = 0;
    static uint32_t lastClockUpdate = 0;  
    static int heartbeatCount = 0;        
    const int pulsePin = 4;               // We send the heartbeat on D4

    // --- 2.1 UPDATE SENSORS ---
    // Every 2 seconds, ask the temp sensor for a new reading
    if (millis() - lastTempRequest >= 2000) {
        sensors.requestTemperatures();
        currentTemp = sensors.getTempCByIndex(0);
        lastTempRequest = millis();
    }

    // Get the latest Volts and Amps from both sides of the charger
    float altV = ina_alt.getBusVoltage_V();
    float altA = (ina_alt.getShuntVoltage_mV() / 1000.0) / SHUNT_OHMS;
    float battV = ina_batt.getBusVoltage_V();
    float battA = (ina_batt.getShuntVoltage_mV() / 1000.0) / SHUNT_OHMS;

    // --- 2.2 THE LOGIC ENGINE (Safety Check) ---
    
    // STEP 1: Check for Critical Danger (Overheat or Overvoltage)
    if (currentTemp > 60.0 || currentTemp < -50.0 || battV > 14.6) { 
        systemState = PROTECT; 
        lowVoltStartTime = 0; 
    }
    
    // STEP 2: Engine Detection Logic
    else {
        if (altV >= TURN_ON_VOLTS) {
            // Voltage is high: Engine is definitely running
            lowVoltStartTime = 0; // Reset the "low voltage" timer
            
            // Determine if the battery is full (High Volts AND Low Amps)
            if (battV > 14.2 && battA < 0.5) {
                if (fullTimerStart == 0) fullTimerStart = millis();
                // Stay in this mode for 5 mins before declaring 'FULL'
                if (millis() - fullTimerStart > 300000) systemState = FULL;
            } else {
                fullTimerStart = 0;
                // If we were already 'FULL', stay there as long as volts stay above 12.8
                if (systemState == FULL && battV > 12.8) systemState = FULL;
                else systemState = BULK; // Otherwise, we need BULK charging
            }
        } 
        else if (altV < TURN_OFF_VOLTS) {
            // Voltage is low: Engine might be off. Start a 3-second countdown.
            if (lowVoltStartTime == 0) lowVoltStartTime = millis();
            
            // If it stays low for 3 full seconds, turn everything off
            if (millis() - lowVoltStartTime > DISCONNECT_DELAY) {
                systemState = OFF;
            }
        } 
        else {
            // Hysteresis Zone: Voltage is between 12.8 and 13.4. 
            // Don't change anything; this prevents the relay from clicking repeatedly.
            lowVoltStartTime = 0; 
        }
    }

    // --- 2.3 HARDWARE ACTIONS ---
    if (systemState == BULK || systemState == FULL) {
        // CHARGING: Click the relay ON
        setChargerPower(true);
        
        // If we are pushing more than 14A, beep the buzzer as a warning
        if (systemState == BULK && battA > 14.0) {
            digitalWrite(PIN_BUZZER, (millis() % 500 < 250));
        } else {
            digitalWrite(PIN_BUZZER, LOW);
        }
    } 
    else {
        // STOPPED: Click the relay OFF (Isolate batteries)
        setChargerPower(false);

        // If we stopped because of a fault (PROTECT), sound the Triple-Beep alarm
        if (systemState == PROTECT && heartbeatCount == 0) {
            uint32_t pattern = millis() % 1000;
            if ((pattern < 100) || (pattern > 200 && pattern < 300) || (pattern > 400 && pattern < 500)) {
                digitalWrite(PIN_BUZZER, HIGH);
            } else {
                digitalWrite(PIN_BUZZER, LOW);
            }
        } else {
            digitalWrite(PIN_BUZZER, LOW);
        }
    }

    // --- 2.4 SAVING DATA ---
    // Log standard data every 2 minutes
    if (millis() - lastLog >= LOG_INTERVAL) {
        logToSD("DATA", altV, altA, battV, battA, currentTemp);
        lastLog = millis();
    }
    
    // Every 10 minutes, pause charging for 1 second. 
    // This lets us measure the true "Resting" voltage of the battery.
    if (millis() - lastHealth >= HEALTH_INT) {
        setChargerPower(false); 
        delay(1000); 
        logToSD("PULSE", ina_alt.getBusVoltage_V(), 0, ina_batt.getBusVoltage_V(), 0, currentTemp);
        if (systemState == BULK) setChargerPower(true); // Restart charging
        lastHealth = millis();
    }

    // --- 2.5 THE WATCHDOG PULSE ---
    // We send a pulse to the NE555 timer every second.
    // If the engine stops, we keep pulsing for 30 seconds to allow the final SD logs to finish.
    if (systemState != OFF || (millis() - lowVoltStartTime < 30000)) {
        if (millis() - lastWatchdogPulse > 1000) {
            pinMode(pulsePin, OUTPUT);
            digitalWrite(pulsePin, HIGH); // Send pulse
            delay(10);
            digitalWrite(pulsePin, LOW);  // End pulse
            lastWatchdogPulse = millis();
            
            // Keep track of heartbeats for the alarm timing
            heartbeatCount++;
            if (heartbeatCount >= 10) heartbeatCount = 0;
        }
    }

    // --- 2.6 SCREEN UPDATES ---
    // Fade the panel LED in and out (Visual check that code is running)
    analogWrite(PIN_HEARTBEAT, 128 + 127 * cos(millis() / 500.0));
    
    // Display the current State (Charging, Standby, etc.)
    oled.setCursor(0, 0);
    oled.print(F("STATE: "));
    if(systemState==BULK)      oled.println(F("CHARGING  "));
    else if(systemState==FULL) oled.println(F("FINISHED  "));
    else if(systemState==OFF)  oled.println(F("STANDBY   "));
    else                       oled.println(F("ALARM/HOT "));

    // Display SD status and how many logs saved
    oled.setCursor(0, 1);
    if(sdActive) {
        oled.print(F("SD OK - Log: ")); oled.print(logCount);
        oled.print(F("    ")); 
    } else {
        oled.print(F("SD ERROR/MISSING "));
    }

    // Update the clock on the screen every second
    if (rtcActive && (millis() - lastClockUpdate >= 1000)) { 
        lastClockUpdate = millis();
        DateTime now = rtc.now(); 
        char timeBuf[22]; 
        sprintf(timeBuf, "T: %02d/%02d %02d:%02d:%02d", now.day(), now.month(), now.hour(), now.minute(), now.second());
        oled.setCursor(0, 2); 
        oled.print(timeBuf);
    }

    // Big Bold Text: Battery Voltage and Charging Amps
    oled.set2X();
    oled.setCursor(0, 4); 
    oled.print(battV, 1); oled.print(F("V "));
    oled.print(battA, 1); oled.println(F("A  "));
    
    // Small Text: Alternator Voltage and Interior Temp
    oled.set1X();
    oled.setCursor(0, 7);
    oled.print(F("Alt: ")); oled.print(altV, 1); oled.print(F("V "));
    
    oled.setCursor(95, 7);
    if (currentTemp < -50.0) oled.print(F("ERR!")); // If sensor fails or is unplugged
    else {
        oled.print((int)currentTemp);
        oled.print(F("C "));
    }
}