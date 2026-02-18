# Projet Escrime Sans Fil (Wireless Fencing)

## Contexte & Objectif

Concevoir un systeme d'arbitrage electronique sans fil pour le **fleuret** (escrime),
a base de **Raspberry Pi Pico 2W**, qui remplace le systeme filaire traditionnel.

La problematique principale du sans-fil est l'**absence de masse commune** entre les
deux tireurs. La solution retenue est la **detection de frequence** : chaque tireur
porte un Pico 2W qui genere des signaux AC sur ses surfaces conductrices (cuirasse
et coque du fleuret). Le fleuret adverse detecte ces frequences au contact, ce qui
permet d'identifier la nature de la touche sans connexion physique entre les tireurs.

---

## Materiel Disponible

| Element                        | Statut         |
|--------------------------------|----------------|
| Arduino Mega                   | Disponible     |
| Raspberry Pi Pico 2W x2       | Disponible     |
| Fleuret electrique (tete allemande) | Disponible |
| Fil de corps                   | Disponible     |
| Cuirasse / Lame (FIE standard) | Disponible    |
| Connecteurs fleuret/fil de corps pour Arduino | Disponible |
| Resistances et condensateurs   | Disponible     |
| Piste metallique               | NON disponible (sujet separe) |
| Oscilloscope / analyseur logique | NON disponible |
| 3eme Pico 2W (central)        | A commander    |

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

| Pin Pico   | Ligne fil de corps | Direction  | Fonction                              |
|------------|--------------------|------------|---------------------------------------|
| Pin 1 (PWM)| C (vert)          | SORTIE     | Generation Freq_NEUTRE sur bati/coque |
| Pin 2 (PWM)| A (rouge)         | SORTIE     | Generation Freq_VALID sur lame/cuirasse|
| Pin 3 (GPIO)| B <-> C           | ENTREE     | Detection bouton (circuit ouvert/ferme)|
| Pin 4 (ADC)| B (bleu)          | ENTREE     | Detection frequence sur la pointe     |

### Detail de chaque pin

