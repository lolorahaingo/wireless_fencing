#include <Arduino.h>

// ============================================================
// Phase 0.3 — Détection sans GND commun
// Récepteur : Raspberry Pi Pico W (RP2040)
// ============================================================
//
// OBJECTIF DU TEST :
//   Vérifier si un signal carré AC généré par un Arduino Mega
//   peut être détecté par un Pico W qui ne partage PAS de GND
//   avec le Mega. Un seul fil relie les deux cartes.
//
// CÂBLAGE :
//   - UN SEUL fil obligatoire  : Pin 9 (Mega) → GPIO 2 (Pico)
//   - Fil optionnel (ADC)      : Pin 9 (Mega) → GPIO 26 / ADC0 (Pico)
//   - PAS de fil GND entre les deux cartes
//   - Mega  : alimenté par adaptateur secteur
//   - Pico  : alimenté par USB sur PC (Serial Monitor)
//
// MÉTHODE DE DÉTECTION :
//   Comptage d'impulsions par interruption sur front montant.
//   La fréquence est calculée sur une fenêtre de 50 ms.
//   Affichage toutes les 500 ms avec classification et lecture ADC.
// ============================================================

// --- Pins ---
const int PIN_INTERRUPT = 2;   // GPIO 2 — entrée signal (interruption)
const int PIN_ADC       = 26;  // GPIO 26 / ADC0 — lecture analogique optionnelle

// --- Fréquences cibles (Hz) ---
// Fréquences exactes du Mega (prescaler 8, OCR entier)
const unsigned int FREQ_NEUTRE  = 20000;  // 20 kHz exact
const unsigned int FREQ_VALID_A = 25000;  // 25 kHz exact
const unsigned int FREQ_VALID_B = 40000;  // 40 kHz exact
const unsigned int TOLERANCE    = 2000;   // ±2 kHz

// --- Périodes de mesure et d'affichage ---
const unsigned int MEASURE_PERIOD  = 5;   // ms — fenêtre de comptage
const unsigned int DISPLAY_PERIOD  = 500;  // ms — rafraîchissement Serial

// --- Variables partagées avec l'ISR ---
volatile unsigned long pulseCount = 0;  // compteur d'impulsions (ISR)

// --- Variables de mesure ---
unsigned long lastMeasureTime  = 0;
unsigned long lastDisplayTime  = 0;
unsigned long measuredFreqHz   = 0;  // fréquence calculée sur la dernière fenêtre

// --- Variables ADC (accumulées entre deux affichages) ---
unsigned int adcMin   = 4095;
unsigned int adcMax   = 0;
unsigned long adcSum  = 0;
unsigned int adcCount = 0;

// ============================================================
// ISR — Comptage des fronts montants
// ============================================================
void countPulse() {
    pulseCount++;
}

// ============================================================
// Classification de la fréquence mesurée
// ============================================================
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

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(3000);  // laisser le temps au Serial USB de s'initialiser

    // LED intégrée pour feedback visuel
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Résolution ADC 12 bits (0–4095) au lieu des 10 bits par défaut
    analogReadResolution(12);

    // Entrée interruption — PAS de pull-up ni pull-down
    // On veut observer le signal brut sans biaiser le test
    pinMode(PIN_INTERRUPT, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(PIN_INTERRUPT), countPulse, RISING);

    // --- Header d'information ---
    Serial.println();
    Serial.println("============================================================");
    Serial.println("  Phase 0.3 — Detection sans GND commun");
    Serial.println("  Recepteur : Raspberry Pi Pico W");
    Serial.println("============================================================");
    Serial.println("  CABLAGE ATTENDU :");
    Serial.println("    [OBLIGATOIRE] Pin 9 (Mega) -> GPIO 2  (Pico) — interruption");
    Serial.println("    [OPTIONNEL]   Pin 9 (Mega) -> GPIO 26 (Pico) — lecture ADC");
    Serial.println("    Pas de fil GND entre les deux cartes.");
    Serial.println("  METHODE : comptage d'impulsions par interruption (RISING)");
    Serial.println("  Fenetre de mesure : 50 ms | Affichage : 500 ms");
    Serial.println("============================================================");
    Serial.println();

    lastMeasureTime = millis();
    lastDisplayTime = millis();
}

// ============================================================
// Loop
// ============================================================
void loop() {
    unsigned long now = millis();

    // ----------------------------------------------------------
    // 1. Lecture ADC en continu (accumulée pour stats)
    // ----------------------------------------------------------
    unsigned int adcVal = (unsigned int)analogRead(PIN_ADC);
    if (adcVal < adcMin) adcMin = adcVal;
    if (adcVal > adcMax) adcMax = adcVal;
    adcSum += adcVal;
    adcCount++;

    // ----------------------------------------------------------
    // 2. Calcul de la fréquence toutes les MEASURE_PERIOD ms
    // ----------------------------------------------------------
    if (now - lastMeasureTime >= MEASURE_PERIOD) {
        // Lire et remettre à zéro le compteur de manière atomique
        noInterrupts();
        unsigned long count = pulseCount;
        pulseCount = 0;
        interrupts();

        unsigned long elapsed = now - lastMeasureTime;
        lastMeasureTime = now;

        // Fréquence = impulsions / durée_en_secondes
        measuredFreqHz = (count * 1000UL) / elapsed;
    }

    // ----------------------------------------------------------
    // 3. Affichage toutes les DISPLAY_PERIOD ms
    // ----------------------------------------------------------
    if (now - lastDisplayTime >= DISPLAY_PERIOD) {
        lastDisplayTime = now;

        // Calcul des stats ADC sur la période d'affichage
        unsigned int adcMoy = (adcCount > 0) ? (unsigned int)(adcSum / adcCount) : 0;

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

        // Remise à zéro des accumulateurs ADC
        adcMin   = 4095;
        adcMax   = 0;
        adcSum   = 0;
        adcCount = 0;
    }
}
