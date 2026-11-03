# 09 — Plugins Audio Lab Opto et Audio Lab FET (interface et moteur)

Deux plugins VST3 / AU / AAX tirés des outils « Opto » et « FET » d'Audio Lab, construits comme Audio Lab Compresseur.
**Interface à la charte UI V2 des plugins (fiche 02, 01/11/2026)** depuis la version 1.1.0 : thème sombre unique, police Poppins,
familles de couleurs teal / orange, cadran VU clair rétroéclairé. Cette fiche précise la mise en page ; la charte 02 fait foi pour les composants.

| | Audio Lab Opto | Audio Lab FET |
|---|---|---|
| Inspiré de | compresseur optique type LA-2A | compresseur FET type 1176 |
| Dépôt GitHub | drummingludo/audiolab-opto | drummingludo/audiolab-fet |
| Code plugin (AU) | `aufx AlOp Trsv` | `aufx AlFt Trsv` |
| Identifiant | fr.transversalstudio.audiolab.opto | fr.transversalstudio.audiolab.fet |
| Taille de base | 574 × 292 px (574 × 400 avec les réglages avancés) | 819 × 248 px (819 × 322 avec les réglages avancés) |

Noms : « LA-2A » et « 1176 » sont des marques d'Universal Audio, d'où « Opto » et « FET » ; la charte interdit les noms de marques matérielles dans les plugins (description affichée par le logiciel hôte : « Compresseur optique / FET d'Audio Lab »).

## 1. Moteur audio (versions 1.x, identique au web ; depuis la 2.0.0, voir § 7)

- Portage ligne à ligne des classes `OptoDSP` (worklet `al-opto`) et `FetDSP` (worklet `al-fet`) d'Audio_Lab.html (version du 17/10/2026), en double précision.
- Les calculs faits « une fois par bloc de 128 échantillons » dans le web sont refaits au même rythme quel que soit le bloc envoyé par le logiciel hôte : sortie identique au web (écart < 10⁻⁶) à 44,1 / 48 / 96 kHz et pour des blocs de 1 à 4096 échantillons.
- Opto : seuil = 10 − 0,55 × Peak Reduction (dBFS), ratio 3,5:1 (Compress) ou 12:1 (Limit), knee 12 dB ; cellule optique attaque 10 ms, relâchement 60 ms puis 0,5 à 5 s selon la densité du programme ; étage à tubes (tanh), transfo (coupe-bas 25 Hz, coupe-haut 18 kHz).
- FET : seuil fixe, on compresse en poussant Input ; ratios 4:1 (seuil −14, knee 6), 8:1 (−11, 4), 12:1 (−9, 3), 20:1 (−7, 2), All (20:1, −10, knee 10, la sortie redescend au-delà de 8 dB, attaque deux fois plus rapide) ; attaque 800 µs → 20 µs, relâchement 1,1 s → 50 ms (positions 1 à 7, 7 = le plus rapide), relâchement jusqu'à 1,6 fois plus lent quand la réduction est forte ; harmoniques paires du FET, ampli classe A (tanh), transfo (coupe-bas 18 Hz).
- Commun : Dry/Wet à puissance constante, Bypass de l'hôte avec fondu de 20 ms (sortie = entrée exacte une fois le fondu fini), aucune latence.

## 2. VU-mètre (commun, charte 02 § 7)

- **Écran 270 × 200** (réduit le 01/11 à la demande de Ludovic ; avant 330 × 224), « Surface » #2A2B29, bord #3E3F3D, coins 10 ; cadran encastré 14 px à l'intérieur, 242 × 172 comme sur le Compresseur (coins 8, ombre intérieure 4 px vers le bas, flou 5,5, 50 %).
- Cadran clair #F9F9FB rétroéclairé : halo de lampe ambre #EAA93D sous le pivot (34 % → 12 % → 0), **éteint en bypass** ; bords assombris (0 → 9 %) ; reflet de verre diagonal (blanc 85 % → 18 %, coupé net) qui ne touche pas les chiffres.
- Échelle −20 … +3 VU du SVG d'Audio Lab (repère 330 × 200, placé en haut du cadran, pivot visible) : grandes graduations #1D1E1C, petites #969697, chiffres #686868, zone 0 à +3 rouge #9B2226 ; « VU » 700, fonction 600 (#686868).
- Aiguille #1D1E1C de 1,6 px à l'écran avec ombre portée (28 %), pivot en relief (#4A4B49 → #1D1E1C).
- Balistique VU normalisée : 99 % en 300 ms, dépassement ≈ 1,3 %. Le cadran ne se redessine que si l'aiguille ou la valeur bougent.
- Mode GR : réduction de gain (mémoire de crête 60 ms), valeur « x.x dB GR » en bas à droite (chiffres en cellules fixes).
- Modes de sortie : niveau mesuré sur 10 ms, convention AES17 (sinus de crête −18 dBFS = −18 dBFS) ; +4 → 0 VU = −18 dBFS, +8 → −14 dBFS, +10 → −12 dBFS ; valeur « ±x.x VU ».
- Mode OFF (FET seulement) : aiguille au repos à gauche.
- Le mode choisi est enregistré avec la session.

## 3. Audio Lab Opto — 574 × 292

- Rangée principale (y 24 → 224) : Gain (potentiomètre principal, 4, 68, 143 × 150, sous-titre « rattrapage de sortie », **teal secondaire #79AFB0**, arc depuis 0 dB) · VU (152, 24, 270 × 200) · Peak Reduction (427, 68, 143 × 150, unité %, sous-titre « seuil + ratio couplés », **dégradé teal #0A9396 → #79AFB0** depuis 0).
- Rangée du bas (y 240, hauteur 28) : sélecteur VU « GR / +4 / +10 » à gauche (x 24) ; « Compress / Limit » centré sous le VU ; petit Mix (28 px, teal secondaire) aligné à droite (x 550).
- Paramètres : Gain −20 → +30 dB (0), Peak Reduction 0 → 100 (40), Mode Compress / Limit, Dry/Wet 0 → 100 % (100), Bypass.

## 4. Audio Lab FET — 819 × 248

- Colonnes de gauche à droite, toutes entre y 24 et y 224 :
  - Input (4, 68, 143 × 130), **dégradé teal** (niveau qui attaque le seuil fixe), arc depuis 0 dB ; Output (131, 68, 143 × 130), **teal secondaire**, arc depuis 0 dB (gains : l'arc part du point neutre).
  - Colonne Attack / Release centrée en x 313,5 : titre ATTACK, potentiomètre moyen **orange (dégradé #BB3E03 → #EAA93D)**, Ø 76, capuchon 50, centré y 79 ; second centré y 153 ; titre RELEASE puis « 7 = le plus rapide ». Au centre, la graduation de l'original en gros (« 7 ») et la valeur réelle dessous (« 50 ms », « 20 µs »), comme le demande la charte (« 7 · 50 ms »).
  - RATIO : sélecteur vertical 20 / 12 / 8 / 4 / All (373, 46, 52 × 178).
  - VU (449, 24, 270 × 200).
  - METER : sélecteur vertical GR / +8 / +4 / OFF (743, 46, 52 de large, même hauteur de segment que Ratio), puis petit Mix (teal secondaire) aligné à droite en x 795, y 196 (bas aligné sur le VU).
- Paramètres : Input −20 → +30 dB (0), Output −30 → +20 dB (0), Attack 1 → 7 (4), Release 1 → 7 (4), Ratio 20 / 12 / 8 / 4 / All (4), Dry/Wet 0 → 100 % (100), Bypass.

## 5. Comportements communs

- Thème sombre unique (fond #1D1E1C), police Poppins embarquée (400 / 500 / 600 / 700, licence SIL OFL) ; valeurs qui bougent en chiffres à cellules fixes.
- Potentiomètres (charte 02 § 7) : arc 270° d'épaisseur 3,2, rail #3E3F3D (éclairci au survol), partie colorée en dégradé du départ à la valeur, bout mobile coupé droit, pastille de départ ronde, halo léger renforcé pendant le réglage ; capuchon #373837 avec liseré de lumière et ombres ; repère de 1,5 px couleur de fin d'arc ; valeur 14 px 600, unité 11 px #A5A6A6 ; nom 11 px 600 majuscules.
- Sélecteurs segmentés : bande #2A2B29, marge 3 px, segments égaux, segment choisi #005F73 (texte #F9F9FB en 500), autres en #C9C9CA, texte 12 px.
- Fenêtre redimensionnable de 60 % à 200 % (proportions fixes), taille mémorisée.
- Bypass (depuis le logiciel hôte) : la lampe du VU s'éteint ; les réglages restent accessibles.
- Info-bulles en français sur chaque réglage (ce que fait le réglage, ce qu'il faut écouter).
- Presets, snapshots A/B/C : non inclus (presets via le logiciel hôte), comme Audio Lab Compresseur.

## 6. Vérifications faites

- `tests/verify_dsp.py` : plugin ↔ code web d'Audio Lab (empreinte SHA-256 de la copie de référence), courbes statiques, temps d'attaque / relâchement, harmoniques (FET).
- `tests/processor_test.cpp` : sauvegarde / rappel, bypass exact, mono, Dry/Wet 0 %, niveau du VU (AES17), FET « All » = moteur.
- `tests/render_ui.cpp` : captures des modes du VU, bypass (lampe éteinte), 60 % / 200 %.
- pluginval niveau 10 (Linux VST3, avec tests d'interface) ; sur GitHub : auval + pluginval niveau 10 (Mac VST3 + AU), pluginval (Windows VST3), contrôle de structure AAX.
- Non testé ici : écoute réelle dans un logiciel, Windows sur un vrai PC, AAX dans Pro Tools Developer.

## 7. Version 2.0.0 (03/11/2026) : qualité « pro »

Décision : les plugins deviennent la version « pro » ; la page web Audio_Lab.html ne change pas pour l'instant. Le moteur part toujours du code web, mais il est rendu plus fidèle aux appareils d'origine et plus propre (audit qualité, fiche 15). Les identifiants et le sens des anciens réglages ne changent pas : une session 1.2 se rouvre avec ses réglages, les nouveaux réglages prennent leur valeur par défaut.

### 7.1 Lab FET

**Nouveau réglage** (panneau « Advanced settings ») : **Oversampling** (identifiant `oversampling`) : Off / 2× / 4× / 8×, **défaut 4×**.

| Mode | Latence | à 48 kHz | Processeur (machine de test, 48 kHz stéréo) |
|---|---|---|---|
| Off | 0 échantillon | 0 ms | 0,5 % d'un cœur |
| 2× | 71 | 1,48 ms | 2,4 % |
| 4× (défaut) | 77 | 1,60 ms | 4,6 % |
| 8× | 79 | 1,65 ms | 8,4 % |

La latence est annoncée au logiciel ; le son direct du Dry/Wet et du bypass est retardé d'autant (aucun filtre en peigne). Changer de mode pendant la lecture passe par un fondu de 10 ms. Dans Pro Tools (AAX), la latence reste fixe à 79 échantillons quel que soit le mode.

**Ce qui change dans le son**
1. **Suréchantillonnage autour de tout le moteur** (Oversampler.h commun : phase linéaire, ondulation < 0,001 dB, réjection ≥ 99 dB). En 2× / 4× / 8×, l'attaque du détecteur lit la vraie crête entre deux échantillons (3 points intermédiaires interpolés, retenus seulement s'ils forment une crête) : sans cela, un aigu fort fait varier la réduction d'une période à l'autre selon la position des échantillons, ce qui module le gain et crée des raies parasites (c'est ce qui limitait le 2× et le 4× à −70 / −80 dB). Coût : 1 échantillon de latence (2 en 2×).
2. **Détection après l'étage de gain** (contre-réaction, comme le 1176). La boucle est résolue exactement à chaque échantillon (pas de retard d'un échantillon dans la boucle) : stable à tous les réglages. Sa loi est déduite de la courbe d'origine : les ratios statiques restent exacts. La boucle accélère l'attaque d'un facteur égal au ratio : c'est compensé, le temps affiché reste vrai à tous les ratios.
3. **Calibration 0 VU = −18 dBFS** : l'ampli de sortie classe A est asymétrique (H2 + H3, au lieu de H3 seule) ; distorsion faible au niveau nominal, qui monte vers 0 dBFS.
4. **Mode « All »** plus fidèle au manuel de l'original (« ratio entre 12:1 et 20:1 », temps modifiés, distorsion due au retard de l'attaque sur les transitoires) : 20:1 sur un genou de 10 dB à partir de −10 dBFS (donc 12 à 20:1 selon le niveau), attaque deux fois plus lente (avant : deux fois plus rapide), retour 0,7 fois plus court (pompage), surcompression adoucie au-delà de +8 dB, distorsion paire plus forte qu'aux autres ratios mais bornée.
5. Relâchement dépendant du programme recalculé à chaque échantillon (avant : par paliers de 128 échantillons) ; coupe-bas 18 Hz du transfo en bilinéaire pré-déformée : même réponse à 44,1 / 48 / 96 / 192 kHz (écart < 0,001 dB).

**Mesures avant (1.2) / après (2.0, défaut 4×)**

| Mesure | 1.2 | 2.0 |
|---|---|---|
| Repliement : sinus 9,5 kHz, −12 dBFS, Input +12, ratio 4 (raie repliée la plus forte, sous la note) | −39,6 dB | −99,2 dB (48 kHz), −91,7 dB (44,1 kHz) ; Off −41,3 ; 2× −89,8 ; 8× −94,7 |
| Même sinus, attaque 7 / relâchement 7, ratios 4 à 20, Input +12 et +24 (pire cas) | −32,2 dB (ratio 20) | −80,2 dB (2× : −73 dB, d'où le défaut 4×) |
| Ratios statiques mesurés (entre +10 et +20 dB au-dessus du seuil) | 4 / 8 / 12 / 20 (détection sur l'entrée) | 4,00 / 8,00 / 12,00 / 20,00 (détection sur la sortie), écart à la courbe 0,000 dB |
| Attaque mesurée (63 %, 44,1 kHz) pour 800 / 126 / 20 µs | 800 / 126 / 20 µs | 816 / 136 / 23 µs, à 4 comme à 20:1 |
| Stabilité, attaque 7 / relâchement 7, 44,1 kHz, Off | | aucune oscillation, aucun dépassement, aucune sous-harmonique |
| « All », Input +20, sinus −12 dBFS : THD | 5,0 % (H2 4,9 %) | 3,1 % (H2 2,9 %) |
| « All » : la sortie redescend, de son maximum à +20 dBFS d'entrée | 4,4 dB | 1,5 dB |

**Distorsion selon le niveau** (sinus 1 kHz, réglages par défaut : Input 0, ratio 4, attaque 4, relâchement 4 ; la compression commence vers −17 dBFS)

| Entrée | Réduction | THD 1.2 (H3 seule) | THD 2.0 | H2 / H3 2.0 |
|---|---|---|---|---|
| −36 dBFS | 0 dB | 0,00 % | 0,02 % | 0,02 / 0,00 % |
| −30 dBFS | 0 dB | 0,01 % | 0,04 % | 0,03 / 0,01 % |
| −24 dBFS | 0 dB | 0,05 % | 0,08 % | 0,07 / 0,04 % |
| **−18 dBFS (0 VU)** | 0 dB | 0,19 % | **0,21 %** | 0,14 / 0,15 % |
| −12 dBFS | −1,6 dB | 0,53 % | 0,51 % | 0,29 / 0,42 % |
| −6 dBFS | −6,0 dB | 0,80 % | 0,82 % | 0,55 / 0,61 % |
| 0 dBFS | −10,5 dB | 1,19 % | 1,23 % | 0,88 / 0,85 % |

**Interface** : flèche « Advanced settings » en bas à gauche, sur la ligne du petit Mix (16, 196, comme sur Lab Comp) ; panneau « Surface » (24, 240, 771 × 58, coins 12) : « Oversampling » puis le sélecteur Off / 2× / 4× / 8×, et la latence du mode choisi (« Latency 77 smp (1.60 ms) », « No latency »). Fenêtre 819 × 248 replié, 819 × 322 déplié ; l'état du panneau est enregistré avec la session.

### 7.2 Lab Opto

**Nouveaux réglages** (panneau « Advanced settings ») :
- **Oversampling** (`oversampling`) : Off / 2× / 4× / 8×, **défaut 2×**. Latence : Off 0, 2× 69, 4× 76, 8× 78 échantillons (à 48 kHz : 0 / 1,44 / 1,58 / 1,63 ms) ; AAX : 78 fixes. Même fondu et même recalage du son direct que le FET.
- **SC Emphasis** (`emphasis`) : 0 à 100 %, **défaut 0 %** (neutre). Emphase aiguë du side-chain, le réglage interne R37 de l'original : étagère de +0 à +9 dB au-dessus de 3 kHz, la cellule réagit plus aux aigus (sifflantes, cymbales). Lissée.

**Ce qui change dans le son**
1. **Étage à lampes asymétrique** : H2 dominante, 0,23 % de THD à −18 dBFS en sortie, qui monte avec le niveau (avant : tanh symétrique, H3 seule, déjà 0,62 % au niveau nominal).
2. **Suréchantillonnage** autour du moteur non linéaire (cellule et lampes).
3. **Filtres de sortie identiques à toutes les fréquences d'échantillonnage** : coupe-bas 25 Hz en bilinéaire pré-déformée, coupe-haut 18 kHz par un biquad ajusté par moindres carrés sur le 1er ordre analogique (−3,49 dB à 20 kHz), calculés au rythme de base.
4. **Détection après l'étage de gain** : la cellule voit la sortie (comme le T4 du LA-2A), avant le Gain de rattrapage, qui ne change donc toujours pas la compression. Loi de la boucle déduite de la courbe d'origine : même compression en régime établi. Les temps de la cellule (attaque 10 ms, relâchement 60 ms puis 0,5 à 5 s selon la mémoire) sont compensés dans la boucle pour rester ceux de l'original ; la mémoire dépend de l'éclairement de la cellule par rapport à son seuil (identique à la 1.2 au réglage par défaut, Peak Reduction 40).
5. **Détection stéréo sur la somme des puissances** √((L² + R²) / 2), au lieu de (L + R) / 2.
6. Seuil, ratio et emphase lissés sur 20 ms : tourner Peak Reduction ou passer en Limit ne fait plus de saut.

**Mesures avant (1.2) / après (2.0, défaut 2×)**

| Mesure | 1.2 | 2.0 |
|---|---|---|
| Repliement : sinus 9,5 kHz, −12 dBFS, Peak Reduction 50, Gain +6 | −28 dB | −97,3 dB (48 kHz), −113,1 dB (44,1 kHz) ; Off −50,1 ; 4× −111,2 ; 8× −142,6 |
| Même sinus à −6 dBFS, PR 50 et 100, Compress et Limit, Gain +6 et +12 (pire cas) | | −90,2 dB |
| Gain +20 (lampes saturées, sortie au-dessus de 0 dBFS) | | −70,9 dB en 2×, −112,3 dB en 4× : passer en 4× pour saturer fort |
| Au réglage de référence (−12 dBFS, PR 50, Gain +6) : H2 / H3 | −130 dB / −27 dB (4,4 %) | −44,1 dB / −47,9 dB |
| Réponse à 20 kHz : 44,1 / 48 / 96 / 192 kHz | −1,33 / −1,57 / −2,91 / −3,34 dB | −3,48 dB partout (écart au modèle ≤ 0,014 dB de 20 Hz à 20 kHz) |
| Signal en opposition de phase (L = −R), PR 80, −6 dBFS | aucune compression | −19,5 dB, comme L = R |
| Signal d'un seul côté | détecté 6 dB plus bas | 3 dB plus bas |
| Courbe statique (PR 40 / 80 / 100, Compress et Limit) | | écart 0,000 dB à la courbe d'origine |
| Attaque (63 % du niveau) ; 1er temps du retour ; mémoire après 2 s de signal dense ; après un coup bref de 30 ms | 10,0 ms ; −1,94 dB en 30 ms ; 5 s ; 0,5 à 2 s | 10,3 ms ; −1,99 dB ; 5,01 s ; 1,24 s |
| SC Emphasis 100 % : réduction pour un 300 Hz / un 10 kHz au même niveau (PR 70, −10 dBFS) | (non réglable) | −12,8 / −19,1 dB (0 % : −12,8 / −12,8 dB) |

**Distorsion selon le niveau de sortie** (sinus 1 kHz, sans compression : Peak Reduction 0)

| Sortie | THD 1.2 (H3 seule) | THD 2.0 | H2 / H3 2.0 |
|---|---|---|---|
| −36 dBFS | 0,01 % | 0,03 % | 0,03 / 0,00 % |
| −30 dBFS | 0,04 % | 0,06 % | 0,06 / 0,00 % |
| −24 dBFS | 0,16 % | 0,11 % | 0,11 / 0,01 % |
| **−18 dBFS (nominal)** | 0,62 % | **0,23 %** | 0,23 / 0,05 % |
| −12 dBFS | 2,35 % | 0,49 % | 0,45 / 0,20 % |
| −6 dBFS | 7,8 % | 1,16 % | 0,86 / 0,78 % |
| −3 dBFS | 12,8 % | 1,92 % | 1,17 / 1,51 % |

**Interface** : la rangée du bas étant pleine (VU, Compress / Limit, Mix), la flèche « Advanced settings » est en haut à gauche, au-dessus de Gain (8, 20) ; panneau « Surface » (24, 284, 526 × 92, coins 12) : ligne 1 « Oversampling », sélecteur et latence ; ligne 2 curseur « SC Emphasis » (teal secondaire, 300 px). Fenêtre 574 × 292 replié, 574 × 400 déplié.

### 7.3 Vérifications

- `tests/quality_test.cpp` (nouveau, sans JUCE, lancé sur GitHub avant la compilation Mac) : repliement, ratios, temps, stabilité, distorsion selon le niveau, réponse à 44,1 / 48 / 96 / 192 kHz, latence exacte ; Opto : en plus filtres conformes au modèle, détection stéréo, emphase, réglages sans clic. Seuils chiffrés, renvoie 1 en cas d'échec.
- `tests/processor_test.cpp` : session enregistrée puis relue, session 1.2 sans les nouveaux réglages, latence annoncée par mode, bypass = entrée retardée au bit près, Dry/Wet 0 % = son direct recalé, Dry/Wet 50 % sans peigne, mono, VU, plugin = moteur, bascule du suréchantillonnage en lecture sans clic.
- `tests/verify_dsp.py` : les comparaisons au code web sont retirées (le son a changé volontairement) ; restent l'empreinte de la copie de référence, l'indépendance à la taille des blocs, les courbes statiques et les temps.
- pluginval niveau 10 (Linux, VST3, avec interface) ; captures 100 %, 60 %, 200 %, panneau ouvert et fermé.
- Non testé ici : écoute réelle, AAX dans Pro Tools Developer, AU (auval tourne sur GitHub), Windows.

