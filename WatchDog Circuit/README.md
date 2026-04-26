# 🩺 Technical Deep-Dive: The NE555 Watchdog Failsafe

The most critical safety feature of the **T5-BMS** is the hardware Watchdog circuit. In a vehicle environment, electromagnetic interference (EMI) or software hangs can cause a microcontroller to "freeze." If the Nano freezes while the 40A relay is closed, it could lead to a permanent connection that flattens your starter battery.

This circuit ensures that **physical hardware**—not just software—monitors the system's health.

---

## 🔬 Pulse & Coupling Analysis

| Technical Explanation | Oscilloscope Trace |
| :--- | :--- |
| **1. The Heartbeat Logic (Arduino Nano)** <br><br> The Arduino Nano is programmed to send a **1Hz Pulse** (1 heartbeat per second) from Pin D4. This pulse is the "Keep Alive" signal for the entire system. <br><br> As seen in the trace, it is a precise **10ms wide pulse** sent every **1000ms**. | <img src="https://raw.githubusercontent.com/MatsRobot/VW-T5-Intelligent-Split-Charge-Management-System/main/WatchDog%20Circuit/Pulses%20at%20D3%20Pin.png" width="400" alt="Nano Heartbeat Pulse" /> |
| **2. The Differentiator (AC Coupling)** <br><br> To prevent a "false positive" (where the Nano freezes with the LED stuck **ON**), the signal passes through a capacitor and resistor. <br><br> A capacitor blocks steady DC voltage and only allows the *transition* (the spikes) to pass. Even if the Nano freezes "High," the capacitor blocks the signal and the heartbeat "dies." | <img src="https://raw.githubusercontent.com/MatsRobot/VW-T5-Intelligent-Split-Charge-Management-System/main/WatchDog%20Circuit/Pulses%20after%20the%20Capacitor.jpg" width="400" alt="Pulses after Differentiator" /> |

---

## 3. Missing Pulse Detection (NE555 Monostable)

The heart of the watchdog is an **NE555 Timer** configured in **Monostable Mode** (a "one-shot" timer).

### The "Reset" Mechanism
Normally, a monostable timer starts a countdown and turns off. However, in this circuit, each pulse from the Nano hits a transistor that momentarily **shorts (discharges)** the timing capacitor of the 555. 

* **Pulse Present:** Every second, the "clock" is reset to zero before it can finish its countdown. The relay stays **ON**.
* **Pulse Missing:** If the Nano freezes or the engine stops, the "reset" stops. The 555 finishes its countdown and opens the relay.

| **Normal Operation (Pulse Present)** | **Fail Event (Pulse Missing)** |
| :--- | :--- |
| <img src="https://raw.githubusercontent.com/MatsRobot/VW-T5-Intelligent-Split-Charge-Management-System/main/WatchDog%20Circuit/Voltage%20at%20Transistor's%20collector_With%20Pulse%20Present.jpg" width="400"/> | <img src="https://raw.githubusercontent.com/MatsRobot/VW-T5-Intelligent-Split-Charge-Management-System/main/WatchDog%20Circuit/Voltage%20at%20Transistor's%20collector_With%20Pulse%20Missing.jpg" width="400"/> |
| Each "sawtooth" represents a heartbeat resetting the timer. | Without a pulse, the voltage reaches the limit, triggering shutdown. |

---

## 4. Dual-Transistor Stability

To achieve "Automotive Grade" reliability, the circuit uses two separate **BC547** transistors to interface with the **NE555 Timer**:

1.  **The Trigger Transistor:** This transistor provides a clean, sharp drop to ground on **Pin 2 (Trigger)** of the 555. This ensures the circuit triggers instantly and reliably the moment the very first pulse is detected from the Nano.
2.  **The Discharge Transistor:** This transistor handles the actual "Missing Pulse Detection" by shorting the 555's timing capacitor, effectively "kicking" the timer back to the start of its 30-second window every second.

### 🛠️ Watchdog Schematic
Refer to the official schematic for component values. The 30-second "Safety Window" is determined by the RC constant of the 100kΩ resistor and 100µF capacitor.

<p align="center">
  <img src="https://raw.githubusercontent.com/MatsRobot/VW-T5-Intelligent-Split-Charge-Management-System/main/WatchDog%20Circuit/Watchdog_555_Schematic.jpg" width="800" alt="Watchdog Schematic" />
</p>

---

<small>© 2026 MatsRobot | Licensed under the [MIT License](https://github.com/MatsRobot/matsrobot.github.io/blob/main/LICENSE)</small>
