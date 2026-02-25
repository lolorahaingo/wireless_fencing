# Projet Escrime Sans Fil (Wireless Fencing)

## Contexte & Objectif

Concevoir un systeme d'arbitrage electronique sans fil pour le **fleuret** (escrime),
a base de **Raspberry Pi Pico W**, qui remplace le systeme filaire traditionnel.

La problematique principale du sans-fil est l'**absence de masse commune** entre les
deux tireurs. La solution retenue est la **detection de frequence** : chaque tireur
porte un Pico W qui genere des signaux carres AC sur ses surfaces conductrices (cuirasse
et coque du fleuret). Le fleuret adverse detecte ces frequences au contact, ce qui
permet d'identifier la nature de la touche sans connexion physique entre les tireurs.

---

## Decouvertes & Validations Cles

### Detection sans GND commun : VALIDEE (Phase 0.3 + 0.4)

**Phase 0.3** (Mega 5V → Pico W) : Arduino Mega (adaptateur secteur) genere 20 kHz
sur Pin 9, Pico W (USB sur Mac batterie) detecte sur GPIO 2. Un seul fil, aucun GND
commun. Resultat : ~18500-19700 Hz detecte (~95% des fronts). Le signal 5V du Mega
a assez d'amplitude pour franchir le seuil du GPIO recepteur.

**Phase 0.4** (Pico W 3.3V → Pico W) : Deux Pico W, alimentations separees (secteur
vs USB Mac). Resultat initial : seulement ~16 kHz detecte (~80% des fronts). Le signal
3.3V est trop faible sans GND commun pour declencher tous les fronts montants.

**Solution : resistance pull-down 10kΩ** sur le pin recepteur (entre GPIO 2 et GND
du Pico recepteur). Cette resistance tire le pin vers le bas, augmentant l'excursion
du signal couple et permettant au GPIO de detecter tous les fronts.

**Resultat avec pull-down 10kΩ** : 20 020 Hz detecte — precision quasi parfaite,
detection instantanee, equivalent au test avec GND commun.

| Test | Signal | Sans pull-down | Avec pull-down 10kΩ |
|------|--------|----------------|---------------------|
| Mega 5V → Pico | 20 kHz | ~19 kHz (95%) | non teste |
| Pico 3.3V → Pico | 20 kHz | ~16 kHz (80%) | **20 020 Hz (100%)** |
| Pico 3.3V → Pico (GND commun) | 20 kHz | 20 000 Hz exact | - |

**Implications** :
- L'hypothese fondamentale du projet est validee
- Le signal AC traverse un contact physique sans masse commune
- Une resistance pull-down 10kΩ cote recepteur est NECESSAIRE pour le Pico 3.3V
- Le couplage capacitif parasite suffit comme chemin de retour

### Charge capacitive de la cuirasse : RESOLU (Phase 1)

**Probleme** : Le GPIO du Pico (3.3V, ~12 mA max) ne peut pas driver la charge
capacitive de la cuirasse a 20 kHz. La cuirasse est une grande surface conductrice
qui forme un condensateur parasite avec tout ce qui l'entoure (siege, sol, corps).

**Tests sans buffer (GPIO direct)** :

| Test | Resultat |
|------|----------|
| Pince croco ↔ fiche directe (sans cuirasse) | 20 kHz exact ✅ |
| Cuirasse posee sur siege plastique | 5-7 kHz fluctuant ❌ |
| Cuirasse suspendue en l'air | 10-19 kHz fluctuant ❌ |

**Solution : MOSFET 2N7000 + pull-up 100Ω vers VBUS (5V)**

La pull-up 10kΩ initialement prevue etait trop elevee — le courant (0.5 mA) ne
suffisait pas a recharger la capacitance parasite entre chaque front. Avec 100Ω
(50 mA max), les fronts montants sont assez rapides pour 20 kHz.

```
Circuit buffer valide :

        VBUS (5V)
            │
         [100Ω]  pull-up (10kΩ trop faible, 100Ω necessaire)
            │
            ├──────── vers cuirasse (fil de corps, ligne A ou C)
            │
        Drain (D)
            │
  GPIO ──[100Ω]── Gate (G)    2N7000 (MOSFET N-channel)
            │
        Source (S)
            │
          GND Pico
```

**Tests avec buffer MOSFET + pull-up 100Ω (cuirasse sur siege)** :

Detection via fleuret + fil de corps touchant la cuirasse :
- **20 000 - 20 200 Hz** sur la majorite des mesures → detection parfaite ✅
- Quelques mesures a 0 Hz → perte de contact physique (normal pour test a la main)
- Quelques mesures aberrantes (12 kHz, 24 kHz) → transitions de contact
  (etablissement/rupture pendant la fenetre de mesure)
