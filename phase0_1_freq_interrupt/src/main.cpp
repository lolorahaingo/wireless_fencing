#include <Arduino.h>

// =============================================================================
// Phase 0 - Proof of Concept : Generation & Detection de Frequence
// Plateforme : Arduino Mega
// =============================================================================
//
// CABLAGE : Un seul fil entre Pin 9 et Pin 2 (interruption externe)
//
// =============================================================================

const int PIN_SIGNAL_OUT = 9;
const int PIN_FREQ_IN = 2;  // INT0 sur Mega

volatile unsigned long pulseCount = 0;
unsigned long lastTime = 0;
bool signalEnabled = true;

void countPulse() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_FREQ_IN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_FREQ_IN), countPulse, RISING);
  
  tone(PIN_SIGNAL_OUT, 25000);
  
  Serial.println("Phase 0 - Detection de Frequence");
  Serial.println("Cablage: Pin 9 --> Pin 2");
  Serial.println("Commande: 't' = toggle signal");
  
  lastTime = millis();
}

void loop() {
  if (Serial.available() && Serial.read() == 't') {
    signalEnabled = !signalEnabled;
    signalEnabled ? tone(PIN_SIGNAL_OUT, 25000) : noTone(PIN_SIGNAL_OUT);
    Serial.println(signalEnabled ? "Signal ON" : "Signal OFF");
  }
  
  // Mesure toutes les secondes
  if (millis() - lastTime >= 1000) {
    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();
    
    Serial.print("Frequence detectee: ");
    Serial.print(count);
    Serial.println(" Hz");
    
    lastTime = millis();
  }
}
