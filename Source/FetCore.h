// Moteur du compresseur FET d'Audio Lab (classe FetDSP du worklet « al-fet », type 1176),
// recopié ligne à ligne en C++ : mêmes formules, même ordre des calculs, en double précision.
// Seuil fixe, attaque 20–800 µs, relâchement 50–1100 ms dépendant du programme,
// distorsion paire du FET, ampli classe A (tanh), transfo de sortie (coupe-bas 18 Hz).
// Indépendant de JUCE : testé hors plugin contre le vrai code web (tests/verify_dsp.py).
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>

namespace audiolab
{
inline constexpr double kPi = 3.14159265358979323846;

struct FetParams
{
    double inDb = 0, outDb = 0;   // Input / Output (dB)
    double atk = 4, rel = 4;      // positions 1 à 7 (7 = le plus rapide)
    int ratio = 4;                // 4, 8, 12, 20 ou 0 (« All » : les 4 boutons enfoncés)
};

class FetCore
{
public:
    // Équivaut à « new FetDSP(sr) »
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        p = FetParams();
        grDb = 0; gIn = 1; gOut = 1; gInT = 1; gOutT = 1;
        hpLx1 = hpLy1 = hpRx1 = hpRy1 = 0;
        const double rc = 1 / (2 * kPi * 18); hpA = rc / (rc + 1 / sr);
        sm = std::exp (-1 / (0.02 * sr));
        setParams (p);
        blockPos = 0;
        grMinShared.store (0); pkShared.store (0);
    }

    void setParams (const FetParams& np)
    {
        p = np;
        // Attack 1..7 : 800 us -> 20 us ; Release 1..7 : 1100 ms -> 50 ms (7 = le plus rapide, comme sur l'original)
        const double ta = attackSeconds (p.atk);
        const double tr = releaseSeconds (p.rel);
        abm = p.ratio == 0;
        aA = std::exp (-1 / ((abm ? ta * 0.5 : ta) * sr));
        tRel = tr;
        // Ratio / seuil fixe / genou : seuil plus haut pour les ratios élevés (comportement de l'original)
        switch (p.ratio)
        {
            case 8:  R = 8;  thr = -11; knee = 4;  break;
            case 12: R = 12; thr = -9;  knee = 3;  break;
            case 20: R = 20; thr = -7;  knee = 2;  break;
            case 0:  R = 20; thr = -10; knee = 10; break;
            default: R = 4;  thr = -14; knee = 6;  break;
        }
        gInT = std::pow (10.0, p.inDb / 20); gOutT = std::pow (10.0, p.outDb / 20);
    }

    static double attackSeconds (double v)  { return 800e-6 * std::pow (20.0 / 800, (v - 1) / 6); }
    static double releaseSeconds (double v) { return 1.1 * std::pow (0.05 / 1.1, (v - 1) / 6); }

    double staticGR (double l) const
    {
        const double xg = l - thr, h = knee / 2, s = 1 / R - 1;
        double over;
        if (xg <= -h) over = 0; else if (xg >= h) over = xg; else { const double t = xg + h; over = t * t / (2 * knee); }
        double gr = s * over;
        if (abm && xg > 8) gr -= 0.25 * (xg - 8);   // « all buttons » : la sortie redescend quand on pousse
        return gr;
    }

    // Traitement : le web recalcule le relâchement une fois par bloc de 128 échantillons ;
    // on refait exactement pareil quelle que soit la taille des blocs envoyés par le logiciel.
    void process (const float* L, const float* R_, float* oL, float* oR, int n)
    {
        double grMin = 0, pk = 0;
        for (int i = 0; i < n; ++i)
        {
            if (blockPos == 0)
            {
                // relâchement dépendant du programme : plus lent quand la réduction est profonde
                aR = std::exp (-1 / (tRel * (1 + 0.6 * std::min (1.0, -grDb / 20)) * sr));
            }
            if (++blockPos == 128) blockPos = 0;

            gIn += (gInT - gIn) * (1 - sm);
            gOut += (gOutT - gOut) * (1 - sm);
            const double l = L[i] * gIn, r = R_[i] * gIn;
            const double a = std::max (std::abs (l), std::abs (r));
            const double lev = 20 * std::log10 (a > 1e-6 ? a : 1e-6);
            const double target = staticGR (lev);
            grDb = target < grDb ? target + (grDb - target) * aA : target + (grDb - target) * aR;
            if (grDb < grMin) grMin = grDb;
            const double g = std::pow (10.0, grDb / 20);
            // distorsion du FET : harmoniques paires qui augmentent avec la réduction (beaucoup plus en « all buttons »)
            const double depth = std::min (1.0, -grDb / 20);
            const double e = abm ? 0.06 + 0.35 * depth : 0.08 * depth;
            double yl = l * g, yr = r * g;
            yl = yl + e * yl * yl; yr = yr + e * yr * yr;
            // ampli de sortie classe A : saturation douce
            yl = std::tanh (yl * 1.2) / 1.2; yr = std::tanh (yr * 1.2) / 1.2;
            // couplage transfo (coupe-bas 18 Hz, retire aussi le continu créé par la distorsion paire)
            const double hl = hpA * (hpLy1 + yl - hpLx1); hpLx1 = yl; hpLy1 = hl;
            const double hr = hpA * (hpRy1 + yr - hpRx1); hpRx1 = yr; hpRy1 = hr;
            const double ol = hl * gOut, orr = hr * gOut;
            oL[i] = (float) ol; oR[i] = (float) orr;
            const double m = std::max (std::abs (ol), std::abs (orr)); if (m > pk) pk = m;
        }
        float cur = grMinShared.load();
        while ((float) grMin < cur && ! grMinShared.compare_exchange_weak (cur, (float) grMin)) {}
        float cp = pkShared.load();
        while ((float) pk > cp && ! pkShared.compare_exchange_weak (cp, (float) pk)) {}
    }

    float takeGRMin() { return grMinShared.exchange (0.0f); }
    float takePeak()  { return pkShared.exchange (0.0f); }

private:
    double sr = 48000;
    FetParams p;
    double grDb = 0, gIn = 1, gOut = 1, gInT = 1, gOutT = 1;
    double hpLx1 = 0, hpLy1 = 0, hpRx1 = 0, hpRy1 = 0;
    double hpA = 0, sm = 0;
    bool abm = false;
    double aA = 0, aR = 0, tRel = 0.2;
    double R = 4, thr = -14, knee = 6;
    int blockPos = 0;
    std::atomic<float> grMinShared { 0 }, pkShared { 0 };
};
} // namespace audiolab
