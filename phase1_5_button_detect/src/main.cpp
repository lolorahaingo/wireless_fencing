// =============================================================================
// Phase 1.5 — Détection du bouton + classification de touche
// Projet : Escrime sans fil
// =============================================================================
//
// RÔLE :
//   1. Génère Freq_NEUTRE (20 kHz) en continu sur la coque (ligne C)
//      via MOSFET 2N7000 + pull-up 100Ω vers VBUS
//   2. Détecte l'appui du bouton du fleuret (tête allemande, normalement fermé)
//      via filtre RC (10kΩ + 100nF) sur GP16 → digitalRead
//   3. Quand le bouton est pressé, mesure la fréquence sur GP2 (ligne B)
//      et classifie la touche (valide / neutre / blanche)
//
// CÂBLAGE :
//
//   === Génération Freq_NEUTRE (20 kHz) → ligne C ===
//
//           VBUS (5V) [pin 40]
//               │
//            [100Ω] pull-up
//               │
//               ├──── ligne C (vert) → coque/bâti
//               │
//           Drain (D)
//               │
//   GP14 ──[100Ω]── Gate (G)    2N7000
//               │
//           Source (S)
//               │
//           GND [pin 18]
//
//   === Détection fréquence + bouton (ligne B) ===
//
//                            [10kΩ]        [100nF]
//   Ligne B (bleu) ───┬──── [R] ────┬──── [C] ──── GND
//                     │              │
//                     │           GP16 [pin 21] (bouton, digitalRead)
//                     │
//                     ├──── GP2 (détection freq, interruption)
//                     │
//                   [10kΩ] pull-down
//                     │
//                    GND
//
// FONCTIONNEMENT :
//   - Au repos (bouton non pressé) : B↔C fermé, le signal 20 kHz de C
//     circule vers B. Le filtre RC lisse ce signal → GP16 = HIGH.
//     GP2 voit 20 kHz mais on l'ignore (bouton pas pressé).
//
//   - Bouton pressé : B↔C ouvert, plus de signal de C sur B.
//     Le condensateur se décharge → GP16 = LOW.
//     GP2 voit ce qui arrive par la pointe du fleuret :
//       → Freq_VALID adverse (25/40 kHz) = TOUCHE VALIDE
//       → Freq_NEUTRE (20 kHz)           = PAS DE LUMIÈRE (coque/piste)
//       → Rien (0 Hz)                    = TOUCHE BLANCHE (non-valide)
//
// =============================================================================

#include <Arduino.h>
#include <hardware/pwm.h>
#include <hardware/clocks.h>

// =============================================================================
// CONFIGURATION DES PINS
// =============================================================================

const uint PIN_PWM_NEUTRE = 14;   // GP14 : sortie PWM → MOSFET → ligne C (coque)
const int  PIN_FREQ_IN    = 2;    // GP2  : entrée interruption (ligne B, détection freq)
const int  PIN_BUTTON     = 16;   // GP16 : entrée digitalRead (ligne B via filtre RC)

// =============================================================================
// FRÉQUENCES DE RÉFÉRENCE (Hz)
// =============================================================================

const unsigned int FREQ_NEUTRE  = 20000;  // 20 kHz — coque, piste
const unsigned int FREQ_VALID_A = 25000;  // 25 kHz — cuirasse tireur A
const unsigned int FREQ_VALID_B = 40000;  // 40 kHz — cuirasse tireur B
const unsigned int TOLERANCE    = 2000;   // ±2 kHz

// =============================================================================
// PARAMÈTRES DE MESURE
// =============================================================================

const unsigned int MEASURE_WINDOW_MS = 50;    // fenêtre de comptage (ms)
const unsigned int DISPLAY_PERIOD_MS = 200;   // affichage série (ms)
const unsigned int DEBOUNCE_MS       = 5;     // anti-rebond bouton (ms)

// =============================================================================
// VARIABLES PARTAGÉES AVEC L'ISR
// =============================================================================

volatile unsigned long pulseCount = 0;

// =============================================================================
// VARIABLES D'ÉTAT
// =============================================================================

// Bouton
bool     buttonPressed      = false;   // état courant du bouton
bool     lastButtonState    = false;   // état précédent (pour détection de front)
unsigned long buttonChangeTime = 0;    // timestamp du dernier changement (debounce)

// Mesure de fréquence
unsigned long measuredFreqHz   = 0;
unsigned long lastMeasureTime  = 0;
bool          measuring        = false;  // true = on est en train de compter

