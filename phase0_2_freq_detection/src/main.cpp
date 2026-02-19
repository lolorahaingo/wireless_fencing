#include <Arduino.h>

// =============================================================================
// Phase 0.2 - Detection et Distinction de 3 Frequences
// Plateforme : Arduino Mega
// =============================================================================
//
// CABLAGE : Pin 9 --> Pin 2 (interruption externe)
//
// Methode : Comptage d'impulsions + classification par plage
// =============================================================================

const int PIN_SIGNAL_OUT = 9;
const int PIN_FREQ_IN = 2;  // INT0 sur Mega

// Frequences cibles (Hz) - choisies pour etre precises avec timer Arduino
// 16MHz / (2 * 8 * (1+OCR)) -> OCR entier = frequence precise
// 20000 Hz -> OCR=49  -> 16M/(2*8*50) = 20000 exact
// 25000 Hz -> OCR=39  -> 16M/(2*8*40) = 25000 exact  
// 32000 Hz -> OCR=30.25 -> pas exact, on prend 31250 Hz (OCR=31)
// Ou: 16000 Hz -> OCR=62 -> 16M/(2*8*63) = 15873 Hz (pas exact)
//
// Meilleures frequences "exactes" avec prescaler 8:
// 20000, 25000, 40000, 16000, 10000, 8000, 5000, 4000, 2000, 1000
//
// On garde 20k et 25k, et on change 30k -> 40k ou 16k

const unsigned int FREQ_NEUTRE = 16000;   // Exact: 16MHz/(2*8*63)=15873 ~16k
const unsigned int FREQ_VALID_A = 25000;  // Exact: 16MHz/(2*8*40)=25000
const unsigned int FREQ_VALID_B = 40000;  // Exact: 16MHz/(2*8*25)=40000

// Tolerance pour la detection (+/- Hz)
const unsigned int TOLERANCE = 1000;

// Frequence generee (modifiable)
unsigned int freqGenerated = FREQ_VALID_A;

volatile unsigned long pulseCount = 0;
unsigned long lastMeasureTime = 0;
unsigned long lastDisplayTime = 0;
unsigned long lastFreqHz = 0;
bool signalEnabled = true;

// Periode de mesure (ms) - 15ms = dwell time FIE minimum
const unsigned int MEASURE_PERIOD = 15;

// Periode d'affichage (ms) - pour lisibilite du terminal
const unsigned int DISPLAY_PERIOD = 500;

void countPulse() {
  pulseCount++;
}

// Determine quelle frequence est detectee
const char* classifyFrequency(unsigned long freq) {
  if (freq >= FREQ_NEUTRE - TOLERANCE && freq <= FREQ_NEUTRE + TOLERANCE) {
    return "NEUTRE (16kHz)";
  }
  if (freq >= FREQ_VALID_A - TOLERANCE && freq <= FREQ_VALID_A + TOLERANCE) {
    return "VALID_A (25kHz)";
  }
  if (freq >= FREQ_VALID_B - TOLERANCE && freq <= FREQ_VALID_B + TOLERANCE) {
    return "VALID_B (40kHz)";
  }
  if (freq < 100) {
    return "AUCUNE";
  }
  return "INCONNUE";
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_FREQ_IN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_FREQ_IN), countPulse, RISING);
  
  tone(PIN_SIGNAL_OUT, freqGenerated);
  
  Serial.println("===========================================");
  Serial.println("Phase 0.2 - Detection Multi-Frequences");
  Serial.println("===========================================");
  Serial.println("Cablage: Pin 9 --> Pin 2");
  Serial.println();
  Serial.println("Frequences optimisees pour precision:");
  Serial.println("  1 = 16 kHz (NEUTRE)");
  Serial.println("  2 = 25 kHz (VALID_A)");
  Serial.println("  3 = 40 kHz (VALID_B)");
  Serial.println();
  Serial.println("Commandes: t=toggle, 1/2/3=frequence");
  Serial.println();
  
  lastMeasureTime = millis();
  lastDisplayTime = millis();
}

void loop() {
  // Commandes serie
  while (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '\n' || cmd == '\r') continue;
    
    switch (cmd) {
      case 't': case 'T':
        signalEnabled = !signalEnabled;
        if (signalEnabled) {
          tone(PIN_SIGNAL_OUT, freqGenerated);
          Serial.println(">>> Signal ON <<<");
        } else {
          noTone(PIN_SIGNAL_OUT);
          Serial.println(">>> Signal OFF <<<");
        }
        break;
      case '1':
        freqGenerated = FREQ_NEUTRE;
        tone(PIN_SIGNAL_OUT, freqGenerated);
        signalEnabled = true;
        Serial.println(">>> Genere: 16 kHz (NEUTRE) <<<");
        break;
      case '2':
        freqGenerated = FREQ_VALID_A;
        tone(PIN_SIGNAL_OUT, freqGenerated);
        signalEnabled = true;
        Serial.println(">>> Genere: 25 kHz (VALID_A) <<<");
        break;
      case '3':
        freqGenerated = FREQ_VALID_B;
        tone(PIN_SIGNAL_OUT, freqGenerated);
        signalEnabled = true;
        Serial.println(">>> Genere: 40 kHz (VALID_B) <<<");
        break;
    }
  }
  
  // Mesure toutes les MEASURE_PERIOD ms (rapide, 15ms)
  if (millis() - lastMeasureTime >= MEASURE_PERIOD) {
    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();
    
    // Convertir en Hz
    lastFreqHz = count * (1000 / MEASURE_PERIOD);
    
    lastMeasureTime = millis();
  }
  
  // Affichage toutes les DISPLAY_PERIOD ms (lent, 500ms)
  if (millis() - lastDisplayTime >= DISPLAY_PERIOD) {
    const char* detected = classifyFrequency(lastFreqHz);
    
    Serial.print("Freq: ");
    Serial.print(lastFreqHz);
    Serial.print(" Hz  ->  ");
    Serial.println(detected);
    
    lastDisplayTime = millis();
  }
}
