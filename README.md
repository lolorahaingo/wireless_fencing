<p align="center">
  <img src="https://img.shields.io/badge/Platform-Raspberry%20Pi%20Pico%20W-c51a4a?style=for-the-badge&logo=raspberrypi&logoColor=white" />
  <img src="https://img.shields.io/badge/Language-C%2B%2B-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/IDE-PlatformIO-FF7F00?style=for-the-badge&logo=platformio&logoColor=white" />
  <img src="https://img.shields.io/badge/PCB-KiCad%209-314CB0?style=for-the-badge&logo=kicad&logoColor=white" />
  <img src="https://img.shields.io/badge/Status-Phase%201%20(R%26D)-yellow?style=for-the-badge" />
</p>

# Wireless Fencing

> Replacing the wired scoring apparatus in competitive fencing with a fully wireless system built on Raspberry Pi Pico W microcontrollers.

---

## The Problem

In competitive foil fencing, every touch is detected by an electronic scoring apparatus connected to both fencers through **15-meter retractable cables**. These cables restrict movement, require constant maintenance, and make the sport inaccessible outside of dedicated venues.

**The fundamental challenge of going wireless:** without a physical wire connecting both fencers, there is **no common electrical ground** between them. The traditional system relies on DC circuits that require this shared reference. A completely different detection approach is needed.

---

## The Solution

Each fencer wears a **Raspberry Pi Pico W** that generates unique **AC square wave frequencies** on their conductive surfaces (lame/cuirasse). When a fencer's foil tip touches the opponent's valid target, the frequency is detected through the foil's internal wiring and identified — all without any electrical connection between the two fencers.

```
                    ┌─────────────────────────────────────────────────────────┐
                    │                    NO WIRE BETWEEN FENCERS              │
                    │                                                         │
  FENCER A          │          WiFi UDP            │          FENCER B        │
 ┌──────────┐       │       ┌──────────┐           │       ┌──────────┐      │
 │ Pico W   │       │       │ CENTRAL  │           │       │ Pico W   │      │
 │          │  ─ ─ ─│─ ─ ─ >│ Pico W   │< ─ ─ ─ ─ │─ ─ ─  │          │      │
 │ Generates│       │       │          │           │       │ Generates│      │
 │ Freq A   │       │       │ Arbitrate│           │       │ Freq B   │      │
 │ on lame  │       │       │ & Score  │           │       │ on lame  │      │
 └────┬─────┘       │       └──────────┘           │       └────┬─────┘      │
      │             │                                           │            │
   ┌──┴──┐          │                                        ┌──┴──┐         │
   │LAME │ ← Freq A │    Foil tip touches lame →             │LAME │ ← Freq B
   │     │          │    Freq detected = VALID TOUCH          │     │         │
   └─────┘          │                                        └─────┘         │
                    └─────────────────────────────────────────────────────────┘
```

### Why This Works

AC signals can traverse a single wire between two independently powered boards through **parasitic capacitive coupling** — the stray capacitance between the fencer's body, the floor, and the environment acts as the return path. This was the core hypothesis of the project, and it has been **experimentally validated**.

---

## System Architecture

### Per-Fencer Electronics

Each Pico W interfaces with the standard **3-wire body cord** (FIE convention) through a custom circuit with MOSFET buffers:

```
                         PICO W (RP2040)
                    ┌─────────────────────┐
                    │                     │
    ┌───────────────┤ GP14 (PWM out)      │
    │               │                     │
    │  ┌────────────┤ GP2  (interrupt in) │
    │  │            │                     │
    │  │  ┌─────────┤ GP16 (digital in)   │
    │  │  │         │                     │
    │  │  │         │ GP17 (PWM out)  ────┼──┐  (Mode Time-Division)
    │  │  │         │ GP15 (digital out)──┼──┤  (Mode Time-Division)
    │  │  │         └─────────────────────┘  │
    │  │  │                                  │
    ▼  ▼  ▼                                  ▼
 ┌──────────────────────────────────────────────┐
 │              MOSFET BUFFER STAGE              │
 │                                               │
 │   VBUS (5V)          VBUS (5V)                │
 │      │                  │                     │
 │   [100R]             [MOSFET C] ← GP15       │
 │      │                  │                     │
 │      ├─── Line A       [100R]                 │
 │      │   (red)          │                     │
 │   [MOSFET A]            ├─── Line C           │
 │      │    ← GP14        │   (green)           │
 │     GND              [MOSFET B] ← GP17       │
 │                         │                     │
 │                        GND                    │
 │                                               │
 │   GP2 ──┬── Line B (blue)                    │
 │         │                                     │
 │      [10kR] pull-down                        │
 │         │                                     │
 │        GND                                    │
 └───────────────────────────────────────────────┘
          │         │         │
          ▼         ▼         ▼
     ┌─────────────────────────────┐
     │       BODY CORD (3 wires)   │
     │                             │
     │  Line A ──── Lame/Cuirasse  │  ← emits Freq_VALID
     │  Line B ──── Foil tip       │  ← detects frequency
     │  Line C ──── Guard/shell    │  ← button detection (DC)
     └─────────────────────────────┘
```

