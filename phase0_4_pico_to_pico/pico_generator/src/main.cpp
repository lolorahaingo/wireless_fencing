// =============================================================================
// GÉNÉRATEUR DE SIGNAL — Raspberry Pi Pico W
// Projet : Escrime sans fil — Phase 0.4 (Pico → Pico, sans GND commun)
// =============================================================================
//
// RÔLE : Génère un signal carré sur GPIO 15 via PWM hardware du RP2040.
//        tone() est imprécis sur le Pico W aux hautes fréquences (~14-16 kHz
//        au lieu de 20 kHz). Le PWM hardware est exact.
//
// CÂBLAGE :
//   GPIO 15  → un seul fil vers GPIO 2 du Pico récepteur
//   GND      → NE PAS connecter au récepteur
//   VBUS     → alimentation 5V depuis adaptateur secteur via multiprise
//
// LED INTÉGRÉE :
//   LED allumée en continu  → signal actif
//   LED clignotante lente   → signal désactivé
//
// COMMANDES SÉRIE (optionnel, si FTDI branché) :
//   t → bascule signal ON/OFF
//   1 → 20 kHz (NEUTRE)
//   2 → 25 kHz (VALID_A)
//   3 → 40 kHz (VALID_B)
// =============================================================================

#include <Arduino.h>
#include <hardware/pwm.h>
#include <hardware/clocks.h>

// --- Broche de sortie du signal ---
const uint PIN_SIGNAL_OUT = 15;

// --- Fréquences disponibles ---
const unsigned int FREQ_NEUTRE  = 20000;  // 20 kHz — exact
const unsigned int FREQ_VALID_A = 25000;  // 25 kHz — exact
const unsigned int FREQ_VALID_B = 40000;  // 40 kHz — exact

// --- État courant ---
bool          signalActif  = true;
unsigned int  freqActuelle = FREQ_NEUTRE;
uint          sliceNum     = 0;

// --- Timing ---
unsigned long dernierClignotement = 0;
bool          etatLed             = false;
unsigned long dernierAffichage    = 0;

// ---------------------------------------------------------------------------
// Configure le PWM hardware pour une fréquence exacte (signal carré 50%)
// RP2040 clock = 125 MHz
// freq = 125000000 / (diviser * wrap)
// Pour un signal carré : duty = wrap / 2
// ---------------------------------------------------------------------------
void configurerPWM(unsigned int freq) {
  // Calcul du wrap pour cette fréquence (diviser = 1)
  // wrap = clock_freq / freq
  uint32_t clock_freq = clock_get_hz(clk_sys);  // 125 MHz
  uint32_t wrap = clock_freq / freq;

  // Configurer le slice PWM
  sliceNum = pwm_gpio_to_slice_num(PIN_SIGNAL_OUT);

  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, 1.0f);       // pas de division, pleine vitesse
  pwm_config_set_wrap(&config, wrap - 1);      // période
  pwm_init(sliceNum, &config, false);          // ne pas démarrer encore

  // Duty cycle 50% → signal carré
  pwm_set_chan_level(sliceNum, pwm_gpio_to_channel(PIN_SIGNAL_OUT), wrap / 2);
}

// ---------------------------------------------------------------------------
void demarrerPWM() {
  pwm_set_enabled(sliceNum, true);
}

void arreterPWM() {
  pwm_set_enabled(sliceNum, false);
  gpio_put(PIN_SIGNAL_OUT, 0);  // forcer le pin à LOW
}

// ---------------------------------------------------------------------------
const char* nomFreq(unsigned int freq) {
  if (freq == FREQ_NEUTRE)  return "NEUTRE  (20 kHz)";
  if (freq == FREQ_VALID_A) return "VALID_A (25 kHz)";
  if (freq == FREQ_VALID_B) return "VALID_B (40 kHz)";
  return "INCONNUE";
}

// ---------------------------------------------------------------------------
void activerSignal() {
  configurerPWM(freqActuelle);
  demarrerPWM();
  digitalWrite(LED_BUILTIN, HIGH);
  signalActif = true;
  Serial.print("[SIGNAL ON]  Frequence : ");
  Serial.println(nomFreq(freqActuelle));
}

void desactiverSignal() {
  arreterPWM();
  signalActif = false;
  Serial.println("[SIGNAL OFF]");
}

void changerFrequence(unsigned int nouvelleFreq) {
  freqActuelle = nouvelleFreq;
  if (signalActif) {
    configurerPWM(freqActuelle);
    demarrerPWM();
  }
  Serial.print("[FREQ] ");
  Serial.println(nomFreq(freqActuelle));
}

// ---------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(3000);

  // Configurer GPIO 15 comme sortie PWM
  gpio_set_function(PIN_SIGNAL_OUT, GPIO_FUNC_PWM);
  pinMode(LED_BUILTIN, OUTPUT);

  // Démarrage du signal à 20 kHz
  activerSignal();

  Serial.println("=========================================");
  Serial.println("  GENERATEUR — Pico W  (phase0_4)");
  Serial.println("  PWM hardware (exact, pas tone())");
  Serial.println("=========================================");
  Serial.println("  Broche signal : GPIO 15");
  Serial.println("  Alimentation  : adaptateur secteur");
  Serial.println("  GND commun    : NON");
  Serial.println("-----------------------------------------");
  Serial.println("  t -> ON/OFF | 1=20k | 2=25k | 3=40k");
  Serial.println("=========================================");
  Serial.print("  Etat initial : ");
  Serial.println(nomFreq(freqActuelle));
  Serial.println();

  dernierAffichage = millis();
}

// ---------------------------------------------------------------------------
// LOOP
// ---------------------------------------------------------------------------
void loop() {

  // --- Commandes série ---
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    switch (cmd) {
      case 't':
        signalActif ? desactiverSignal() : activerSignal();
        break;
      case '1': changerFrequence(FREQ_NEUTRE);  break;
      case '2': changerFrequence(FREQ_VALID_A); break;
      case '3': changerFrequence(FREQ_VALID_B); break;
    }
  }

  // --- LED : allumée si signal actif, clignote sinon ---
  if (!signalActif) {
    unsigned long now = millis();
    if (now - dernierClignotement >= 500) {
      dernierClignotement = now;
      etatLed = !etatLed;
      digitalWrite(LED_BUILTIN, etatLed ? HIGH : LOW);
    }
  }

  // --- Affichage périodique ---
  unsigned long now = millis();
  if (now - dernierAffichage >= 2000) {
    dernierAffichage = now;
    Serial.print("[STATUS] ");
    Serial.print(signalActif ? "ON" : "OFF");
    Serial.print(" | ");
    Serial.println(nomFreq(freqActuelle));
  }
}
