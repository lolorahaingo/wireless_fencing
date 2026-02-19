/**
 * Phase 0.3 - Générateur de signal sans GND commun
 * ===================================================
 * Rôle : Arduino Mega génère un signal carré sur Pin 9.
 *        Le signal est capté par un Raspberry Pi Pico 2W via UN SEUL fil,
 *        sans GND commun entre les deux cartes.
 *
 * Câblage :
 *   - Pin 9 (Mega) --> Pin d'entrée (Pico 2W)
 *   - Aucun GND commun
 *   - Mega alimenté par adaptateur secteur (pas USB)
 *
 * Commandes Serial (optionnel, via FTDI) :
 *   't' : toggle signal ON/OFF
 *   '1' : fréquence NEUTRE  → 16 kHz
 *   '2' : fréquence VALID_A → 25 kHz
 *   '3' : fréquence VALID_B → 40 kHz
 */

#include <Arduino.h>

// --- Constantes ---
const int PIN_SIGNAL_OUT = 9;  // Pin de sortie du signal (Timer 2 sur Mega)

// Fréquences disponibles (en Hz)
// Fréquences choisies pour être EXACTES avec le timer du Mega (prescaler 8) :
// 20000 Hz → OCR=49 → 16M/(2*8*50)  = 20000 exact
// 25000 Hz → OCR=39 → 16M/(2*8*40)  = 25000 exact
// 40000 Hz → OCR=24 → 16M/(2*8*25)  = 40000 exact
// (16000 Hz n'est PAS exact → 15873 Hz réel, donc on utilise 20 kHz)
const unsigned int FREQ_NEUTRE  = 20000;  // Signal neutre (exact)
const unsigned int FREQ_VALID_A = 25000;  // Touche valide A (exact)
const unsigned int FREQ_VALID_B = 40000;  // Touche valide B (exact)

// --- Variables globales ---
bool          signalEnabled  = true;
unsigned int  freqActuelle   = FREQ_NEUTRE;
unsigned long dernierAffichage = 0;

// Noms des fréquences pour l'affichage Serial
const char* nomFreq(unsigned int freq) {
  if (freq == FREQ_NEUTRE)  return "NEUTRE  (20 kHz)";
  if (freq == FREQ_VALID_A) return "VALID_A (25 kHz)";
  if (freq == FREQ_VALID_B) return "VALID_B (40 kHz)";
  return "INCONNUE";
}

// --- Setup ---
void setup() {
  // Serial optionnel : utile si on branche un FTDI plus tard
  Serial.begin(115200);

  pinMode(PIN_SIGNAL_OUT, OUTPUT);

  // Démarrage du signal à la fréquence neutre
  tone(PIN_SIGNAL_OUT, freqActuelle);

  Serial.println("=== Phase 0.3 - Generateur sans GND commun ===");
  Serial.println("Commandes : t=toggle | 1=20kHz | 2=25kHz | 3=40kHz");
  Serial.print("Signal demarre : ");
  Serial.println(nomFreq(freqActuelle));

  dernierAffichage = millis();
}

// --- Loop ---
void loop() {
  // --- Lecture des commandes Serial ---
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd == 't') {
      // Toggle ON/OFF
      signalEnabled = !signalEnabled;
      if (signalEnabled) {
        tone(PIN_SIGNAL_OUT, freqActuelle);
        Serial.print("Signal ON  - ");
        Serial.println(nomFreq(freqActuelle));
      } else {
        noTone(PIN_SIGNAL_OUT);
        Serial.println("Signal OFF");
      }
    }
    else if (cmd == '1') {
      // Fréquence NEUTRE
      freqActuelle = FREQ_NEUTRE;
      if (signalEnabled) tone(PIN_SIGNAL_OUT, freqActuelle);
      Serial.print("Frequence : ");
      Serial.println(nomFreq(freqActuelle));
    }
    else if (cmd == '2') {
      // Fréquence VALID_A
      freqActuelle = FREQ_VALID_A;
      if (signalEnabled) tone(PIN_SIGNAL_OUT, freqActuelle);
      Serial.print("Frequence : ");
      Serial.println(nomFreq(freqActuelle));
    }
    else if (cmd == '3') {
      // Fréquence VALID_B
      freqActuelle = FREQ_VALID_B;
      if (signalEnabled) tone(PIN_SIGNAL_OUT, freqActuelle);
      Serial.print("Frequence : ");
      Serial.println(nomFreq(freqActuelle));
    }
  }

  // --- Affichage périodique toutes les 2 secondes ---
  if (millis() - dernierAffichage >= 2000) {
    if (signalEnabled) {
      Serial.print("Signal actif : ");
      Serial.println(nomFreq(freqActuelle));
    } else {
      Serial.println("Signal inactif (OFF)");
    }
    dernierAffichage = millis();
  }
}