// Dwell time (durée d'appui du bouton)
unsigned long buttonPressStart = 0;
unsigned long dwellTimeMs      = 0;

// Affichage
unsigned long lastDisplayTime  = 0;

// Statistiques de touche
unsigned long touchCount       = 0;

// =============================================================================
// ISR — compte chaque front montant sur GP2
// =============================================================================

void countPulse() {
    pulseCount++;
}

// =============================================================================
// Classification de la fréquence mesurée
// =============================================================================

const char* classifyFrequency(unsigned long freq) {
    if (freq < 500)
        return "AUCUNE (blanche)";
    if (freq >= FREQ_NEUTRE  - TOLERANCE && freq <= FREQ_NEUTRE  + TOLERANCE)
        return "NEUTRE  (20 kHz)";
    if (freq >= FREQ_VALID_A - TOLERANCE && freq <= FREQ_VALID_A + TOLERANCE)
        return "VALID_A (25 kHz)";
    if (freq >= FREQ_VALID_B - TOLERANCE && freq <= FREQ_VALID_B + TOLERANCE)
        return "VALID_B (40 kHz)";
    return "INCONNUE";
}

// =============================================================================
// Résultat de la touche (logique d'arbitrage simplifiée)
// =============================================================================

const char* touchResult(unsigned long freq) {
    if (freq < 500)
        return ">>> TOUCHE BLANCHE (non-valide) <<<";
    if (freq >= FREQ_NEUTRE  - TOLERANCE && freq <= FREQ_NEUTRE  + TOLERANCE)
        return "--- Pas de lumiere (coque/piste) ---";
    if (freq >= FREQ_VALID_A - TOLERANCE && freq <= FREQ_VALID_A + TOLERANCE)
        return "*** TOUCHE VALIDE sur tireur A ! ***";
    if (freq >= FREQ_VALID_B - TOLERANCE && freq <= FREQ_VALID_B + TOLERANCE)
        return "*** TOUCHE VALIDE sur tireur B ! ***";
    return "??? Frequence inconnue ???";
}

// =============================================================================
// Configuration du PWM hardware (signal carré exact)
// =============================================================================

uint pwmSliceNeutre = 0;

void setupPWM(uint pin, unsigned int freq) {
    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint sliceNum = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);  // 125 MHz
    uint32_t wrap = clock_freq / freq;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.0f);
    pwm_config_set_wrap(&config, wrap - 1);
    pwm_init(sliceNum, &config, false);

    // Duty cycle 50% → signal carré
    pwm_set_chan_level(sliceNum, pwm_gpio_to_channel(pin), wrap / 2);

    // Démarrer le PWM
    pwm_set_enabled(sliceNum, true);

    pwmSliceNeutre = sliceNum;
}

// =============================================================================
// Lecture du bouton avec anti-rebond
// =============================================================================

