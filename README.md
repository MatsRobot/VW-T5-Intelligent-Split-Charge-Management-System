# 🚐 VW-T5-Intelligent-Split-Charge-Management-System

**T5-BMS** is a custom-engineered **Power Management** solution designed to solve the "low-resistance trap" in modern campervan conversions. By replacing a standard Voltage Sensitive Relay (VSR) with a software-defined **LTC3780 Buck-Boost** controller and an **Arduino Nano**, the system provides precise current limiting, data telemetry, and automated failsafe isolation for VW T5 leisure battery systems.

---

<table width="100%">
  <tr>
    <td width="70%" align="left" valign="middle">
      <h2>🚀 The Backstory: The Split-Charge Failure</h2>
    </td>
    <td width="30%" align="center" valign="middle">
      <img src="https://raw.githubusercontent.com/MatsRobot/matsrobot.github.io/main/BMS_Logo.png" alt="T5 BMS Hardware Configuration" width="150" style="border-radius: 8px;" />
    </td>
  </tr>
  <tr>
    <td colspan="2">
      <p>
        Standard split-charge systems (VSRs) rely on a direct mechanical bridge between the starter and leisure batteries. When new, low-internal-resistance batteries are installed, they can pull massive current from the alternator—often exceeding the limits of the vehicle's original loom and causing 20A fuses to blow repeatedly during transit.
      </p>
      <p>
        This project eliminates the "dumb" relay logic. By placing a <b>DC-to-DC converter</b> between the batteries, we create a current-limited "gate" that protects the wiring while providing <b>14.4V Bulk charging</b> even when the alternator voltage fluctuates. The result is a self-contained power station with <b>Real-Time Telemetry</b> and <b>Zero-Drain Shutdown</b>.
      </p>
    </td>
  </tr>
</table>

---

## ✨ Key Technical Features

* **Active Current Limiting:** Hardware-capped at **14A** via the LTC3780, ensuring vehicle fuses and wiring remain protected.
* **Dual-Rail Telemetry:** Twin **INA219 High-Side sensors** monitor both the Alternator input and the Battery output in real-time.
* **Watchdog Power Latch:** An integrated **NE555 "Missing Pulse Detector"** provides a 2-minute "grace period" for data logging after the engine stops before physically cutting all parasitic drain.
* **Persistent Data Logging:** Records voltage and amperage to a **CSV file** on an SD Card, time-stamped by a high-precision **DS3231 RTC**.
* **Safety Interlocks:** Automated buzzer alerts for over-current conditions and a dedicated **40A mechanical relay** for physical high-current isolation.

## 🛠️ Hardware Stack

* **Logic:** Arduino Nano (ATmega328P).
* **Power Regulation:** LTC3780 Automatic Step Up-down Power Module (manually set to 14.4V).
* **Sensing:** 2x Adafruit INA219 (I2C Addresses 0x40 and 0x41).
* **Display:** SSD1306 OLED (128x64) via I2C for real-time diagnostics.
* **Latch Circuit:** NE555 Timer, BC337 Transistors, and a 47µF Timing Capacitor for the watchdog circuit.

---

## 📐 Logic & Charge Stages

The system uses a software **State Machine** to manage the battery lifecycle and prevent rapid relay switching.

### 1. STANDBY Stage (OFF)
* **Trigger:** Alternator Voltage is detected below **13.2V**.
* **Action:** The 40A Relay is disconnected to prevent draining the starter battery.

### 2. BULK Stage (CHARGING)
* **Trigger:** Alternator Voltage is detected above **13.2V**.
* **Action:** The 40A Relay is engaged. The system monitors output current; if it exceeds **14A**, a buzzer provides an audible warning.

### 3. FINISHED Stage (FULL)
* **Trigger:** Battery Voltage exceeds **14.2V** AND current draw drops below **0.5A** for a continuous 5 minutes.
* **Action:** Charging stops to protect the battery from overcharging. The system uses hysteresis to remain in this state until the voltage drops below **12.8V**.

### 4. PROTECT Stage (OVERVOLT)
* **Trigger:** Battery Voltage is detected above **14.6V**.
* **Action:** Immediate emergency disconnect of the charging relay and a solid buzzer alarm.

---

<small>© 2026 MatsRobot | Licensed under the [MIT License](https://github.com/MatsRobot/matsrobot.github.io/blob/main/LICENSE)</small>


