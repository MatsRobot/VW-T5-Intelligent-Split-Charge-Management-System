# 🚐 VW-T5-Intelligent-Split-Charge-Management-System

**T5-BMS** is a custom-engineered **Power Management** solution designed to optimize the charging profile of VW Transporter leisure battery systems. This system is specifically designed for vehicles **without smart alternators**, where a software-defined management layer is added to the existing VSR infrastructure to provide active current regulation that standard relays cannot achieve.

---

<table width="100%">
  <tr>
    <td width="70%" align="left" valign="middle">
      <h2>🚀 The Backstory: Solving the Low-Resistance Trap</h2>
    </td>
    <td width="30%" align="center" valign="middle">
      <img src="https://raw.githubusercontent.com/MatsRobot/matsrobot.github.io/main/BMS_Logo.png" alt="T5 BMS Hardware Configuration" width="150" style="border-radius: 8px;" />
    </td>
  </tr>
  <tr>
    <td colspan="2">
      <p>
        The need for this system arises when upgrading to modern leisure batteries. New battery technologies feature significantly **lower internal resistance**; when connected to a standard VW Transporter alternator (non-smart version) via a basic VSR, they can pull excessive current that exceeds 20A.
      </p>
      <p>
        Without active limiting, this not only blows vehicle fuses but can <b>permanently damage your new leisure battery</b> by forcing a charge rate far beyond the manufacturer's specification. By adding the <b>LTC3780 Buck-Boost</b> controller and Arduino Nano, we create an intelligent "gate" that caps current at 14A, protecting both your wiring and your investment.
      </p>
    </td>
  </tr>
</table>

---

## ✨ Key Technical Features

* **Active Current Regulation:** Supplements the existing VSR by capping charging current at **14A**, preventing battery damage and blown fuses.
* **Non-Smart Alternator Compatibility:** Designed specifically for traditional VW T5 charging systems to provide modern DC-DC charging benefits.
* **Dual-Rail Telemetry:** Twin **INA219 High-Side sensors** monitor Alternator health and Leisure Battery intake in real-time.
* **Watchdog Power Latch:** An **NE555 circuit** ensures zero parasitic drain by physically isolating the system 2 minutes after the engine stops.
* **Persistent Data Logging:** Records voltage and amperage to a **CSV file** via SD Card and **DS3231 RTC**.

---

## 📐 Logic & Charge Stages

1. **STANDBY:** Remains isolated until alternator voltage exceeds **13.2V**.
2. **BULK:** Engages the 40A relay and limits current to **14A** via the LTC3780.
3. **FINISHED:** Disconnects once the battery hits **14.2V** with **<0.5A** draw to prevent overcharging.
4. **PROTECT:** Emergency cutoff and alarm if voltage exceeds **14.6V**.

---

<small>© 2026 MatsRobot | Licensed under the [MIT License](https://github.com/MatsRobot/matsrobot.github.io/blob/main/LICENSE)</small>