- Le signal est detecte meme **bouton non presse** → comportement normal car
  B est connecte a la pointe en permanence, le bouton (normalement ferme) relie
  B a C mais ne bloque pas le signal vers GP2. La logique de filtrage par l'etat
  du bouton (GP20) est necessaire cote software.

**Resultat** : Le buffer MOSFET 2N7000 + pull-up 100Ω vers VBUS resout
completement le probleme de charge capacitive. Detection fiable a 20 kHz
a travers la cuirasse posee sur un siege, via fleuret + fil de corps.

**Historique des pull-up testees** :
- 10kΩ : insuffisant — signal 20-50 Hz cote drain (front montant trop lent)
- **100Ω : fonctionne** — 20 kHz detecte parfaitement

### Generation de signal sur Pico W : PWM hardware (pas tone())

`tone()` sur le Pico W (framework Earlephilhower) est imprecis aux hautes frequences
(~14-16 kHz au lieu de 20 kHz demande). Le **PWM hardware du RP2040** est exact :

```
Clock RP2040 = 125 MHz
freq = 125000000 / wrap   (diviser = 1)
20 kHz → wrap = 6250 → exact
25 kHz → wrap = 5000 → exact
40 kHz → wrap = 3125 → exact
```

Utiliser `hardware/pwm.h` avec `pwm_init()` au lieu de `tone()`.

### Methode de detection : Comptage par interruption (pas Goertzel)

**Goertzel est inadapte** pour ce projet : le signal genere est un signal carre
(pas sinusoidal), et l'energie est repartie sur les harmoniques. Le comptage
d'impulsions par interruption sur front montant est plus simple, plus fiable,
et parfaitement adapte aux signaux carres.

**Methode retenue** : `attachInterrupt(pin, callback, RISING)` + comptage sur
une fenetre temporelle (50 ms). La frequence = nombre_impulsions * (1000 / duree_ms).
Classification par plage de tolerance (±2 kHz).

### Frequences exactes

Les frequences doivent etre exactes sur les deux plateformes :

| Frequence | Usage | Mega (prescaler 8) | Pico W (PWM hardware) |
|-----------|-------|---------------------|-----------------------|
| **20 000 Hz** | Freq_NEUTRE | OCR=49 → exact | wrap=6250 → exact |
| **25 000 Hz** | Freq_VALID_A | OCR=39 → exact | wrap=5000 → exact |
| **40 000 Hz** | Freq_VALID_B | OCR=24 → exact | wrap=3125 → exact |

Sur le Mega : `tone(pin, freq)` fonctionne car le timer est precis.
Sur le Pico W : utiliser le **PWM hardware** (`hardware/pwm.h`), pas `tone()`
qui est imprecis aux hautes frequences (~14-16 kHz au lieu de 20 kHz).

### Materiel : Pico W (RP2040), pas Pico 2W (RP2350)

Les cartes disponibles sont des **Raspberry Pi Pico W** (chip RP2040, Cortex-M0+),
et non des Pico 2W (chip RP2350, Cortex-M33). La distinction est importante pour
PlatformIO :
- Board ID : `rpipicow` (pas `rpipico2w`)
- Plateforme : `https://github.com/maxgerhardt/platform-raspberrypi.git`
- Core : `earlephilhower` (obligatoire pour attachInterrupt sur tous les GPIO,
  analogReadResolution, Serial USB, etc.)
- Upload : `picotool`
- La plateforme officielle `raspberrypi` de PlatformIO ne supporte PAS le Pico W

---

## Materiel Disponible

| Element                        | Statut         |
|--------------------------------|----------------|
| Arduino Mega                   | Disponible     |
| Raspberry Pi Pico W x2        | Disponible     |
| Fleuret electrique (tete allemande) | Disponible |
| Fil de corps                   | Disponible     |
| Cuirasse / Lame (FIE standard) | Disponible    |
| Connecteurs fleuret/fil de corps pour Arduino | Disponible |
| Resistances et condensateurs   | Disponible     |
| Piste metallique               | NON disponible (sujet separe) |
| Oscilloscope / analyseur logique | NON disponible |
| 3eme Pico W (central)         | A commander    |

---

## Specifications FIE (Fleuret)

| Parametre                          | Valeur FIE                |
|------------------------------------|---------------------------|
| Temps de contact minimum (dwell)   | 15 ms (+/- 0.5 ms)       |
| Temps de blocage (lockout/cutout)  | 300-350 ms                |
| Poids minimum sur la pointe        | 500 g (4.90 N) - mecanique, non pertinent pour l'electronique |
| Temps de blocage epee (reference)  | 40 ms                     |

> Le lockout time est le temps entre la premiere touche enregistree et le moment
> ou le systeme se ferme. Si l'autre tireur touche dans cette fenetre, c'est une
> double touche. La latence de communication doit etre bien inferieure a 300 ms.

---

## Circuit Electrique du Fleuret — Systeme Filaire Standard

### Les 3 lignes du fil de corps (convention FIE)

