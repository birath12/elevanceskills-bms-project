# Elevanceskills Internship — Project Report
**Modular Battery Management, Protection, and Telemetry System**

**Name:** Birath Kaur Ambhore  
**Internship:** Elevanceskills Embedded Systems Internship  
**Platform:** ESP32 (simulated in Wokwi)

---

## Table of Contents
1. [Task 1: Modular Battery Management Engine](#task-1-modular-battery-management-engine) — ✅ Completed
2. Task 2: Non-Blocking Protection Relay and Safety System — 🚧 Pending
3. Task 3: Flicker-Free LCD Display Engine — 🚧 Pending
4. Task 4: Fault State Machine with Structured Recovery — 🚧 Pending
5. Task 5: Event-Driven Telemetry and Live Blynk Dashboard — 🚧 Pending
6. Task 6: Enterprise Blynk Analytics and Decision Dashboard — 🚧 Pending

---

## Task 1: Modular Battery Management Engine

### Objective
Design a modular BMS engine operating on a scalable array of battery cells,
where the number of cells is controlled by a single compile-time constant.
The engine identifies the weakest and strongest cells, calculates voltage
imbalance, tracks whether imbalance is increasing or decreasing, and applies
adaptive thresholds based on estimated State of Charge (SoC) rather than
fixed limits.

### Design Overview
- Cell count controlled via `#define NUM_CELLS 4`
- Cell voltages simulated using potentiometers wired to ESP32 ADC pins
  (GPIO34, 35, 32, 33), mapped from raw ADC readings (0–4095) to a
  realistic Li-ion voltage range (3.0V–4.2V)
- Core functions: `readCellVoltage()`, `updateAllCells()`,
  `findWeakestCell()`, `findStrongestCell()`, `calculateImbalance()`,
  `estimateSoC()`, `getAdaptiveThreshold()`
- Adaptive threshold logic: stricter thresholds at low SoC, relaxed
  thresholds at high SoC, reflecting that small imbalances matter more
  when a pack is nearly empty

### Verification
Tested in Wokwi by varying potentiometer positions to simulate charging/
discharging cells. Confirmed via Serial Monitor output that:
- Weakest/strongest cell identification updates correctly as voltages change
- Imbalance calculation matches expected voltage differences
- Trend correctly reports INCREASING, DECREASING, or STABLE between readings
- Adaptive threshold shifted from 0.030V to 0.050V as average SoC increased
- Warning correctly triggers whenever imbalance exceeds the current threshold

### Scalability Analysis (4 → 16 cells)
**Memory:** All cell data is stored in arrays sized by `NUM_CELLS`. Each
array element is a 4-byte float, so even at 16 cells, total memory usage
is under 100 bytes — negligible against the ESP32's 520KB RAM.

**Execution time:** Core analysis functions (`findWeakestCell`,
`findStrongestCell`, `calculateImbalance`) all iterate once through the
cell array — O(n) time complexity. At 16 cells, this remains a
microsecond-scale operation on a 240MHz processor, with no meaningful
performance impact.

**Data structures:** Because the design uses arrays indexed by a single
`NUM_CELLS` constant (rather than individually named variables per cell),
scaling from 4 to 16 cells requires only two changes: updating the
`NUM_CELLS` value and extending the `cellPins[]` array with the additional
ADC pins. No changes to core logic are needed — this directly fulfills
the "modular, reusable interface" requirement, and mirrors how real
production BMS ICs (e.g., TI BQ76952) handle configurable cell counts.

---

## Task 2: Non-Blocking Protection Relay and Safety System
*Pending*

## Task 3: Flicker-Free LCD Display Engine
*Pending*

## Task 4: Fault State Machine with Structured Recovery
*Pending*

## Task 5: Event-Driven Telemetry and Live Blynk Dashboard
*Pending*

## Task 6: Enterprise Blynk Analytics and Decision Dashboard
*Pending*
