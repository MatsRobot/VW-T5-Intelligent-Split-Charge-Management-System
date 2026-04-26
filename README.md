# 🚐 VW-T5-Intelligent-Split-Charge-Management-System (Version 1.0)

**VW-T5 Battery Management System (T5-BMS)** is a custom-engineered **Power Management** solution designed to correct the "low-resistance trap" in VW Transporter campervan conversions. By adding a software-defined management layer—powered by an **Arduino Nano** and an **LTC3780 Buck-Boost** controller—this system provides active current regulation, precision telemetry, and automated failsafe isolation for leisure battery systems.

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
 Modern leisure batteries (AGM, Gel, and Lithium) feature significantly **lower internal resistance** than legacy batteries. When connected to a high-output **non-smart alternator** via a standard Voltage Sensitive Relay (VSR), they attempt to pull massive current—often exceeding 30A.
     </p>
      <p>
This surge not only blows the vehicle's original 20A fuses but can permanently damage your battery. This project provides the missing "intelligence" by adding an active management stage that caps current at a safe **14A** while ensuring a rock-steady **14.4V charge profile**.
    </p>
    </td>
  </tr>
</table>

---

## ✨ Key Technical Features (V1.0)

* **Active Current Regulation:** Hardware-capping the charging current at **14A** using the LTC3780 Constant Current (CC) mode.
* **Hysteresis-Based Switching:** Prevents relay chatter by connecting at **13.4V** and disconnecting only if the voltage remains below **12.8V** for more than **3 continuous seconds**.
* **Precision Dual-Rail Telemetry:** Uses two **INA219 sensors** with modified **0.01 Ohm shunts** for high-accuracy monitoring of input and output power.
* **Thermal Failsafe:** Integrated **DS18B20 sensor** triggers a physical disconnect and a **Triple-Pulse Alarm** if internal enclosure temperatures exceed **60°C**.
* **Watchdog Power Latch:** An integrated **NE555 circuit** monitors a 1Hz heartbeat from the Nano. It provides a **30-second grace period** for final data logging after the engine stops before cutting all parasitic drain.
* **Persistent Data Logging:** Automatically records time-stamped voltage, amperage, and temperature to a **CSV file** on an SD Card via a **DS3231 RTC**.
  
---

<img width="4986" height="2850" alt="Battery Management System - Nano_bb" src="https://github.com/user-attachments/assets/c9b9c29e-7af7-4797-a5bf-2665156401bc" />

---


## 🛠️ Hardware Stack

### 1. The Brain (Arduino Nano)
Manages the state machine, monitors safety thresholds, and drives the **SSD1306 OLED** diagnostics.

### 2. The Power Stage (LTC3780)
Maintains a constant 14.4V output regardless of whether the alternator voltage is sagging or peaking.

### 3. Safety Watchdog (NE555)
To prevent battery drain or software hangs, the Nano sends a heartbeat to an NE555 timer. If the heartbeat stops, the system physically isolates the battery after **30 seconds**.

---

## 📐 Logic & Charge Stages

The system software implements a robust state machine:

1. **OFF/STANDBY:** System isolated until alternator > **13.4V**.
2. **BULK:** Relay engaged. LTC3780 provides 14.4V, capped at 14A.
3. **FULL/FINISHED:** Triggered at **14.2V** and < **0.5A** current for 5 mins.
4. **PROTECT:** Instant disconnect if Temp > **60°C**, Sensor Error, or Battery > **14.6V**.
5. **PULSE CHECK:** Every 10 mins, charging halts momentarily to log resting voltage.

---

## 🗺️ Future Roadmap

* **Solar Integration:** Adding an MPPT charging rail to balance power between the Alternator and Solar panels.
* **Bluetooth Telemetry:** HC-05 module integration for live smartphone dashboard monitoring.
* **LiFePO4 Profiles:** Software updates to support the specific voltage cut-offs required for Lithium Iron Phosphate.

---

<small>© 2026 MatsRobot | Licensed under the [MIT License](https://github.com/MatsRobot/matsrobot.github.io/blob/main/LICENSE)</small>