| Ligne | Couleur schema | Nom FIE | Connecte a                                      |
|-------|----------------|---------|--------------------------------------------------|
| A     | Rouge          | Lame    | Lame/cuirasse de l'ADVERSAIRE (via appareil)     |
| B     | Bleu           | Arme    | Pointe du fleuret (via fil dans la rainure lame) |
| C     | Vert           | Masse   | Bati du fleuret (coque/garde + lame de l'arme)   |

### Bouton-poussoir (tete allemande) — NORMALEMENT FERME

- **Au repos** (bouton NON presse) : ligne B connectee a ligne C.
  Le courant circule de B vers C en permanence. Circuit ferme.
- **Bouton presse** : la connexion B <-> C est **rompue**.
  La ligne B n'est plus reliee qu'a ce qui touche la pointe physiquement.

### Fonctionnement du systeme filaire classique

| Situation                                  | Etat electrique                    | Resultat                          |
|--------------------------------------------|------------------------------------|-----------------------------------|
| Bouton **non** presse                      | B connecte a C (ferme)             | Rien                              |
| Bouton presse, pointe touche **lame adv.** | B se connecte a A (via le lame)    | **Touche valide** (lumiere color) |
| Bouton presse, pointe touche **piste/arme**| B se connecte a C (via masse/piste)| **Rien** (annule)                 |
| Bouton presse, pointe touche **autre**     | B n'est connecte a rien            | **Touche non-valide** (blanche)   |

---

## Adaptation Sans Fil — Architecture par Tireur

### Principe : 4 pins fonctionnels par Pico

Chaque Pico utilise les 3 lignes du fil de corps (A, B, C) plus un pin dedie
pour la detection du bouton. Les fonctions sont clairement separees :

| Pin Pico    | Ligne fil de corps | Direction  | Fonction                               |
|-------------|--------------------|------------|----------------------------------------|
| Pin 1 (PWM) | C (vert)          | SORTIE     | Generation Freq_NEUTRE sur bati/coque  |
| Pin 2 (PWM) | A (rouge)         | SORTIE     | Generation Freq_VALID sur lame/cuirasse|
| Pin 3 (GPIO)| B <-> C           | ENTREE     | Detection bouton (circuit ouvert/ferme)|
| Pin 4 (GPIO)| B (bleu)          | ENTREE     | Detection frequence par interruption   |

### Detail de chaque pin

**Pin 1 — Generation Freq_NEUTRE (ligne C / vert) — SORTIE PWM**
- Genere un signal carre a 20 kHz sur la ligne C
- Ce signal se propage sur le bati du fleuret (coque + lame de l'arme)
- Meme frequence que la piste metallique (quand disponible)
- But : toute surface "neutre" (coque, piste) porte cette frequence

**Pin 2 — Generation Freq_VALID (ligne A / rouge) — SORTIE PWM**
- Genere un signal carre a Freq_VALID (unique par tireur) sur la ligne A
- La ligne A est connectee au lame/cuirasse du tireur
- Freq_VALID_A = 25 kHz, Freq_VALID_B = 40 kHz
- But : identifier une touche sur la surface valable de CE tireur

**Pin 3 — Detection bouton (dedie) — ENTREE GPIO**
- Pin dedie a la detection de l'etat du bouton-poussoir
- Au repos : B est relie a C (bouton ferme) → etat connu
- Bouton presse : B <-> C rompu → changement d'etat detecte
- Lecture numerique simple (digitalRead), instantanee (~microsecondes)

**Pin 4 — Detection frequence (ligne B / bleu) — ENTREE GPIO (interruption)**
- Detecte le signal qui arrive sur la pointe du fleuret via la ligne B
- N'est analyse que QUAND le bouton est presse (Pin 3)
- Comptage d'impulsions par interruption (RISING) sur une fenetre de 50 ms
- Classification par plage de frequence (±2 kHz) :
  - Freq_VALID adverse → touche valide
  - Freq_NEUTRE → pas de lumiere (piste ou coque)
  - Aucune frequence → touche non-valide (blanche)

### Sequence de detection (boucle principale)

```
BOUCLE PRINCIPALE (chaque Pico tireur) :

1. Lire Pin 3 (GPIO) : le bouton est-il presse ?
   |
   +-- NON → rien, retour debut boucle
   |
   +-- OUI → passer a l'etape 2

2. Lire Pin 4 (GPIO interruption) : quelle frequence arrive sur la pointe ?
   |
   +-- Comptage detecte Freq_VALID adverse → TOUCHE VALIDE
   |   Envoyer {player_id, VALID, timestamp, dwell} au central via WiFi
   |
   +-- Comptage detecte Freq_NEUTRE → PAS DE LUMIERE
   |   (ou ne rien envoyer, selon le choix d'implementation)
   |
   +-- Comptage ne detecte rien → TOUCHE NON-VALIDE
       Envoyer {player_id, INVALID, timestamp, dwell} au central via WiFi
```

### Schema du flux electrique

```
ETAT REPOS (bouton non presse) :

  Pin 1 (PWM) --[Freq_NEUTRE]--> Ligne C --> Bati/Coque
                                                  |
                                          [bouton FERME]
                                                  |
  Pin 4 (GPIO) <-------- Ligne B <---------- Pointe
  Pin 3 (GPIO) : detecte circuit B-C ferme → "bouton NON presse"

  → Le Pico sait que le bouton n'est pas presse. Pas d'analyse.


ETAT TOUCHE (bouton presse) :

  Pin 1 (PWM) --[Freq_NEUTRE]--> Ligne C --> Bati/Coque
                                                  |
                                          [bouton OUVERT] ← rupture
                                                  |
  Pin 3 (GPIO) : detecte circuit B-C ouvert → "bouton PRESSE"

  La pointe du fleuret touche une surface externe :

  Pin 2 adv. --[Freq_VALID_adv]--> Lame adverse --> Pointe --> Ligne B --> Pin 4 (GPIO)
  → Comptage detecte Freq_VALID adverse → TOUCHE VALIDE

  OU

  Pin 1 adv. --[Freq_NEUTRE]--> Coque adverse --> Pointe --> Ligne B --> Pin 4 (GPIO)
  → Comptage detecte Freq_NEUTRE → PAS DE LUMIERE

  OU

  Surface non conductrice --> Pointe --> Ligne B --> Pin 4 (GPIO)
  → Comptage ne detecte rien → TOUCHE BLANCHE
```

---

## Architecture Globale (2 tireurs + central)

```
TIREUR A                                          TIREUR B
+----------------------+                          +----------------------+
|  Pico W (A)          |                          |  Pico W (B)          |
|                      |                          |                      |
|  GENERE:             |                          |  GENERE:             |
|  Pin1: Freq_NEUTRE   |                          |  Pin1: Freq_NEUTRE   |
|    (20kHz) → coque A |                          |    (20kHz) → coque B |
|  Pin2: Freq_VALID_A  |                          |  Pin2: Freq_VALID_B  |
|    (25kHz) → lame A  |                          |    (40kHz) → lame B  |
|                      |                          |                      |
|  DETECTE:            |                          |  DETECTE:            |
|  Pin3: bouton (GPIO) |                          |  Pin3: bouton (GPIO) |
|  Pin4: freq (GPIO    |         WiFi UDP         |  Pin4: freq (GPIO    |
|    interruption)     |<------------------------>|    interruption)     |
+----------+-----------+                          +----------+-----------+
           |                                                  |
           |              +--------------+                    |
           +------------->|  CENTRAL     |<-------------------+
                          |  (Pico W     |
                          |   ou Mega)   |
                          |              |
                          |  - Arbitrage |
                          |  - Lockout   |
                          |  - Affichage |
                          |    lumieres  |
                          +--------------+
```

### Frequences

- **Freq_NEUTRE** (20 kHz) : frequence commune generee sur les coques des
  fleurets (ligne C) des DEUX tireurs, et sur la piste metallique (plus tard).
  Detecter cette frequence = annuler la touche (pas de lumiere).
- **Freq_VALID_A** (25 kHz) : frequence unique du tireur A, generee sur
  sa cuirasse/lame (ligne A). Si le fleuret de B detecte cette frequence,
  c'est une touche valide de B sur A.
- **Freq_VALID_B** (40 kHz) : frequence unique du tireur B, generee sur
  sa cuirasse/lame (ligne A). Si le fleuret de A detecte cette frequence,
  c'est une touche valide de A sur B.

### Logique de Detection (tableau de verite)

| Bouton presse ? | Frequence sur Pin 4 (GPIO) | Resultat                              |
|-----------------|----------------------------|---------------------------------------|
| Non             | (non lu)                   | Rien (pas de touche)                  |
| Oui             | Freq_VALID adverse         | **Touche valide** (lumiere coloree)   |
| Oui             | Freq_NEUTRE                | **Pas de lumiere** (piste/coque)      |
| Oui             | Aucune frequence           | **Touche non-valide** (lumiere blanche)|

---

## Detection de Frequence : Approche Technique

### Methode : Comptage d'impulsions par interruption

Le signal genere est un signal carre. La detection se fait par comptage de
fronts montants (RISING) via interruption materielle sur un pin GPIO.

**Principe** :
1. Configurer le pin en INPUT avec `attachInterrupt(pin, callback, RISING)`
2. Ajouter une **resistance pull-down 10kΩ** entre le pin et GND (indispensable
   pour la detection sans GND commun en 3.3V)
3. L'ISR incremente un compteur a chaque front montant
4. Toutes les 50 ms, lire le compteur et le remettre a zero (section atomique)
5. Frequence = compteur * (1000 / duree_fenetre_ms)
6. Classifier la frequence par plage de tolerance (±2 kHz)

**Avantages** :
- Extremement simple a implementer
- Fonctionne parfaitement avec des signaux carres
- Pas de calcul flottant, pas de buffer, pas de FFT
- Valide sur le Pico W (interruption sur n'importe quel GPIO)

**Pourquoi pas Goertzel / FFT** :
- Le signal est carre, pas sinusoidal → energie repartie sur les harmoniques
- Le comptage par interruption est plus direct et plus fiable pour ce cas
- Goertzel necessite un ADC rapide et du calcul flottant → surcharge inutile

### Chemin du Signal (touche valide)

1. Le Pico adverse genere Freq_VALID sur sa ligne A (Pin 2 PWM)
2. Ce signal est present sur le lame/cuirasse de l'adversaire
3. Le fleuret du tireur touche le lame → le signal entre par la pointe
4. Le bouton-poussoir est presse → B <-> C rompu → B est isole
5. Le signal remonte par la ligne B jusqu'au Pin 4 (GPIO) du Pico du tireur
6. L'interruption compte les fronts montants → frequence identifiee
7. → Touche valide envoyee au central

---

## Communication Sans Fil

### Protocole : WiFi UDP brut

**IMPORTANT** : ESP-NOW n'est PAS disponible sur le Pico W (c'est un protocole
Espressif pour ESP32). Le Pico W utilise un chip CYW43439 (Infineon).

**Solution retenue** : UDP brut sur WiFi
- Le Pico central agit comme Access Point WiFi
- Les deux Pico tireurs s'y connectent
- Communication par paquets UDP
- Latence typique : 2-10 ms (largement suffisant pour le lockout de 300 ms)

### Format de Message (a affiner)

```
struct TouchEvent {
    uint8_t  player_id;      // 1 ou 2
    uint8_t  touch_type;     // VALID=1, INVALID=2, NEUTRAL=3, NONE=0
    uint32_t timestamp_ms;   // timestamp local du Pico
    uint16_t dwell_time_ms;  // duree de contact mesuree
};
```

---

## Plan d'Execution par Phases

### Phase 0 : Proof of Concept — Generation & Detection de Frequence

#### Phase 0.1 : Detection sur meme carte (Arduino Mega) — TERMINE
**Plateforme** : Arduino Mega
**Resultat** : Signal carre genere par tone() detecte par interruption sur la meme carte.
Comptage d'impulsions fonctionnel. Mais test biaise : GND commun entre emetteur et recepteur.
**Code** : `phase0_1_freq_interrupt/`

#### Phase 0.2 : Multi-frequences sur meme carte (Arduino Mega) — TERMINE
**Plateforme** : Arduino Mega
**Resultat** : Distinction de 3 frequences (16/25/40 kHz) par comptage + classification.
Decouverte : 16 kHz n'est pas exact sur le timer du Mega (15873 Hz reel).
Frequences corrigees : 20/25/40 kHz (toutes exactes).
**Code** : `phase0_2_freq_detection/`

#### Phase 0.3 : Detection sans GND commun (Mega → Pico W) — TERMINE
**Plateforme** : Arduino Mega (generateur) + Raspberry Pi Pico W (recepteur)
**Objectif** : Valider que le signal traverse sans masse commune
**Setup** : Mega sur adaptateur secteur, Pico W sur USB Mac (batterie).
Un seul fil entre Pin 9 (Mega) et GPIO 2 (Pico W). Aucun fil GND.
**Resultat** : Detection fiable a ~18500-19700 Hz (classifie NEUTRE 20 kHz).
Le couplage capacitif parasite suffit comme chemin de retour.
**Code** : `phase0_3_no_common_gnd/mega_generator/` et `phase0_3_no_common_gnd/pico_receiver/`

**Checkpoint** : Hypothese fondamentale du projet VALIDEE — detection sans GND commun fonctionne.

#### Phase 0.4 : Detection Pico → Pico (sans Arduino Mega) — TERMINE
**Plateforme** : 2x Raspberry Pi Pico W
**Objectif** : Valider la generation ET la detection sur Pico W uniquement.
**Setup** : Pico generateur (adaptateur secteur) sur GPIO 15, Pico recepteur
(USB Mac) sur GPIO 2. Un seul fil + resistance pull-down 10kΩ cote recepteur.
**Code** : `phase0_4_pico_to_pico/pico_generator/` et `phase0_4_pico_to_pico/pico_receiver/`

**Decouvertes** :
- `tone()` est imprecis sur le Pico W → utiliser PWM hardware (`hardware/pwm.h`)
- Sans pull-down : seulement ~80% des fronts detectes (~16 kHz au lieu de 20 kHz)
  avec montee progressive (capacites parasites qui se chargent)
- Pull-down interne (INPUT_PULLDOWN, ~50kΩ) : insuffisant, meme resultat
- **Pull-down externe 10kΩ** : detection parfaite (20 020 Hz), instantanee

**Circuit recepteur** :
```
GPIO 2 ──┬── fil signal vers generateur
          │
         [10kΩ]
          │
GND ──────┘
```

**Checkpoint** : Le systeme fonctionne entierement sur Pico W avec pull-down 10kΩ.

---

### Phase 1 : Detection via Fil de Corps + Fleuret — EN COURS
**Plateforme** : 2x Raspberry Pi Pico W
**Objectif** : Valider le chemin complet du signal a travers l'equipement reel
**Prerequis** : Pull-down 10kΩ sur le pin de detection (valide en Phase 0.4),
PWM hardware pour la generation de signal, buffer MOSFET 2N7000 + pull-up 100Ω.

- **1.1** : Pico generateur genere 20 kHz (PWM hardware) sur un GPIO connecte
         a une surface conductrice (feuille alu, cuirasse, ou lame) — **TERMINE**
- **1.2** : Brancher le fleuret + fil de corps au Pico recepteur via les connecteurs — **TERMINE**
- **1.3** : Toucher la surface conductrice avec la pointe du fleuret
         et verifier que le Pico recepteur detecte la frequence sur la ligne B — **TERMINE**
         Note : le signal est detecte meme bouton non presse (normal, voir 1.5)
- **1.4** : Tester avec la cuirasse reelle comme surface emettrice — **TERMINE**
         GPIO direct : echec (5-7 kHz). Buffer MOSFET 2N7000 + pull-up 100Ω : **20 kHz** ✅
- **1.5** : Tester la detection du bouton (B-C ferme au repos, ouvert quand presse) — A faire
         Observation : GP2 detecte le signal meme bouton non presse car la pointe
         (ligne B) est toujours connectee a GP2. Le bouton relie B a C au repos
         mais ne bloque pas le signal entrant. La detection du bouton doit se faire
         via un pin GPIO dedie (GP20) qui lit l'etat B↔C explicitement.
- **1.6** : Connecter un second signal (Freq_VALID) sur la cuirasse (ligne A)
         et verifier que le comptage distingue Freq_VALID de Freq_NEUTRE — A faire
- **1.7** : Mesurer le rapport signal/bruit a travers le fil de corps complet — A faire

**Checkpoint** : Le signal traverse proprement l'equipement d'escrime, le bouton
est detecte, et les frequences sont distinguees a travers le fil de corps

---

### Phase 2 : Systeme complet sur Pico W + Multi-frequences
**Plateforme** : Raspberry Pi Pico W
**Objectif** : Faire tourner le systeme complet sur le Pico

- **2.1** : Generation de signal sur le Pico (PWM hardware, pas tone())
         Pin 1 → Freq_NEUTRE (20 kHz) sur ligne C
         Pin 2 → Freq_VALID sur ligne A
- **2.2** : Detection bouton sur Pin 3 (GPIO, lecture ligne B <-> C)
- **2.3** : Detection frequence sur Pin 4 (GPIO interruption, ligne B)
- **2.4** : Valider la boucle complete : bouton → comptage → identification
- **2.5** : Tester avec le materiel reel (cuirasse + coque fleuret + fleuret)

**Checkpoint** : Le Pico detecte le bouton et distingue les frequences a travers l'equipement

---

### Phase 3 : Communication WiFi UDP
**Plateforme** : 2x Raspberry Pi Pico W
**Objectif** : Etablir une communication rapide entre 2 Pico

- **3.1** : Configurer un Pico comme Access Point WiFi
- **3.2** : Connecter le second Pico au premier
- **3.3** : Envoyer des paquets UDP avec les evenements de touche
- **3.4** : Mesurer la latence aller-retour (objectif : < 20 ms)
- **3.5** : Definir le protocole de message (type touche, timestamp, ID tireur)

**Checkpoint** : Latence < 20 ms confirmee

---

### Phase 4 : Logique d'Arbitrage
**Plateforme** : Pico central (ou Arduino Mega)
**Objectif** : Implementer les regles FIE sur le central

- **4.1** : Implementer le timer de lockout (300-350 ms, configurable)
- **4.2** : Implementer le dwell time (15 ms minimum de contact)
- **4.3** : Logique : 1ere touche -> demarrer lockout -> attendre 2eme touche ou expiration
- **4.4** : Determiner le resultat (valide/invalide/double/rien) et commander les lumieres

**Checkpoint** : L'arbitrage respecte les timings FIE

---

### Phase 5 : Integration Complete 2 Tireurs
**Objectif** : Deux tireurs complets face a face

- Chaque Pico genere Freq_NEUTRE sur sa coque (Pin 1 → ligne C)
- Chaque Pico genere Freq_VALID sur sa cuirasse (Pin 2 → ligne A)
- Chaque Pico detecte le bouton (Pin 3 → GPIO)
- Chaque Pico detecte la frequence au bout du fleuret (Pin 4 → GPIO interruption)
- Les evenements remontent au central en WiFi UDP
- Le central arbitre et allume les lumieres

**Checkpoint** : Simulation d'assaut complete fonctionnelle

---

### Phase 6 : Polish & Finitions
**Objectif** : Rendre le systeme utilisable en conditions reelles

- LEDs RGB ou NeoPixels pour les lumieres (rouge/vert/blanc)
- Buzzer pour signaler les touches
- Boitier imprime 3D pour les Pico
- Batterie LiPo pour les Pico portes par les tireurs
- Interface d'affichage sur le central

---

## Composants Supplementaires Recommandes

| Composant                         | Usage                          | Prix approx. |
|-----------------------------------|--------------------------------|--------------|
| 3eme Pico W                      | Central d'arbitrage            | ~8 EUR       |
| LEDs RGB ou NeoPixels             | Signalisation des touches      | ~3 EUR       |
| Buzzer piezo                      | Signal sonore                  | ~1 EUR       |
| Batteries LiPo 3.7V + chargeur   | Alimentation portee            | ~10 EUR x2   |
| Condensateurs 100nF, 10uF        | Filtrage des signaux           | ~2 EUR       |
| Resistances (1k, 10k, 100k ohms) | Diviseurs de tension, pull-ups | ~2 EUR       |
| Transistor MOSFET 2N7000 + resistance 100Ω pull-up | **Buffer de courant pour driver la cuirasse** (VALIDE) | ~1 EUR |

---

## Risques Identifies

| Risque                                    | Impact    | Mitigation                                          |
|-------------------------------------------|-----------|-----------------------------------------------------|
| Signal trop attenue a travers le lame     | Bloquant  | Augmenter amplitude (ampli-op) ou baisser frequence |
| GPIO ne peut pas driver la cuirasse (charge capacitive) | **RESOLU** | Buffer 2N7000 + pull-up 100Ω vers VBUS 5V — valide en Phase 1 |
| Bruit electrique parasite                 | Moyen     | Comptage par interruption est robuste au bruit       |
| Latence WiFi trop elevee                  | Moyen     | UDP brut sans overhead HTTP ; mesurer en Phase 3     |
| Interference entre les 2 frequences       | Moyen     | Frequences suffisamment espacees (20/25/40 kHz)     |
| Signal trop faible pour declencher interruption | RESOLU | Pull-down 10kΩ sur le pin recepteur (valide en Phase 0.4) |

---

## Decisions Techniques Prises

1. **Methode de detection** : Comptage d'impulsions par interruption (RISING)
   - Goertzel/FFT ecartes : inadaptes aux signaux carres, surcharge inutile
2. **Generation de signal** : PWM hardware du RP2040 (`hardware/pwm.h`)
   - `tone()` ecarte sur Pico W : imprecis aux hautes frequences
3. **Pull-down 10kΩ** sur le pin de detection (indispensable sans GND commun en 3.3V)
   - Pull-down interne (~50kΩ) insuffisant
4. **Communication** : WiFi UDP brut (pas ESP-NOW, incompatible Pico W)
5. **Topologie WiFi** : Le central est Access Point, les tireurs sont clients
6. **Frequences** : 3 frequences exactes (20 kHz, 25 kHz, 40 kHz)
7. **Freq_NEUTRE** : identique pour les deux coques de fleuret ET la piste
8. **Approche incrementale** : valider la detection de frequence AVANT tout le reste
9. **Detection bouton** : pin GPIO dedie (pas via analyse de frequence sur B)
10. **Separation des fonctions** : 4 pins par Pico avec roles clairement separes
    - 2 pins PWM en sortie (generation Freq_NEUTRE + Freq_VALID)
    - 1 pin GPIO en entree (detection bouton)
    - 1 pin GPIO en entree + pull-down 10kΩ (detection frequence par interruption)
11. **Buffer de courant** : MOSFET 2N7000 + pull-up **100Ω** vers VBUS (5V) sur
    les pins de sortie PWM qui drivent la cuirasse et la coque. Pull-up 10kΩ
    insuffisante (front montant trop lent). 100Ω fournit ~50 mA, suffisant pour
    charger/decharger la capacitance parasite a 20 kHz. Valide en Phase 1.
    Le signal est inverse (GPIO HIGH → Drain a GND, GPIO LOW → Drain a 5V)
    mais sans impact sur la detection par comptage de fronts.

---

## Notes Techniques Importantes

### Configuration PlatformIO pour Pico W

```ini
[env:rpipicow]
platform          = https://github.com/maxgerhardt/platform-raspberrypi.git
board             = rpipicow
framework         = arduino
board_build.core  = earlephilhower
monitor_speed     = 115200
upload_protocol   = picotool
```

**Attention** : La plateforme officielle `raspberrypi` de PlatformIO ne supporte
pas le Pico W. Il faut utiliser le fork de maxgerhardt avec le core earlephilhower.

**Upload** : Maintenir le bouton BOOTSEL enfonce en branchant l'USB, puis relacher.
Le Pico apparait comme un lecteur USB. PlatformIO upload via picotool automatiquement.

**Serial USB** : Ne pas utiliser `while (!Serial)` qui peut bloquer indefiniment.
Utiliser `delay(2000)` ou `delay(3000)` apres `Serial.begin()` pour laisser le
temps au CDC USB de s'initialiser.

### Tete Allemande (Bouton du Fleuret)
Le bouton-poussoir a la pointe du fleuret est de type **normalement ferme** :
- Au repos : ligne B connectee a ligne C (circuit ferme)
- Bouton presse : ligne B deconnectee de ligne C (circuit ouvert)

La detection du bouton se fait via un **pin GPIO dedie** (Pin 3) qui lit
l'etat du circuit B <-> C. Quand le circuit s'ouvre, le Pico sait que le
bouton est presse et lance le comptage de frequence sur Pin 4.

### Piste Metallique (Sujet Separe)
La piste metallique n'est pas disponible pour les tests actuels. Elle sera traitee
separement. En theorie, il suffit de generer Freq_NEUTRE sur la piste, de la meme
maniere que sur les coques des fleurets. La detection de Freq_NEUTRE au bout du
fleuret annule la touche (pas de lumiere).

### Coque du Fleuret vs Cuirasse
- La **coque** (garde) du fleuret est la partie metallique qui protege la main.
  Elle fait partie du **bati** de l'arme, connecte a la ligne C.
  → Porte **Freq_NEUTRE** (touche annulee si detectee, comme la piste)
- La **cuirasse** (lame) est le gilet conducteur qui couvre la surface valable.
  Connectee a la ligne A.
  → Porte **Freq_VALID** (touche valide si detectee par l'adversaire)

### Correspondance Lignes / Pins / Fonctions (resume)

```
Fil de corps :          Pico :                    Connecte a :
+--------+              +----------+              +------------------+
| A (rouge) |---------->| Pin 2 (PWM) |---------->| Lame/Cuirasse    |
| B (bleu)  |---------->| Pin 4 (GPIO)|           | Pointe fleuret   |
| C (vert)  |---------->| Pin 1 (PWM) |---------->| Bati/Coque       |
| B<->C     |---------->| Pin 3 (GPIO)|           | Bouton (NF)      |
+--------+              +----------+              +------------------+
```

### Pinout Pico W (reference rapide)

```
                    ┌──────── micro USB ────────┐
                    │                           │
            GP0  [1]│●                         ●│[40] VBUS (5V)
            GP1  [2]│●                         ●│[39] VSYS
            GND  [3]│●                         ●│[38] GND
            GP2  [4]│●                         ●│[37] 3V3_EN
            GP3  [5]│●                         ●│[36] 3V3 OUT
            GP4  [6]│●                         ●│[35] ADC_VREF
            GP5  [7]│●                         ●│[34] GP28 / ADC2
            GND  [8]│●                         ●│[33] AGND
            GP6  [9]│●                         ●│[32] GP27 / ADC1
           GP7 [10]│●                         ●│[31] GP26 / ADC0
           GP8 [11]│●                         ●│[30] RUN
           GP9 [12]│●                         ●│[29] GP22
           GND [13]│●                         ●│[28] GND
          GP10 [14]│●                         ●│[27] GP21
          GP11 [15]│●                         ●│[26] GP20
          GP12 [16]│●                         ●│[25] GP19
          GP13 [17]│●                         ●│[24] GP18
           GND [18]│●                         ●│[23] GND
          GP14 [19]│●                         ●│[22] GP17
          GP15 [20]│●                         ●│[21] GP16
                    │          ┌─────┐          │
                    │          │DEBUG│          │
                    └──────────┴─────┴──────────┘
```

---

## Etat d'Avancement

| Phase     | Description                                    | Statut      |
|-----------|------------------------------------------------|-------------|
| Phase 0.1 | Detection sur meme carte (Mega)               | TERMINE     |
| Phase 0.2 | Multi-frequences sur meme carte (Mega)        | TERMINE     |
| Phase 0.3 | Detection sans GND commun (Mega → Pico W)     | TERMINE     |
| Phase 0.4 | Detection Pico → Pico (sans Mega)             | TERMINE     |
| Phase 1   | Detection via fil de corps + fleuret           | EN COURS — buffer MOSFET valide, detection cuirasse OK, reste bouton + multi-freq |
| Phase 2   | Systeme complet sur Pico W                     | A faire     |
| Phase 3   | Communication WiFi UDP                         | A faire     |
| Phase 4   | Logique d'arbitrage                            | A faire     |
| Phase 5   | Integration complete 2 tireurs                 | A faire     |
| Phase 6   | Polish (LEDs, buzzer, boitier, batterie)       | A faire     |
