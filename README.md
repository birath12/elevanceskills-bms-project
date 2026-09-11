# Elevanceskills Internship — Modular BMS & Telemetry Project

**Name:** Birath Kaur Ambhore  
**Internship:** Elevanceskills Embedded Systems Internship  
**Platform:** ESP32 (simulated on Wokwi)

## Project Overview
This repository contains a single integrated embedded systems project
built for the Elevanceskills internship. The project implements a
modular Battery Management System (BMS) and expands it across six
tasks into a full fault-tolerant, telemetry-enabled system with live
Blynk dashboards.

## Tasks
- [x] Task 1: Modular Battery Management Engine — ✅ Completed
- [x] Task 2: Non-Blocking Protection Relay and Safety System — ✅ Completed
- [x] Task 3: Flicker-Free LCD Display Engine — ✅ Completed
- [x] Task 4: Fault State Machine with Structured Recovery — ✅ Completed
- [x] Task 5: Event-Driven Telemetry and Live Blynk Dashboard — ✅ Completed
- [x] Task 6: Enterprise Blynk Analytics and Decision Dashboard — ✅ Completed

## Task 1: Modular Battery Management Engine
Implements a scalable BMS engine that:
- Reads voltage from an array of simulated battery cells
  (potentiometers on ESP32 ADC pins)
- Identifies the weakest and strongest cells
- Calculates voltage imbalance and tracks whether it is increasing or
  decreasing
- Applies adaptive imbalance thresholds based on estimated State of
  Charge (SoC)
- Exposes clean, reusable functions for future tasks to build on

Cell count is controlled by a single compile-time constant
(`NUM_CELLS`), allowing the design to scale from 4 cells to 16 cells
without changing core logic.

## Task 2: Non-Blocking Protection Relay and Safety System
Implements a fully non-blocking relay safety system that:
- Uses `millis()`-based timing throughout — no `delay()` in any
  safety-critical logic
- Applies hysteresis (separate trip/reset thresholds) and debounce
  timing to prevent relay chattering near threshold values
- Detects sensor anomalies (frozen readings, unrealistic jumps,
  out-of-range values) using a rolling-window moving average to
  distinguish real events from noise
- Follows a timed, verified recovery sequence after a fault clears,
  rather than resetting instantly
- Logs every relay state transition with a clear description

Simulated using an LED (GPIO25) as the relay indicator.

## Task 3: Flicker-Free LCD Display Engine
Implements an I2C LCD rendering engine that:
- Updates only the specific screen positions whose values changed,
  eliminating full-screen clears and visible flicker
- Automatically rotates through 3 information pages (battery status,
  system state, telemetry) on a non-blocking timer
- Immediately overrides to a dedicated fault screen during critical
  faults, resuming normal rotation only once the fault clears

Wired via I2C (SDA=GPIO21, SCL=GPIO22).

## Task 4: Fault State Machine with Structured Recovery
Implements a deterministic 4-state fault management layer that:
- Defines NORMAL, DEGRADED, FAILSAFE, and SHUTDOWN states via an enum,
  layered on top of the Task 2 relay state machine
- Isolates and tags the fault source (battery, ADC, relay, or
  communication) throughout the fault duration
- Logs every transition with a timestamp, previous state, new state,
  and fault source
- Requires a verification hold after the relay recovers before fully
  returning to NORMAL
- Escalates to SHUTDOWN if faults recur 3+ times within 30 seconds,
  requiring a manual button-hold reset — preventing endless
  fault/recovery cycling

A push button (GPIO26) simulates communication and relay-mismatch
faults for demonstration purposes.

## Task 5: Event-Driven Telemetry and Live Blynk Dashboard
Implements cloud telemetry that:
- Connects to WiFi via a non-blocking connection state machine
- Sends data to Blynk only when values change meaningfully
  (event-driven), rather than continuously streaming
- Queues telemetry in a fixed-size offline buffer when disconnected,
  flushing it in order once reconnected
- Monitors WiFi signal strength (RSSI)
- Displays cell voltages, weakest/strongest cell, imbalance, relay
  status, fault state, RSSI, and offline queue depth on a live Blynk
  web dashboard

## Task 6: Enterprise Blynk Analytics and Decision Dashboard
Extends the dashboard with:
- A composite 0–100 risk score combining imbalance trend, threshold
  breaches, low SoC, and fault severity/frequency
- Structured fault history (last 3 fault events with source and
  timestamp)
- Human-readable maintenance recommendations generated from the risk
  score
- Historical trend charts (Imbalance and SoC over time) using Blynk's
  built-in history and SuperChart widget
- An executive summary view (Risk Score, Fault Count, Uptime, Fault
  State) for at-a-glance system health

## Tools Used
- **Wokwi** — circuit simulation (ESP32, potentiometers, LCD, LED,
  push button)
- **Arduino C++** — firmware
- **GitHub** — version control and submission

- 
## Known Limitations
A few environment-specific constraints, documented transparently in
the full project report:
- WiFi RSSI reads 0 in simulation, as Wokwi's virtual network does not
  provide realistic signal strength data
- The offline-queue disconnect/reconnect cycle and Task 4's SHUTDOWN
  escalation path were implemented and logically verified but not
  exhaustively live-demonstrated in testing, due to Wokwi build server
  congestion — these will be shown in the final demo video
- **Blynk** — live cloud dashboard (Tasks 5–6)

## Repository Structure
