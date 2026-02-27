# Wireless Fencing — LLM Context

## What is this project?

Wireless fencing scoring system for **foil** using **Raspberry Pi Pico W** (RP2040). Replaces the wired scoring apparatus. Two fencers each wear a Pico W that generates AC square wave frequencies on conductive surfaces and detects them at the foil tip via the body cord's 3 wires (A, B, C). A central Pico W arbitrates.

## Key discovery: no common ground needed

AC signal traverses a single wire between two separately-powered boards via parasitic capacitive coupling. A **10kΩ pull-down on the receiver pin is REQUIRED** for 3.3V Pico-to-Pico detection.

## Critical constraint: parasitic B↔C coupling in the foil

Lines B and C run millimeters apart inside the blade groove, forming a parasitic capacitor. A 5V/50mA MOSFET-buffered signal on C leaks to B. **Cannot emit on C and detect on B simultaneously.** This drove the architecture redesign from 4-pin to 3-pin with two modes.

## Critical constraint: AC signal attenuation inside the foil

The internal wire of the foil (~90cm in the blade groove) runs ~1mm from the metal blade, forming a **parasitic capacitor estimated at ~10nF**. This shunts AC signals to the blade before reaching the connector. At 20kHz, Zc ≈ 800Ω → most signal lost. At 1kHz, Zc ≈ 16kΩ → much less leakage. **Frequencies must be lowered from 20-40kHz to ~1-3kHz.** To be validated experimentally.

## GP16 protection: 10kΩ series resistor REQUIRED

When foil tip touches cuirasse (5V/20kHz via MOSFET), signal travels B→button→C→GP16. The 5V exceeds GPIO 3.3V tolerance → latch-up → reset. **10kΩ series resistor on GP16** limits current to 0.5mA. Button reading still works (0.47V LOW / 2.75V HIGH).

## Interrupt management: attach/detach by button state

At rest (button not pressed, B↔C closed), AC signal reaches GP2 via body cord → 20,000 ISR/s → CPU saturation → watchdog reset. **Solution: detachInterrupt when button not pressed**, attachInterrupt only when button pressed.

## Architecture (per fencer)

5 GPIO pins, wired once. Mode switching is software-only.

| GPIO | Line | Function |
|------|------|----------|
| GP14 (PWM out) | A (red) | Freq_VALID → cuirasse via MOSFET buffer |
| GP2 (interrupt in) | B (blue) | Frequency detection at foil tip (10kΩ pull-down to GND) |
| GP16 (INPUT_PULLUP) | C (green) | Button detection via DC reading |
| GP17 (PWM out) | C (green) | Mode Time-Division only: Freq_NEUTRE emission via MOSFET |
| GP15 (digital out) | — | Mode Time-Division only: cuts pull-up power during DETECT phase |

**Mode Simple** (current): GP15=LOW, GP17=LOW. No emission on C. Button detected by DC on GP16. Limitation: guard doesn't emit Freq_NEUTRE (touch on guard = white light instead of nothing).

**Mode Time-Division** (future): GP17 emits PWM 9ms, then GP15=LOW+GP17=LOW for 1ms while GP16 reads button. When button pressed → full detect mode (all emission off, GP2 reads clean).

## Frequencies (all exact on RP2040 PWM hardware)

**Original frequencies (validated in Phase 0, TOO HIGH for foil internal wire):**

| Freq | Usage | PWM wrap |
|------|-------|----------|
| 20 kHz | Freq_NEUTRE (guard/piste) | 6250 |
| 25 kHz | Freq_VALID_A (fencer A cuirasse) | 5000 |
| 40 kHz | Freq_VALID_B (fencer B cuirasse) | 3125 |

**Candidate low frequencies (to be validated in Phase 1.7bis):**

| Freq | Usage | PWM wrap | Zc (10nF) |
|------|-------|----------|-----------|
| 1 kHz | Freq_NEUTRE | 125,000 | 16 kΩ |
| 1.5 kHz | Freq_VALID_A | 83,333 | 10.6 kΩ |
| 2.5 kHz | Freq_VALID_B | 50,000 | 6.4 kΩ |

Detection: `attachInterrupt(pin, callback, RISING)` + pulse counting over 50ms window. Classification by tolerance bands (to be adjusted for lower frequencies).

## MOSFET buffer circuit (2N7000)

Required for driving capacitive loads (cuirasse) at 20kHz. GPIO → 100Ω → Gate, Source → GND, Drain → load, 100Ω pull-up to VBUS (5V). 10kΩ pull-up was too weak.

## Button detection (German tip, normally closed)

- Button NOT pressed: B↔C connected → GP16 pulled LOW by GP2's 10kΩ pull-down (~0.55V)
- Button pressed: B↔C open → GP16 floats HIGH via internal pull-up (~3.3V)
- Capacitive coupling blocks DC, mechanical button passes DC — this difference is exploited

## FIE specs

- Dwell time: 15ms (±0.5ms)
- Lockout: 300-350ms
- Communication must be << 300ms latency

## Tech stack

- **PlatformIO** with `platform = https://github.com/maxgerhardt/platform-raspberrypi.git`
- Board: `rpipicow`, core: `earlephilhower`, upload: `picotool`
- Signal generation: `hardware/pwm.h` (NOT `tone()` which is imprecise on Pico W)
- Serial: `delay(3000)` after `Serial.begin()`, NEVER `while(!Serial)`
- Communication: WiFi UDP (NOT ESP-NOW, incompatible with Pico W's CYW43439)

## Project status

### DONE
- Phase 0.1-0.4: Frequency generation, detection, multi-freq classification, no-common-GND validation, Pico-to-Pico validation
- Phase 1.1-1.5: Signal through real equipment, MOSFET buffer validated, parasitic coupling discovered, RC approach abandoned, architecture redesigned
- Phase 1.6: Button detection via DC on GP16 — VALIDATED
- Phase 1.7 (partial): GP16 crash fixed (10kΩ series resistor), interrupt avalanche fixed (attach/detach), foil attenuation discovered (BLOCKING)

### NEXT (Phase 1.7bis-1.8)
- **1.7bis**: Test lower frequencies (1-3 kHz) through the foil — BLOCKING, must validate before anything else
- **1.8**: Connect Freq_VALID on cuirasse (line A via MOSFET) and validate full loop with new frequencies

### FUTURE
- Phase 2: Complete system on Pico W (Mode Simple → Mode Time-Division)
- Phase 3: WiFi UDP communication
- Phase 4: Arbitration logic (FIE rules)
- Phase 5: Full 2-fencer integration
- Phase 6: Polish (LEDs, buzzer, enclosure, battery)

## Key files

- `PROJECT_PLAN.md` — Full project plan with circuit diagrams, all discoveries, phase details
- `phase1_6_button_dc/` — Phase 1.6/1.7 test code (current: diagnostic ISR for foil attenuation)
- `phase1_5_button_detect/` — Phase 1.5 test code (RC filter approach, obsolete but kept as history)
- `phase0_4_pico_to_pico/pico_generator/src/main.cpp` — PWM hardware generator reference
- `phase0_3_no_common_gnd/pico_receiver/src/main.cpp` — Frequency detection reference
- `phase0_4_pico_to_pico/pico_receiver/src/main.cpp` — Pico receiver with pull-down reference

## Rules for code

- Always use `hardware/pwm.h` for signal generation, never `tone()`
- Always add 10kΩ external pull-down on frequency detection pins
- Always use `delay(3000)` after `Serial.begin()`, never `while(!Serial)`
- Upload protocol is `picotool` (hold BOOTSEL while plugging USB)
- Test incrementally — validate each hardware assumption before building on it