bool readButtonDebounced() {
    // GP16 : HIGH = bouton au repos (B↔C fermé, signal 20 kHz filtré)
    //         LOW = bouton pressé (B↔C ouvert, condensateur déchargé)
    bool rawState = !digitalRead(PIN_BUTTON);  // inversé : LOW = pressé → true

    if (rawState != lastButtonState) {
        buttonChangeTime = millis();
    }
    lastButtonState = rawState;

    if ((millis() - buttonChangeTime) >= DEBOUNCE_MS) {
        return rawState;
    }
    return buttonPressed;  // pas encore stabilisé, garder l'état précédent
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(3000);

    // --- LED intégrée ---
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // --- Génération Freq_NEUTRE sur GP14 (via MOSFET → ligne C) ---
    setupPWM(PIN_PWM_NEUTRE, FREQ_NEUTRE);

    // --- Détection fréquence sur GP2 (interruption RISING) ---
    pinMode(PIN_FREQ_IN, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_FREQ_IN), countPulse, RISING);

    // --- Détection bouton sur GP16 (filtre RC, digitalRead) ---
    pinMode(PIN_BUTTON, INPUT);  // pas de pull-up/pull-down interne, le RC s'en charge

    // --- En-tête ---
    Serial.println("=====================================================");
    Serial.println("  Phase 1.5 — Detection bouton + classification");
    Serial.println("=====================================================");
    Serial.println("  GP14 : PWM 20 kHz → MOSFET → ligne C (coque)");
    Serial.println("  GP2  : detection frequence (interruption RISING)");
    Serial.println("  GP16 : detection bouton (filtre RC, digitalRead)");
    Serial.println("-----------------------------------------------------");
    Serial.println("  Bouton au repos : GP16 = HIGH (signal 20 kHz filtre)");
    Serial.println("  Bouton presse   : GP16 = LOW  (condensateur decharge)");
    Serial.println("-----------------------------------------------------");
    Serial.println("  En attente d'appui sur le bouton du fleuret...");
    Serial.println("=====================================================");
    Serial.println();

    lastMeasureTime = millis();
    lastDisplayTime = millis();
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
    unsigned long now = millis();

    // -----------------------------------------------------------------
    // 1. Lire l'état du bouton (avec anti-rebond)
    // -----------------------------------------------------------------
    bool currentPressed = readButtonDebounced();

    // -----------------------------------------------------------------
    // 2. Détection du front descendant (bouton vient d'être pressé)
    // -----------------------------------------------------------------
    if (currentPressed && !buttonPressed) {
        // Front : bouton vient d'être pressé
        buttonPressStart = now;
        measuring = true;

        // Reset du compteur pour une mesure propre
        noInterrupts();
        pulseCount = 0;
        interrupts();
        lastMeasureTime = now;

        Serial.println("[BOUTON] Presse ! Mesure en cours...");
    }

    // -----------------------------------------------------------------
    // 3. Détection du front montant (bouton vient d'être relâché)
    // -----------------------------------------------------------------
    if (!currentPressed && buttonPressed) {
        // Front : bouton vient d'être relâché
        dwellTimeMs = now - buttonPressStart;
        measuring = false;

        // Dernière mesure
        noInterrupts();
        unsigned long count = pulseCount;
        pulseCount = 0;
        interrupts();

        unsigned long elapsed = now - lastMeasureTime;
        if (elapsed > 0) {
            measuredFreqHz = (count * 1000UL) / elapsed;
        }

        // Afficher le résultat de la touche
        touchCount++;
        Serial.println("-----------------------------------------------------");
        Serial.print("[TOUCHE #");
        Serial.print(touchCount);
        Serial.print("]  Freq: ");
        Serial.print(measuredFreqHz);
        Serial.print(" Hz | ");
        Serial.print(classifyFrequency(measuredFreqHz));
        Serial.print(" | Dwell: ");
        Serial.print(dwellTimeMs);
        Serial.println(" ms");

        Serial.print("  Resultat: ");
        Serial.println(touchResult(measuredFreqHz));

        // Vérification dwell time FIE (15 ms minimum)
        if (dwellTimeMs < 15) {
            Serial.println("  /!\\ Dwell time < 15 ms (insuffisant pour la FIE)");
        }
        Serial.println("-----------------------------------------------------");
        Serial.println();
    }

    // -----------------------------------------------------------------
    // 4. Pendant que le bouton est pressé : mesure continue
    // -----------------------------------------------------------------
    if (measuring && (now - lastMeasureTime >= MEASURE_WINDOW_MS)) {
        noInterrupts();
        unsigned long count = pulseCount;
        pulseCount = 0;
        interrupts();

        unsigned long elapsed = now - lastMeasureTime;
        measuredFreqHz = (count * 1000UL) / elapsed;
        lastMeasureTime = now;
    }

    // -----------------------------------------------------------------
    // 5. Mettre à jour l'état du bouton
    // -----------------------------------------------------------------
    buttonPressed = currentPressed;

    // -----------------------------------------------------------------
    // 6. Affichage périodique de l'état
    // -----------------------------------------------------------------
    if (now - lastDisplayTime >= DISPLAY_PERIOD_MS) {
        lastDisplayTime = now;

        if (buttonPressed) {
            // Bouton pressé : afficher la fréquence en temps réel
            Serial.print("[MESURE] Freq: ");
            Serial.print(measuredFreqHz);
            Serial.print(" Hz | ");
            Serial.print(classifyFrequency(measuredFreqHz));
            Serial.print(" | Dwell: ");
            Serial.print(now - buttonPressStart);
            Serial.println(" ms");

            // LED clignote pendant la mesure
            digitalWrite(LED_BUILTIN, (now / 100) % 2 ? HIGH : LOW);
        } else {
            // Bouton au repos : affichage discret
            Serial.print("[REPOS] GP16=");
            Serial.print(digitalRead(PIN_BUTTON) ? "HIGH" : "LOW");
            Serial.print(" | GP2 freq: ");
            Serial.print(measuredFreqHz);
            Serial.println(" Hz");

            // LED allumée fixe au repos
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }
}
