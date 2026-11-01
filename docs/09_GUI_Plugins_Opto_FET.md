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
| Taille de base | 574 × 292 px | 819 × 248 px |

Noms : « LA-2A » et « 1176 » sont des marques d'Universal Audio, d'où « Opto » et « FET » ; la charte interdit les noms de marques matérielles dans les plugins (description affichée par le logiciel hôte : « Compresseur optique / FET d'Audio Lab »).

## 1. Moteur audio (identique au web)

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