### Detection Logic

The foil's push-button (German tip) is **normally closed**, connecting lines B and C. When the fencer presses the tip against a target, the circuit opens:

```
 BUTTON STATE          ELECTRICAL PATH              RESULT
 ─────────────────────────────────────────────────────────────
 Not pressed      B ←→ C connected (closed)     Nothing (no touch)
 (rest)           GP16 reads LOW (pulled down
                  by GP2's 10kΩ via B→C path)

 Pressed +        B isolated, tip touches        VALID TOUCH
 tip on lame      opponent's lame → Freq_VALID   (colored light)
                  detected on GP2

 Pressed +        B isolated, tip touches        No light
 tip on guard     neutral surface → Freq_NEUTRE  (annulled)

 Pressed +        B isolated, tip in air         WHITE LIGHT
 tip in air       no frequency on GP2            (off-target)
 ─────────────────────────────────────────────────────────────
```

### Communication

```
  Fencer A                                    Fencer B
  Pico W ──── WiFi UDP ────┐  ┌──── WiFi UDP ──── Pico W
                            ▼  ▼
                      ┌──────────┐
                      │ CENTRAL  │
                      │ Pico W   │
                      │          │
                      │ • AP mode│
                      │ • Lockout│  300-350ms (FIE rules)
                      │ • Dwell  │  15ms minimum contact
                      │ • Score  │
                      │ • LEDs   │
                      └──────────┘
```

