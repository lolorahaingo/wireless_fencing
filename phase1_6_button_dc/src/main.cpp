// =============================================================================
// Phase 1.7 — TEST DIAGNOSTIC
// =============================================================================
//
// Test simplifie : PAS de bouton, PAS de GP16.
// Juste GP2 avec interruption en permanence, comme Phase 0.4.
// Mais avec tout le circuit cable (MOSFETs, resistances, fleuret).
//
// BUT : voir si GP2 capte 20 kHz a travers le fleuret quand on
//       touche la cuirasse, avec le circuit complet cable.
//       Si oui → le probleme est dans la logique bouton.
//       Si non → le probleme est hardware (circuit perturbe GP2).
//
// CABLAGE : tout reste branche comme c'est.
//   GP16 avec 10kOhm serie → on s'en sert pas, on le lit meme pas.
//   GP2 + 10kOhm pull-down → detection frequence (interrupt RISING)
//
// =============================================================================

#include <Arduino.h>

// =============================================================================
// PINS
// =============================================================================

const int PIN_FREQ_IN  = 2;
const int PIN_BUTTON   = 16;
const int PIN_PWM_A    = 14;
const int PIN_MOSFET_C = 15;
const int PIN_PWM_C    = 17;

// =============================================================================
// FREQUENCES
// =============================================================================

const unsigned int FREQ_NEUTRE  = 20000;
const unsigned int FREQ_VALID_A = 25000;
const unsigned int FREQ_VALID_B = 40000;
const unsigned int TOLERANCE    = 2000;

// =============================================================================
// PARAMETRES
// =============================================================================

const unsigned int MEASURE_PERIOD_MS = 50;
const unsigned int DISPLAY_PERIOD_MS = 500;

// =============================================================================
// ISR — identique Phase 0.4
// =============================================================================

volatile unsigned long pulseCount = 0;

void countPulse() {
    pulseCount++;
}

// =============================================================================
// CLASSIFICATION
// =============================================================================

const char* classifyFrequency(unsigned long freq) {
    if (freq < 100)
        return "AUCUNE";
    if (freq >= FREQ_NEUTRE  - TOLERANCE && freq <= FREQ_NEUTRE  + TOLERANCE)
        return "NEUTRE  (20 kHz)";
    if (freq >= FREQ_VALID_A - TOLERANCE && freq <= FREQ_VALID_A + TOLERANCE)
        return "VALID_A (25 kHz)";
    if (freq >= FREQ_VALID_B - TOLERANCE && freq <= FREQ_VALID_B + TOLERANCE)
        return "VALID_B (40 kHz)";
    return "INCONNUE";
}

// =============================================================================
// VARIABLES
// =============================================================================

unsigned long measuredFreqHz   = 0;
unsigned long lastMeasureTime  = 0;
unsigned long lastDisplayTime  = 0;

// =============================================================================
// SETUP — copie de Phase 0.4 + pins inactifs
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(3000);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // GP2 : interruption RISING (comme Phase 0.4)
    pinMode(PIN_FREQ_IN, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_FREQ_IN), countPulse, RISING);

    // GP16 : on le met en INPUT simple (pas de pull-up, on le touche pas)
    pinMode(PIN_BUTTON, INPUT);

    // GP14, GP15, GP17 : LOW
    pinMode(PIN_PWM_A, OUTPUT);
    digitalWrite(PIN_PWM_A, LOW);
    pinMode(PIN_MOSFET_C, OUTPUT);
    digitalWrite(PIN_MOSFET_C, LOW);
    pinMode(PIN_PWM_C, OUTPUT);
    digitalWrite(PIN_PWM_C, LOW);

    Serial.println();
    Serial.println("=====================================================");
    Serial.println("  Phase 1.7 — TEST DIAGNOSTIC");
    Serial.println("  GP2 interrupt RISING en permanence (comme Phase 0.4)");
    Serial.println("  GP16 ignore, pas de logique bouton");
    Serial.println("  Tout le circuit reste branche");
    Serial.println("=====================================================");
    Serial.println("  Touche la cuirasse avec le fleuret.");
    Serial.println("  Bouton presse ou pas, on s'en fiche.");
    Serial.println("  On veut juste voir la frequence sur GP2.");
    Serial.println("=====================================================");
    Serial.println();

    lastMeasureTime = millis();
    lastDisplayTime = millis();
}

// =============================================================================
// LOOP — copie de Phase 0.4 (sans ADC)
// =============================================================================

void loop() {
    unsigned long now = millis();

    // Mesure toutes les 50 ms
    if (now - lastMeasureTime >= MEASURE_PERIOD_MS) {
        noInterrupts();
        unsigned long count = pulseCount;
        pulseCount = 0;
        interrupts();

        unsigned long elapsed = now - lastMeasureTime;
        lastMeasureTime = now;

        measuredFreqHz = (count * 1000UL) / elapsed;
    }

    // Affichage toutes les 500 ms
    if (now - lastDisplayTime >= DISPLAY_PERIOD_MS) {
        lastDisplayTime = now;

        Serial.print("Freq: ");
        Serial.print(measuredFreqHz);
        Serial.print(" Hz | ");
        Serial.print(classifyFrequency(measuredFreqHz));
        Serial.print(" | GP16=");
        Serial.println(digitalRead(PIN_BUTTON) ? "HIGH" : "LOW");
    }
}
