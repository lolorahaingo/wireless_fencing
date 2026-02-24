// =============================================================================
// TEST GPIO 14 & 15 — Raspberry Pi Pico W
// =============================================================================
//
// Test simple pour verifier si les GPIO 14 et 15 fonctionnent.
//
// SEQUENCE :
//   1. GPIO 14 HIGH (3.3V) pendant 3 sec  → mesurer avec multimetre
//   2. GPIO 14 LOW  (0V)   pendant 3 sec  → mesurer avec multimetre
//   3. GPIO 15 HIGH (3.3V) pendant 3 sec  → mesurer avec multimetre
//   4. GPIO 15 LOW  (0V)   pendant 3 sec  → mesurer avec multimetre
//   5. Recommence
//
// MULTIMETRE : mode tension DC, pointe rouge sur le GPIO, pointe noire sur GND
//
// RESULTATS ATTENDUS :
//   - HIGH → ~3.3V  (pin OK)
//   - LOW  → ~0V    (pin OK)
//   - Toujours 0V en HIGH → pin mort ou bloque en entree
//   - Toujours 3.3V en LOW → pin bloque en HIGH
//   - Tension intermediaire → pin endommage
//
// LED INTEGREE :
//   - Clignote 1x → on teste GPIO 14
//   - Clignote 2x → on teste GPIO 15
// =============================================================================

#include <Arduino.h>

const uint PIN_A = 14;
const uint PIN_B = 15;

void clignote(int fois) {
  for (int i = 0; i < fois; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(150);
    digitalWrite(LED_BUILTIN, LOW);
    delay(150);
  }
  delay(300);
}

void testerPin(uint pin, const char* nom, int nbClignote) {
  // Annonce
  clignote(nbClignote);

  // --- HIGH ---
  Serial.print("[TEST] ");
  Serial.print(nom);
  Serial.println(" → HIGH (attendu: ~3.3V) — mesurez maintenant !");
  
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  delay(3000);

  // --- LOW ---
  Serial.print("[TEST] ");
  Serial.print(nom);
  Serial.println(" → LOW  (attendu: ~0V)   — mesurez maintenant !");
  
  digitalWrite(pin, LOW);
  delay(3000);
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("=========================================");
  Serial.println("  TEST GPIO 14 & 15");
  Serial.println("=========================================");
  Serial.println("  Multimetre en mode tension DC");
  Serial.println("  Pointe noire sur GND du Pico");
  Serial.println("  Pointe rouge sur le GPIO a tester");
  Serial.println("-----------------------------------------");
  Serial.println("  LED 1x cligno = test GPIO 14");
  Serial.println("  LED 2x cligno = test GPIO 15");
  Serial.println("  HIGH = 3 sec, LOW = 3 sec");
  Serial.println("=========================================");
  Serial.println();
}

void loop() {
  testerPin(PIN_A, "GPIO 14", 1);
  testerPin(PIN_B, "GPIO 15", 2);

  Serial.println("--- cycle termine, on recommence ---");
  Serial.println();
  delay(1000);
}
