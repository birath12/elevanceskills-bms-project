# elevanceskills-bms-project
Modular BMS engine, protection relay, and telemetry system — Elevanceskills internship

# Elevanceskills Internship — Modular BMS & Telemetry Project

**Name:** Birath Kaur Ambhore  
**Internship:** Elevanceskills Embedded Systems Internship  
**Platform:** ESP32 (simulated on Wokwi)

## Project Overview
This repository contains a single integrated embedded systems project built
for the Elevanceskills internship. The project implements a modular Battery
Management System (BMS) and expands it across six tasks into a full
fault-tolerant, telemetry-enabled system with live dashboards.

## Tasks

- [x] Task 1: Modular Battery Management Engine — in progress
- [ ] Task 2: Non-Blocking Protection Relay and Safety System
- [ ] Task 3: Flicker-Free LCD Display Engine
- [ ] Task 4: Fault State Machine with Structured Recovery
- [ ] Task 5: Event-Driven Telemetry and Live Blynk Dashboard
- [ ] Task 6: Enterprise Blynk Analytics and Decision Dashboard

## Task 1: Modular Battery Management Engine
Implements a scalable BMS engine that:
- Reads voltage from an array of simulated battery cells (potentiometers on ESP32 ADC pins)
- Identifies the weakest and strongest cells
- Calculates voltage imbalance and tracks whether it is increasing or decreasing
- Applies adaptive imbalance thresholds based on estimated State of Charge (SoC)
- Exposes clean, reusable functions for future tasks to build on

Cell count is controlled by a single compile-time constant (`NUM_CELLS`),
allowing the design to scale from 4 cells to 16 cells without changing core logic.

## Tools Used
- **Wokwi** — circuit simulation (ESP32 + potentiometers)
- **Arduino C++** — firmware
- **GitHub** — version control and submission
- **Blynk** — live dashboard (Tasks 5–6)

## Repository Structure
sketch.ino - Main firmware code
README.md - This file
/report - Project report (added later)
/diagrams - Architecture and workflow diagrams (added later)
