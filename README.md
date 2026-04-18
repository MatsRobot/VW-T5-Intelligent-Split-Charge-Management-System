# 🚐 VW-T5-Intelligent-Split-Charge-Management-System

**T5-BMS** is a custom-engineered **Power Management** solution designed to correct the "low-resistance trap" in VW Transporter campervan conversions. By adding a software-defined management layer—powered by an **Arduino Nano** and an **LTC3780 Buck-Boost** controller—this system provides active current regulation, precision telemetry, and automated failsafe isolation for leisure battery systems.

---

<table width="100%">
  <tr>
    <td width="70%" align="left" valign="middle">
      <h2>🚀 The Backstory: Solving the Low-Resistance Trap</h2>
    </td>
    <td width="30%" align="center" valign="middle">
      <img src="https://github.com/user-attachments/assets/50933aef-f656-4cf1-82ef-0227d1f4a8d5"  alt="T5 BMS Management Architecture" width="150" style="border-radius: 8px;" />
    </td>
  </tr>
  <tr>
    <td colspan="2">
      <p>
        Modern leisure batteries (AGM, Gel, and Lithium) feature significantly <b>lower internal resistance</b> than the legacy batteries originally designed for the VW T5. When these new batteries are connected to a high-output <b>non-smart alternator</b> via a standard Voltage Sensitive Relay (VSR), they attempt to pull massive current—often exceeding 30A.
      </p>
      <p>
        This surge not only blows the vehicle's original 20A fuses but can <b>permanently damage your new battery</b> by forcing a charge rate far beyond its specification. This project provides the missing "intelligence" by adding an active management stage that caps current at a safe <b>14A</b> while ensuring a rock-steady 14.4V charge profile.
      </p>
    </td>
  </tr>
</table>

---

## ✨ Key Technical Features

* **Active Current Regulation:** Supplements the existing VSR by hardware-capping the charging current at **14A**, protecting both the vehicle loom and the battery chemistry.
* **Non-Smart Alternator Optimization:** Specifically designed for T5/T5.1 models with traditional alternators to provide the benefits of a modern DC-to-DC charger.
* **Precision Dual-Rail Telemetry:** Features two **INA219 sensors** with modified **0.01 Ohm shunts** for high-accuracy monitoring of both input and output power.
* **Watchdog Power Latch:** An integrated **NE555 "Missing Pulse Detector"** circuit provides a 2-minute "grace period" for data logging after the engine stops before physically cutting all parasitic drain.
* **Persistent Data Logging:** Automatically records time-stamped voltage and amperage to a **CSV file** on an SD Card via a **DS3231 RTC**.

---

<img  alt="Battery Management System - Nano_bb" src="https://github.com/user-attachments/assets/10110602-3062-4d80-888c-5e8549cef712" />

---


## 🛠️ Hardware Stack & Engineering Choices

### 1. The Microcontroller (Arduino Nano)
The "brain" of the system manages the state machine, monitors safety thresholds, and drives the **SSD1306 OLED** for real-time diagnostics.

### 2. High-Precision Sensing (INA219 + Shunt Mod)
Standard current sensors often lack the resolution or range for high-current automotive use. We modified the INA219 modules by replacing the stock shunts with **0.01 Ohm (R100) shunts**, allowing the Nano to measure up to 14A with high precision on both the Alternator and Battery rails.

### 3. The Power Stage (LTC3780)
The **LTC3780 Buck-Boost converter** was chosen for its ability to maintain a constant 14.4V output regardless of whether the alternator voltage is sagging (11V) or peaking (14.8V).

### 4. Safety Watchdog (NE555)
To prevent software hangs from draining the starter battery, the Nano sends a 1Hz heartbeat to an **NE555 timer**. If the heartbeat stops (due to code error or engine shutdown), the system physically isolates the battery via a **40A mechanical relay** after 120 seconds.

---

## 📐 Logic & Charge Stages

The system software implements a robust state machine to handle the battery lifecycle:

1. **STANDBY:** The system remains isolated until the alternator voltage exceeds **13.2V**.
2. **BULK:** The 40A relay engages, and the LTC3780 provides a stabilized 14.4V charge capped at 14A.
3. **FINISHED:** Triggered when the battery reaches **14.2V** and current draw drops below **0.5A** for 5 minutes, preventing overcharging.
4. **PULSE CHECK:** Every 10 minutes, the charge is momentarily halted to log accurate "resting" battery voltage to the SD card.

---

## 🗺️ Future Roadmap

* **Solar Integration:** Adding an MPPT charging rail to balance power between the Alternator and Solar panels.
* **Bluetooth Telemetry:** HC-05 module integration for live smartphone dashboard monitoring.
* **LiFePO4 Profiles:** Software updates to support the specific voltage cut-offs required for **Lithium Iron Phosphate** batteries.

---

<small>© 2026 MatsRobot | Licensed under the [MIT License](https://github.com/MatsRobot/matsrobot.github.io/blob/main/LICENSE)</small>