- **Protocol:** Raw UDP over WiFi (ESP-NOW is Espressif-only, incompatible with Pico W's CYW43439 chip)
- **Topology:** Central Pico W acts as WiFi Access Point
- **Target latency:** < 20ms (well within the 300ms lockout window)

---

## Engineering Journey

This project follows a rigorous **hypothesis-driven** approach: each phase validates a specific assumption before building on it. Several discoveries forced significant architecture redesigns.

### Phase 0 — Proof of Concept

| Phase | Test | Result |
|-------|------|--------|
| 0.1 | Frequency detection on same board (Arduino Mega) | 25 kHz detected via interrupt counting |
| 0.2 | Multi-frequency classification (20/25/40 kHz) | 3 frequencies distinguished with ±2 kHz tolerance |
| 0.3 | **Detection without common ground** (Mega → Pico W) | ~19 kHz detected through parasitic coupling |
| 0.4 | Pico-to-Pico without common ground | **20,020 Hz** — near-perfect with 10kΩ pull-down |

> **Key validation:** AC signals traverse a single wire between independently powered boards. The core hypothesis holds.

### Phase 1 — Real Equipment Integration

This is where the real engineering challenges emerged:

#### Challenge 1: Capacitive Load of the Lame

The conductive lame (target vest) forms a large parasitic capacitor with the environment. The Pico's GPIO (3.3V, 12mA) couldn't drive it at 20 kHz.

```
 Problem:  GPIO direct → lame         5-7 kHz (should be 20 kHz)
 Solution: MOSFET 2N7000 buffer       20 kHz perfect
           + 100Ω pull-up to 5V
           (10kΩ pull-up was too weak — only 0.5mA couldn't charge
            the parasitic capacitance fast enough)
```

#### Challenge 2: Parasitic B↔C Coupling Inside the Foil

Lines B and C run millimeters apart inside the blade groove, forming a parasitic capacitor. The 5V/50mA buffered signal on line C leaked to line B, making it impossible to emit on C and detect on B simultaneously.

```
 Before (4-pin design):
   GP14 → emit on A (lame)     ✓
   GP2  ← detect on B (tip)    ✗ sees C's signal via parasitic coupling
   GP16 → emit on C (guard)    ✗ leaks to B
   GP17 ← detect button        ✗ always sees 20 kHz

 After (3-pin redesign):
   GP14 → emit on A (lame)     ✓ MOSFET buffer
   GP2  ← detect on B (tip)    ✓ clean signal, no C emission
   GP16 ← detect button (DC)   ✓ exploits DC vs AC difference
```

> **Insight:** Capacitive coupling blocks DC but passes AC. The mechanical button passes DC but the parasitic coupling doesn't. By reading DC on line C, we can detect the button state without any interference.

#### Challenge 3: 5V Signal Crashing the Pico

When the foil tip touches the lame (5V via MOSFET), the signal travels back through the button to GP16, exceeding the 3.3V GPIO tolerance and causing a latch-up reset.

```
 Solution: 10kΩ series resistor on GP16
           Limits current to 0.5mA (protection diodes handle it)
           Button reading still works: 0.47V (LOW) / 2.75V (HIGH)
```

#### Challenge 4: Signal Attenuation in the Foil

The foil's internal wire (~90cm in the blade groove) runs ~1mm from the metal blade, forming a **~10nF parasitic capacitor** that shunts AC signals to ground.

```
 Frequency    Impedance (10nF)    Detection
 ──────────────────────────────────────────
 20 kHz       ~800Ω               1,700 Hz detected (massive loss)
  5 kHz       ~3.2kΩ              500-1,700 Hz (still too much loss)
  1 kHz       ~16kΩ               should pass (to be validated)
```

> **Current blocker:** Frequencies must be lowered from 20-40 kHz to ~1-3 kHz. Experimental validation pending.

### Progress Overview

```
 Phase 0   Proof of Concept                          ██████████ DONE
 Phase 1   Real Equipment Integration                ████████░░ 80% (freq. validation pending)
 Phase 2   Complete System (Simple + Time-Division)  ░░░░░░░░░░ TODO
 Phase 3   WiFi UDP Communication                    ░░░░░░░░░░ TODO
 Phase 4   FIE Arbitration Logic                     ░░░░░░░░░░ TODO
 Phase 5   Full 2-Fencer Integration                 ░░░░░░░░░░ TODO
 Phase 6   Polish (LEDs, battery, enclosure)         ░░░░░░░░░░ TODO
```

---

## Key Technical Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| **Detection method** | Interrupt pulse counting (not Goertzel/FFT) | Signal is square wave — energy spread across harmonics makes FFT unreliable. Counting rising edges is simpler and more robust |
| **Signal generation** | RP2040 hardware PWM (not `tone()`) | `tone()` on Pico W is imprecise at high frequencies (~14 kHz instead of 20 kHz). Hardware PWM is cycle-exact |
| **Communication** | WiFi UDP (not ESP-NOW) | ESP-NOW is Espressif-only. Pico W uses Infineon CYW43439 — must use standard WiFi |
| **Button detection** | DC reading on line C (not RC filter) | RC filter approach failed due to B↔C parasitic coupling. DC reading exploits the physical difference between capacitive coupling (blocks DC) and mechanical switch (passes DC) |
| **Current buffer** | 2N7000 MOSFET + 100Ω pull-up to 5V | GPIO can't drive capacitive loads. 100Ω provides ~50mA — enough to charge/discharge parasitic capacitance at 20 kHz |
| **Receiver pull-down** | External 10kΩ (not internal ~50kΩ) | Internal pull-down too weak for 3.3V signals without common ground. 10kΩ gives perfect detection |

---

## Hardware

| Component | Role | Qty |
|-----------|------|-----|
| Raspberry Pi Pico W | Fencer controller + central arbiter | 3 |
| 2N7000 N-MOSFET | Current buffer for driving conductive surfaces | 3 per fencer |
| 100Ω resistors | MOSFET pull-ups + gate protection | 4 per fencer |
| 10kΩ resistors | Receiver pull-down + GP16 protection | 2 per fencer |
| Electric foil (German tip) | Standard FIE weapon | 2 |
| Body cord (3-wire) | Standard FIE body cord | 2 |
| Conductive lame | Valid target surface | 2 |

A custom PCB is being designed in **KiCad 9** (`wireless_fencing_fencer/`).

---

## Project Structure

```
wireless_fencing/
├── phase0_1_freq_interrupt/     # Frequency detection on Arduino Mega
├── phase0_2_freq_detection/     # Multi-frequency classification
├── phase0_3_no_common_gnd/      # No-common-ground validation
│   ├── mega_generator/          #   Arduino Mega signal generator
│   └── pico_receiver/           #   Pico W receiver
├── phase0_4_pico_to_pico/       # Pico-to-Pico validation
│   ├── pico_generator/          #   PWM hardware generator
│   └── pico_receiver/           #   Interrupt-based receiver
├── phase1_5_button_detect/      # Button detection (RC approach — abandoned)
├── phase1_6_button_dc/          # Button detection via DC + diagnostic tests
├── wireless_fencing_fencer/     # KiCad 9 PCB design
├── gpio_test/                   # GPIO validation utilities
├── PROJECT_PLAN.md              # Detailed engineering log (1000+ lines)
└── CLAUDE.md                    # AI assistant context file
```

---

## FIE Compliance

The system targets compliance with FIE (International Fencing Federation) timing specifications for foil:

| Parameter | FIE Specification | System Target |
|-----------|-------------------|---------------|
| Dwell time (minimum contact) | 15ms ± 0.5ms | Measured per touch |
| Lockout time | 300-350ms | Configurable timer on central |
| Communication latency | — | < 20ms (UDP) |

---

## Tech Stack

- **Microcontroller:** Raspberry Pi Pico W (RP2040, Cortex-M0+, 133 MHz)
- **Framework:** Arduino (earlephilhower core)
- **Build system:** PlatformIO
- **Signal generation:** RP2040 hardware PWM (`hardware/pwm.h`)
- **PCB design:** KiCad 9
- **Communication:** WiFi UDP (CYW43439)

---

<p align="center">
  <i>An ongoing R&D project exploring the intersection of embedded systems, signal processing, and competitive sports.</i>
</p>