**Pin 1 — Generation Freq_NEUTRE (ligne C / vert) — SORTIE PWM**
- Genere un signal carre a Freq_NEUTRE (ex: 20 kHz) sur la ligne C
- Ce signal se propage sur le bati du fleuret (coque + lame de l'arme)
- Meme frequence que la piste metallique (quand disponible)
- But : toute surface "neutre" (coque, piste) porte cette frequence

**Pin 2 — Generation Freq_VALID (ligne A / rouge) — SORTIE PWM**
- Genere un signal carre a Freq_VALID (unique par tireur) sur la ligne A
- La ligne A est connectee au lame/cuirasse du tireur
- Freq_VALID_A = ex: 30 kHz, Freq_VALID_B = ex: 50 kHz
- But : identifier une touche sur la surface valable de CE tireur

**Pin 3 — Detection bouton (dedie) — ENTREE GPIO**
- Pin dedie a la detection de l'etat du bouton-poussoir
- Au repos : B est relie a C (bouton ferme) → etat connu
- Bouton presse : B <-> C rompu → changement d'etat detecte
- Lecture numerique simple (digitalRead), instantanee (~microsecondes)
- Pas besoin de Goertzel pour cette detection

**Pin 4 — Detection frequence (ligne B / bleu) — ENTREE ADC**
- Lit le signal qui arrive sur la pointe du fleuret via la ligne B
- N'est analyse que QUAND le bouton est presse (Pin 3)
- L'algorithme de Goertzel identifie la frequence presente :
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

2. Lire Pin 4 (ADC) : quelle frequence arrive sur la pointe ?
   |
   +-- Goertzel detecte Freq_VALID adverse → TOUCHE VALIDE
   |   Envoyer {player_id, VALID, timestamp, dwell} au central via WiFi
   |
   +-- Goertzel detecte Freq_NEUTRE → PAS DE LUMIERE
   |   Envoyer {player_id, NEUTRAL, timestamp, dwell} au central via WiFi
   |   (ou ne rien envoyer, selon le choix d'implementation)
   |
   +-- Goertzel ne detecte rien → TOUCHE NON-VALIDE
       Envoyer {player_id, INVALID, timestamp, dwell} au central via WiFi
```

### Schema du flux electrique

```
ETAT REPOS (bouton non presse) :

  Pin 1 (PWM) --[Freq_NEUTRE]--> Ligne C --> Bati/Coque
                                                  |
                                          [bouton FERME]
                                                  |
  Pin 4 (ADC) <-------- Ligne B <---------- Pointe
  Pin 3 (GPIO) : detecte circuit B-C ferme → "bouton NON presse"

  → Le Pico sait que le bouton n'est pas presse. Pas d'analyse Goertzel.


ETAT TOUCHE (bouton presse) :

  Pin 1 (PWM) --[Freq_NEUTRE]--> Ligne C --> Bati/Coque
                                                  |
                                          [bouton OUVERT] ← rupture
                                                  |
  Pin 3 (GPIO) : detecte circuit B-C ouvert → "bouton PRESSE"

  La pointe du fleuret touche une surface externe :

  Pin 2 adv. --[Freq_VALID_adv]--> Lame adverse --> Pointe --> Ligne B --> Pin 4 (ADC)
  → Goertzel detecte Freq_VALID adverse → TOUCHE VALIDE

  OU

  Pin 1 adv. --[Freq_NEUTRE]--> Coque adverse --> Pointe --> Ligne B --> Pin 4 (ADC)
  → Goertzel detecte Freq_NEUTRE → PAS DE LUMIERE

  OU

  Surface non conductrice --> Pointe --> Ligne B --> Pin 4 (ADC)
  → Goertzel ne detecte rien → TOUCHE BLANCHE
```

---

## Architecture Globale (2 tireurs + central)

```
TIREUR A                                          TIREUR B
+----------------------+                          +----------------------+
|  Pico 2W (A)         |                          |  Pico 2W (B)         |
|                      |                          |                      |
|  GENERE:             |                          |  GENERE:             |
|  Pin1: Freq_NEUTRE   |                          |  Pin1: Freq_NEUTRE   |
|    (20kHz) → coque A |                          |    (20kHz) → coque B |
|  Pin2: Freq_VALID_A  |                          |  Pin2: Freq_VALID_B  |
|    (30kHz) → lame A  |                          |    (50kHz) → lame B  |
|                      |                          |                      |
|  DETECTE:            |                          |  DETECTE:            |
|  Pin3: bouton (GPIO) |                          |  Pin3: bouton (GPIO) |
|  Pin4: freq (ADC)    |         WiFi UDP         |  Pin4: freq (ADC)    |
|                      |<------------------------>|                      |
+----------+-----------+                          +----------+-----------+
           |                                                  |
           |              +--------------+                    |
           +------------->|  CENTRAL     |<-------------------+
                          |  (Pico 2W    |
                          |   ou Mega)   |
                          |              |
                          |  - Arbitrage |
                          |  - Lockout   |
                          |  - Affichage |
                          |    lumieres  |
                          +--------------+
```

### Frequences

- **Freq_NEUTRE** (ex: 20 kHz) : frequence commune generee sur les coques des
  fleurets (ligne C) des DEUX tireurs, et sur la piste metallique (plus tard).
  Detecter cette frequence = annuler la touche (pas de lumiere).
- **Freq_VALID_A** (ex: 30 kHz) : frequence unique du tireur A, generee sur
  sa cuirasse/lame (ligne A). Si le fleuret de B detecte cette frequence,
  c'est une touche valide de B sur A.
- **Freq_VALID_B** (ex: 50 kHz) : frequence unique du tireur B, generee sur
  sa cuirasse/lame (ligne A). Si le fleuret de A detecte cette frequence,
  c'est une touche valide de A sur B.

### Logique de Detection (tableau de verite)

| Bouton presse ? | Frequence sur Pin 4 (ADC) | Resultat                              |
|-----------------|---------------------------|---------------------------------------|
| Non             | (non lu)                  | Rien (pas de touche)                  |
| Oui             | Freq_VALID adverse        | **Touche valide** (lumiere coloree)   |
| Oui             | Freq_NEUTRE               | **Pas de lumiere** (piste/coque)      |
| Oui             | Aucune frequence          | **Touche non-valide** (lumiere blanche)|

---

## Communication Sans Fil

### Protocole : WiFi UDP brut

**IMPORTANT** : ESP-NOW n'est PAS disponible sur le Pico 2W (c'est un protocole
Espressif pour ESP32). Le Pico 2W utilise un chip CYW43439 (Infineon).

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

## Detection de Frequence : Approche Technique

### Algorithme de Goertzel

L'algorithme de Goertzel est un DFT optimise pour detecter UNE frequence specifique.
Beaucoup plus leger qu'une FFT complete. Parfait pour ce cas d'usage.

**Principe** : On calcule l'energie du signal a une frequence cible donnee.
Si l'energie depasse un seuil, la frequence est presente.

**Avantages** :
- Tres peu de memoire et de calcul
- Peut tourner en temps reel sur un microcontroleur
- Tres selectif en frequence (rejette bien le bruit)

**Utilisation dans ce projet** :
- On execute Goertzel pour 2 frequences cibles (Freq_VALID adverse + Freq_NEUTRE)
- Seulement quand le bouton est presse (Pin 3 = ouvert)
- Si l'energie de Freq_VALID depasse le seuil → touche valide
- Si l'energie de Freq_NEUTRE depasse le seuil → pas de lumiere
- Si aucune energie significative → touche blanche

### Capacites ADC

| Plateforme    | Resolution | Vitesse max  | Suffisant pour 20-50 kHz ? |
|---------------|------------|--------------|----------------------------|
| Arduino Mega  | 10-bit     | ~77 ksps     | Oui (Nyquist: 38.5 kHz)    |
| Pico 2W       | 12-bit     | 500 ksps     | Largement oui              |

> Pour l'Arduino Mega, la vitesse ADC par defaut est ~9.6 ksps. Il faut
> modifier le prescaler de l'ADC pour atteindre ~77 ksps (prescaler = 16).
> Cela reduit legerement la precision mais reste suffisant.

### Chemin du Signal (touche valide)

1. Le Pico adverse genere Freq_VALID sur sa ligne A (Pin 2 PWM)
2. Ce signal est present sur le lame/cuirasse de l'adversaire
3. Le fleuret du tireur touche le lame → le signal entre par la pointe
4. Le bouton-poussoir est presse → B <-> C rompu → B est isole
5. Le signal remonte par la ligne B jusqu'au Pin 4 (ADC) du Pico du tireur
6. Le Pico echantillonne et Goertzel identifie Freq_VALID adverse
7. → Touche valide envoyee au central

---

## Plan d'Execution par Phases

### Phase 0 : Proof of Concept — Generation & Detection de Frequence
**Plateforme** : Arduino Mega (disponible immediatement)
**Objectif** : Valider qu'on peut generer un signal AC et le detecter avec un ADC

- **0.1** : Generer un signal carre ~20-30 kHz sur une pin digitale (tone() ou timer)
- **0.2** : Connecter ce signal a une entree analogique via un diviseur de tension (resistance)
- **0.3** : Echantillonner l'ADC et implementer l'algorithme de Goertzel
- **0.4** : Afficher sur Serial Monitor : "Frequence X detectee" / "Aucune frequence"

**Checkpoint** : Goertzel detecte correctement la frequence cible

---

### Phase 1 : Detection via Fil de Corps + Fleuret
**Plateforme** : Arduino Mega
**Objectif** : Valider le chemin complet du signal a travers l'equipement reel

- **1.1** : Connecter le signal AC sur la ligne C (coque) via le connecteur
- **1.2** : Brancher le fleuret + fil de corps a l'Arduino via les connecteurs
- **1.3** : Lire la ligne B sur un pin GPIO : verifier la detection du bouton
         (B-C ferme au repos → signal C present sur B ; bouton presse → signal disparu)
- **1.4** : Lire la ligne B sur un pin ADC : quand le bouton est presse et que la
         pointe touche une surface avec frequence, verifier la detection Goertzel
- **1.5** : Connecter un second signal (Freq_VALID) sur le lame/cuirasse (ligne A)
         et verifier que Goertzel distingue Freq_VALID de Freq_NEUTRE
- **1.6** : Mesurer le rapport signal/bruit, ajuster amplitude/frequence si necessaire

**Checkpoint** : Le signal traverse proprement l'equipement, le bouton est detecte,
et les frequences sont distinguees

---

### Phase 2 : Portage Pico 2W + Multi-frequences
**Plateforme** : Raspberry Pi Pico 2W
**Objectif** : Faire tourner le systeme complet sur le Pico

- **2.1** : Porter la generation de signal sur le Pico (PWM hardware, plus precis)
         Pin 1 → Freq_NEUTRE sur ligne C, Pin 2 → Freq_VALID sur ligne A
- **2.2** : Porter la detection bouton sur Pin 3 (GPIO, lecture ligne B <-> C)
- **2.3** : Porter l'algorithme de Goertzel sur le Pico (ADC 12-bit, 500 ksps)
         Pin 4 → lecture ligne B
- **2.4** : Valider la boucle complete : bouton → Goertzel → identification
- **2.5** : Tester avec le materiel reel (cuirasse + coque fleuret + fleuret)

**Checkpoint** : Le Pico detecte le bouton et distingue les frequences a travers l'equipement

---

### Phase 3 : Communication WiFi UDP
**Plateforme** : 2x Raspberry Pi Pico 2W
**Objectif** : Etablir une communication rapide entre 2 Pico

- **3.1** : Configurer un Pico comme Access Point WiFi
- **3.2** : Connecter le second Pico au premier
- **3.3** : Envoyer des paquets UDP avec les evenements de touche
- **3.4** : Mesurer la latence aller-retour (objectif : < 20 ms)
- **3.5** : Definir le protocole de message (type touche, timestamp, ID tireur)

**Checkpoint** : Latence < 20 ms confirmee

---

### Phase 4 : Logique d'Arbitrage
**Plateforme** : Pico central (ou Arduino Mega + module WiFi)
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
- Chaque Pico detecte la frequence au bout du fleuret (Pin 4 → ADC ligne B)
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
| 3eme Pico 2W                     | Central d'arbitrage            | ~8 EUR       |
| Module ESP8266 (alternative)      | WiFi pour Arduino Mega centrale| ~5 EUR       |
| LEDs RGB ou NeoPixels             | Signalisation des touches      | ~3 EUR       |
| Buzzer piezo                      | Signal sonore                  | ~1 EUR       |
| Batteries LiPo 3.7V + chargeur   | Alimentation portee            | ~10 EUR x2   |
| Condensateurs 100nF, 10uF        | Filtrage des signaux           | ~2 EUR       |
| Resistances (1k, 10k, 100k ohms) | Diviseurs de tension, pull-ups | ~2 EUR       |

---

## Risques Identifies

| Risque                                    | Impact    | Mitigation                                          |
|-------------------------------------------|-----------|-----------------------------------------------------|
| Signal trop attenue a travers le lame     | Bloquant  | Augmenter amplitude (ampli-op) ou baisser frequence |
| Bruit electrique parasite                 | Moyen     | Filtrage analogique (RC) + Goertzel (tres selectif)  |
| Latence WiFi trop elevee                  | Moyen     | UDP brut sans overhead HTTP ; mesurer en Phase 3     |
| Interference entre les 2 frequences       | Moyen     | Frequences suffisamment espacees (ratio > 1.5x)     |
| Pico ADC pas assez rapide                 | Faible    | 500 ksps largement suffisant pour 20-50 kHz         |
| Arduino Mega ADC trop lent par defaut     | Moyen     | Modifier le prescaler ADC (prescaler=16 -> ~77 ksps)|

---

## Decisions Techniques Prises

1. **Algorithme de detection** : Goertzel (pas FFT) — plus leger, plus selectif
2. **Communication** : WiFi UDP brut (pas ESP-NOW, incompatible Pico 2W)
3. **Topologie WiFi** : Le central est Access Point, les tireurs sont clients
4. **Frequences** : 3 frequences distinctes (Freq_VALID_A, Freq_VALID_B, Freq_NEUTRE)
5. **Freq_NEUTRE** : identique pour les deux coques de fleuret ET la piste
6. **Approche incrementale** : valider la detection de frequence AVANT tout le reste
7. **Detection bouton** : pin GPIO dedie (pas via analyse de frequence sur B)
8. **Separation des fonctions** : 4 pins par Pico avec roles clairement separes
   - 2 pins PWM en sortie (generation Freq_NEUTRE + Freq_VALID)
   - 1 pin GPIO en entree (detection bouton)
   - 1 pin ADC en entree (detection frequence)

---

## Notes Techniques Importantes

### Tete Allemande (Bouton du Fleuret)
Le bouton-poussoir a la pointe du fleuret est de type **normalement ferme** :
- Au repos : ligne B connectee a ligne C (circuit ferme)
- Bouton presse : ligne B deconnectee de ligne C (circuit ouvert)

La detection du bouton se fait via un **pin GPIO dedie** (Pin 3) qui lit
l'etat du circuit B <-> C. Quand le circuit s'ouvre, le Pico sait que le
bouton est presse et lance l'analyse de frequence sur Pin 4.

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
| B (bleu)  |---------->| Pin 4 (ADC) |           | Pointe fleuret   |
| C (vert)  |---------->| Pin 1 (PWM) |---------->| Bati/Coque       |
| B<->C     |---------->| Pin 3 (GPIO)|           | Bouton (NF)      |
+--------+              +----------+              +------------------+
```

---

## Etat d'Avancement

| Phase   | Description                                    | Statut      |
|---------|------------------------------------------------|-------------|
| Phase 0 | PoC generation/detection frequence (Mega)      | A faire     |
| Phase 1 | Detection via fil de corps + fleuret (Mega)    | A faire     |
| Phase 2 | Portage Pico 2W + multi-frequences             | A faire     |
| Phase 3 | Communication WiFi UDP                         | A faire     |
| Phase 4 | Logique d'arbitrage                            | A faire     |
| Phase 5 | Integration complete 2 tireurs                 | A faire     |
| Phase 6 | Polish (LEDs, buzzer, boitier, batterie)       | A faire     |
