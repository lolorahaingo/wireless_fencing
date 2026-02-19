#include <Arduino.h>

// =============================================================================
// Phase 0.4 — Pico to Pico : RÉCEPTEUR
// Escrime sans fil — test Pico générateur → Pico récepteur (sans Arduino Mega)
//
// Câblage attendu :
//   GPIO 15 (Pico générateur) → GPIO 2  (Pico récepteur) — signal carré
//   GPIO 15 (Pico générateur) → GPIO 26 (Pico récepteur) — optionnel, lecture ADC
//   PAS de fil GND entre les deux Pico
//
// Alimentation :
//   Pico générateur : adaptateur secteur via multiprise
//   Pico récepteur  : USB sur Mac (Serial Monitor disponible)
// =============================================================================

// --- Broches ---
const int PIN_INTERRUPT = 2;   // GPIO 2 : entrée signal carré (interruption)
const int PIN_ADC       = 26;  // GPIO 26 : lecture ADC optionnelle (info bonus)

// --- Fréquences de référence (Hz) ---
const unsigned int FREQ_NEUTRE  = 20000;
const unsigned int FREQ_VALID_A = 25000;
const unsigned int FREQ_VALID_B = 40000;
const unsigned int TOLERANCE    = 2000;  // ±2 kHz autour de chaque fréquence cible

// --- Périodes de mesure et d'affichage (ms) ---
const unsigned int MEASURE_PERIOD  = 50;   // calcul fréquence toutes les 50 ms
const unsigned int DISPLAY_PERIOD  = 500;  // affichage toutes les 500 ms

// --- Variables partagées avec l'ISR ---
volatile unsigned long pulseCount = 0;  // compteur d'impulsions (incrémenté par ISR)

// --- Variables de timing ---
unsigned long lastMeasureTime = 0;
unsigned long lastDisplayTime = 0;

// --- Fréquence calculée ---
unsigned long measuredFreqHz = 0;

// --- Statistiques ADC ---
unsigned int  adcMin   = 4095;
unsigned int  adcMax   = 0;
unsigned long adcSum   = 0;
unsigned int  adcCount = 0;

// --- ISR : compte chaque front montant ---
void countPulse() {
    pulseCount++;
}

// --- Classifie la fréquence mesurée ---
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
void setup() {
    Serial.begin(115200);
    delay(3000);  // attente fixe — pas de while(!Serial) pour rester non-bloquant

    // LED intégrée allumée au démarrage pour confirmer l'initialisation
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Résolution ADC 12 bits (0–4095)
    analogReadResolution(12);

    // GPIO 2 en entrée, interruption sur front montant
    pinMode(PIN_INTERRUPT, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_INTERRUPT), countPulse, RISING);

    // En-tête dans le Serial Monitor
    Serial.println("==============================================");
    Serial.println("  Phase 0.4 — Pico to Pico | RECEPTEUR");
    Serial.println("  Signal : GPIO 15 (gen) -> GPIO 2 (rec)");
    Serial.println("  ADC    : GPIO 15 (gen) -> GPIO 26 (rec) [optionnel]");
    Serial.println("  Pas de GND commun entre les deux Pico");
    Serial.println("==============================================");

    lastMeasureTime = millis();
    lastDisplayTime = millis();
}

// =============================================================================
void loop() {
    unsigned long now = millis();

    // ------------------------------------------------------------------
    // 1. Lecture ADC en continu (info bonus, non bloquant)
    // ------------------------------------------------------------------
    unsigned int adcVal = analogRead(PIN_ADC);
    if (adcVal < adcMin) adcMin = adcVal;
    if (adcVal > adcMax) adcMax = adcVal;
    adcSum += adcVal;
    adcCount++;

    // ------------------------------------------------------------------
    // 2. Calcul de la fréquence toutes les MEASURE_PERIOD ms
    // ------------------------------------------------------------------
    if (now - lastMeasureTime >= MEASURE_PERIOD) {
        // Lecture atomique du compteur d'impulsions
        noInterrupts();
        unsigned long count = pulseCount;
        pulseCount = 0;
        interrupts();

        // Fréquence = impulsions / durée réelle (en secondes)
        unsigned long elapsed = now - lastMeasureTime;
        measuredFreqHz = (count * 1000UL) / elapsed;

        lastMeasureTime = now;
    }

    // ------------------------------------------------------------------
    // 3. Affichage compact toutes les DISPLAY_PERIOD ms
    // ------------------------------------------------------------------
    if (now - lastDisplayTime >= DISPLAY_PERIOD) {
        // Calcul de la moyenne ADC
        unsigned int adcMoy = (adcCount > 0) ? (unsigned int)(adcSum / adcCount) : 0;

        // Affichage sur une seule ligne
        Serial.print("Freq: ");
        Serial.print(measuredFreqHz);
        Serial.print(" Hz | ");
        Serial.print(classifyFrequency(measuredFreqHz));
        Serial.print(" | ADC: min=");
        Serial.print(adcMin);
        Serial.print(" max=");
        Serial.print(adcMax);
        Serial.print(" moy=");
        Serial.println(adcMoy);

        // Remise à zéro des stats ADC pour la prochaine fenêtre
        adcMin   = 4095;
        adcMax   = 0;
        adcSum   = 0;
        adcCount = 0;

        lastDisplayTime = now;
    }
}
