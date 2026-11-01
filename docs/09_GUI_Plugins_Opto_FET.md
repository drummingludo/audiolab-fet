# 09 — Plugins Audio Lab Opto et Audio Lab FET (interface et moteur)

Deux plugins VST3 / AU / AAX tirés des outils « Opto » et « FET » d'Audio Lab, construits comme Audio Lab Compresseur :
même méthode, mêmes composants, même charte (voir 06_GUI_Plugin_Compresseur.md pour les potentiomètres, couleurs, polices, thèmes).

| | Audio Lab Opto | Audio Lab FET |
|---|---|---|
| Inspiré de | compresseur optique type LA-2A | compresseur FET type 1176 |
| Dépôt GitHub | drummingludo/audiolab-opto | drummingludo/audiolab-fet |
| Code plugin (AU) | `aufx AlOp Trsv` | `aufx AlFt Trsv` |
| Identifiant | fr.transversalstudio.audiolab.opto | fr.transversalstudio.audiolab.fet |
| Taille de base | 634 × 316 px | 879 × 272 px |

Noms : « LA-2A » et « 1176 » sont des marques d'Universal Audio, d'où « Opto » et « FET » (renommage possible à tout moment).

## 1. Moteur audio (identique au web)

- Portage ligne à ligne des classes `OptoDSP` (worklet `al-opto`) et `FetDSP` (worklet `al-fet`) d'Audio_Lab.html (version du 17/10/2026), en double précision.
- Les calculs faits « une fois par bloc de 128 échantillons » dans le web sont refaits au même rythme quel que soit le bloc envoyé par le logiciel hôte : sortie identique au web (écart < 10⁻⁶) à 44,1 / 48 / 96 kHz et pour des blocs de 1 à 4096 échantillons.
- Opto : seuil = 10 − 0,55 × Peak Reduction (dBFS), ratio 3,5:1 (Compress) ou 12:1 (Limit), knee 12 dB ; cellule optique attaque 10 ms, relâchement 60 ms puis 0,5 à 5 s selon la densité du programme ; étage à tubes (tanh), transfo (coupe-bas 25 Hz, coupe-haut 18 kHz).
- FET : seuil fixe, on compresse en poussant Input ; ratios 4:1 (seuil −14, knee 6), 8:1 (−11, 4), 12:1 (−9, 3), 20:1 (−7, 2), All (20:1, −10, knee 10, la sortie redescend au-delà de 8 dB, attaque deux fois plus rapide) ; attaque 800 µs → 20 µs, relâchement 1,1 s → 50 ms (positions 1 à 7, 7 = le plus rapide), relâchement jusqu'à 1,6 fois plus lent quand la réduction est forte ; harmoniques paires du FET, ampli classe A (tanh), transfo (coupe-bas 18 Hz).
- Commun : Dry/Wet à puissance constante, Bypass de l'hôte avec fondu de 20 ms (sortie = entrée exacte une fois le fondu fini), aucune latence.

## 2. VU-mètre (commun)

- Face encastrée (rayon 12, ombre intérieure noire 50 %, flou 11, décalage 4 px vers le bas), cadran SVG d'Audio Lab 330 × 200 posé sur le bas de la face (l'axe de l'aiguille reste caché).
- Aiguille : réponse VU normalisée du 2ᵉ ordre (99 % en 300 ms, dépassement ≈ 1,3 %).
- Mode GR : réduction de gain (mémoire de crête 60 ms), texte « x.x dB GR ».
- Modes de sortie : niveau mesuré sur 10 ms, convention AES17 (sinus de crête −18 dBFS = −18 dBFS) ; +4 → 0 VU = −18 dBFS, +8 → −14 dBFS, +10 → −12 dBFS ; texte « ±x.x VU ».
- Mode OFF (FET seulement) : aiguille au repos à gauche.
- Le mode choisi est enregistré avec la session.

## 3. Audio Lab Opto — 634 × 316

- Rangée principale (y 24 → 248) : Gain (potentiomètre moyen, 4,62 → 143 × 150, sous-titre « rattrapage de sortie », arc depuis 0 dB) · VU (152, 24, 330 × 224) · Peak Reduction (486, 62, 143 × 150, unité %, sous-titre « seuil + ratio couplés »).
- Rangée du bas (y 264, hauteur 28) : boutons VU « GR / +4 / +10 » à gauche (x 24) ; « Compress / Limit » centrés sous le VU ; petit Mix (28 px) aligné à droite (x 610).
- Paramètres : Gain −20 → +30 dB (0), Peak Reduction 0 → 100 (40), Mode Compress / Limit, Dry/Wet 0 → 100 % (100), Bypass.

## 4. Audio Lab FET — 879 × 272

- Colonnes de gauche à droite, toutes entre y 24 et y 248 :
  - Input (4, 70, 143 × 130), arc depuis −20 dB ; Output (131, 70, 143 × 130), arc depuis −30 dB.
  - Colonne Attack / Release centrée en x 313,5 : titre ATTACK, petit potentiomètre orange (Ø 76) centré y 87 ; second centré y 169 ; titre RELEASE puis « 7 = le plus rapide ». Valeurs affichées en µs / ms comme dans Audio Lab.
  - RATIO : boutons verticaux 20 / 12 / 8 / 4 / All (373, 46, 52 × 202).
  - VU (449, 24, 330 × 224).
  - METER : boutons verticaux GR / +8 / +4 / OFF (803, 46, 52 de large, même hauteur de bouton que Ratio), puis petit Mix aligné à droite en x 855, y 220 (bas aligné sur le VU).
- Paramètres : Input −20 → +30 dB (0), Output −30 → +20 dB (0), Attack 1 → 7 (4), Release 1 → 7 (4), Ratio 20 / 12 / 8 / 4 / All (4), Dry/Wet 0 → 100 % (100), Bypass.

## 5. Comportements communs

- Thème clair / sombre selon le système, fenêtre redimensionnable de 60 % à 200 % (proportions fixes), taille mémorisée.
- Bypass (depuis le logiciel hôte) : tout s'estompe à 35 % sauf les boutons du VU-mètre.
- Info-bulles en français sur chaque réglage (ce que fait le réglage, ce qu'il faut écouter).
- Presets, snapshots A/B/C : non inclus (presets via le logiciel hôte), comme Audio Lab Compresseur.

## 6. Vérifications faites

- `tests/verify_dsp.py` : plugin ↔ code web d'Audio Lab (empreinte SHA-256 de la copie de référence), courbes statiques, temps d'attaque / relâchement, harmoniques (FET).
- `tests/processor_test.cpp` : sauvegarde / rappel, bypass exact, mono, Dry/Wet 0 %, niveau du VU (AES17), FET « All » = moteur.
- `tests/render_ui.cpp` : captures clair / sombre, bypass, 60 % / 200 %.
- pluginval niveau 10 (Linux VST3, avec tests d'interface) ; sur GitHub : auval + pluginval niveau 10 (Mac VST3 + AU), pluginval (Windows VST3), contrôle de structure AAX.
- Non testé ici : écoute réelle dans un logiciel, Windows sur un vrai PC, AAX dans Pro Tools Developer.
