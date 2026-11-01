# Audio Lab FET

Le compresseur FET d'**Audio Lab** (« Chimiste du son », Transversal Studio), de type 1176, en plugin **VST3 / AU** pour Logic Pro, Ableton Live, FL Studio, Cubase, Reaper, Studio One, Bitwig…, et **AAX** pour Pro Tools (version de test non signée, voir plus bas).

- **Même son que l'outil web** : le moteur est un portage ligne à ligne du processeur `al-fet` d'`Audio_Lab.html`. Le banc de test compare les deux versions : sortie identique (écart < 10⁻⁶) à 44,1 / 48 / 96 kHz, quelle que soit la taille des blocs du logiciel.
- **Réglages de l'original** : Input (seuil fixe : on compresse en poussant l'entrée), Output, Attack 800 µs → 20 µs et Release 1,1 s → 50 ms (positions 1 à 7, 7 = le plus rapide), Ratio 4 / 8 / 12 / 20 et « All » (les quatre boutons enfoncés). Distorsion paire du FET, ampli classe A, transfo.
- **Interface Audio Lab** : potentiomètres à arc, VU-mètre à aiguille (GR / +8 / +4 / OFF), petit Dry/Wet, thème clair / sombre, fenêtre redimensionnable, info-bulles en français.

Plan détaillé de l'interface et du moteur : `docs/09_GUI_Plugins_Opto_FET.md`.

## Télécharger

Chaque fabrication produit deux archives (onglet **Actions** > dernière fabrication > *Artifacts*) :
- `AudioLabFET-Mac.zip` : VST3 + AU, Apple Silicon et Intel, avec un installateur en un double-clic.
- `AudioLabFET-Windows.zip` : VST3 64 bits.

Pour une version durable sur la page **Releases**, créer un tag `v…` : la fabrication y dépose alors les deux archives.
Mode d'emploi : fichier `LISEZ-MOI.txt` dans chaque archive.

### Pro Tools (AAX)

Chaque archive contient aussi `Audio Lab FET.aaxplugin` (Mac universel, Windows 64 bits), fabriqué avec le SDK AAX fourni par JUCE (GPL v3). Il n'est **pas encore signé** par PACE : il ne se charge que dans **Pro Tools Developer**. Aucune latence : rien de particulier à régler dans Pro Tools.

## Fabrication

Chaque envoi sur `main` déclenche la fabrication sur des machines Mac et Windows de GitHub, puis la validation automatique (`auval` pour l'AU, `pluginval` niveau 10 pour VST3 et AU ; pour l'AAX, contrôle de structure, d'architectures et du point d'entrée — pluginval ne sait pas charger l'AAX). Un tag `v…` publie la version.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Tests (Linux ou Mac) :

```
cmake -B build -DALF_TESTS=ON -DALF_STANDALONE=ON
cmake --build build --target ALFProcessorTest ALFRenderUI
g++ -O2 -std=c++17 -ISource tests/core_run.cpp -o tests/core_run
python3 tests/verify_dsp.py     # moteur C++ ↔ code web d'Audio Lab (Node.js requis)
```

## Licence

Code sous **GNU AGPLv3** (voir `LICENSE`), comme la version libre du framework [JUCE](https://juce.com) qu'il utilise. Police Inter sous licence SIL OFL 1.1 (`Resources/Inter-OFL-LICENSE.txt`).

© Transversal Studio — Ludovic Diaz
