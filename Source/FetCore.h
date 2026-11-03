// Moteur du compresseur FET d'Audio Lab (type 1176), version 2.0 « pro ».
// Point de départ : la classe FetDSP du worklet « al-fet » d'Audio_Lab.html (même seuil fixe, mêmes ratios,
// mêmes temps), améliorée pour le plugin (la page web ne change pas pour l'instant) :
//  - détection APRÈS l'étage de gain (contre-réaction, comme le 1176), résolue sans retard d'un échantillon
//    (boucle « sans délai » résolue exactement à chaque échantillon : stable à tous les réglages) ;
//    la loi de gain de la boucle est déduite de la courbe d'origine : mêmes ratios statiques 4 / 8 / 12 / 20 ;
//  - le moteur non linéaire tourne au rythme suréchantillonné (Oversampler.h, ×1 à ×8) : plus de repliement ;
//    en ×2 / ×4 / ×8, l'attaque du détecteur lit la vraie crête entre deux échantillons (3 points intermédiaires,
//    sinc fenêtré de 8 coefficients, retenus seulement s'ils forment une crête plus haute que les deux échantillons) :
//    la réduction ne dépend plus de la position des échantillons sur la sinusoïde (sinon un aigu fort module
//    le gain et crée des raies parasites) ; 1 échantillon de latence en plus (2 en ×2) ;
//  - calibration 0 VU = −18 dBFS : étage de sortie classe A asymétrique (H2 + H3 ≈ 0,2 % au niveau nominal),
//    distorsion qui monte vers 0 dBFS ;
//  - mode « All » revu d'après le manuel UA : ratio 12 à 20:1 à genou large, attaque retardée sur les transitoires,
//    retour plus rapide, légère surcompression aux niveaux forts, distorsion paire plus marquée mais bornée ;
//  - relâchement dépendant du programme recalculé à chaque échantillon (plus de paliers de 128 échantillons) ;
//  - transfo de sortie (coupe-bas 18 Hz) en transformée bilinéaire pré-déformée : identique à toutes les fréquences.
// Indépendant de JUCE : testé hors plugin (tests/quality_test.cpp).
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include "Oversampler.h"

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
    // Constantes de l'étage de sortie classe A (calibration 0 VU = −18 dBFS) : t = tanh (k·y) / k ; y = t + c2·t²
    static constexpr double outK = 1.08, outC2 = 0.022;
    // Mode « All » : attaque retardée (×2), retour plus rapide (×0,7), surcompression au-delà de +8 dB
    static constexpr double allAtkMul = 2.0, allRelMul = 0.7, allOverSlope = 0.12;

    // baseRate : fréquence du logiciel hôte ; factor : suréchantillonnage (1, 2, 4 ou 8).
    // La partie non linéaire tourne à baseRate × factor, la partie linéaire de sortie au rythme de base.
    void prepare (double baseRate, int factor)
    {
        srBase = baseRate; fac = std::max (1, factor); srHi = srBase * fac;
        // transfo de sortie : coupe-bas 18 Hz du 1er ordre, bilinéaire pré-déformée (même réponse à toutes les fréquences)
        const double K = std::tan (kPi * 18.0 / srBase);
        hpB0 = 1 / (1 + K); hpA1 = (1 - K) / (1 + K);
        smHi = std::exp (-1 / (0.02 * srHi));
        // retard du chemin audio au rythme haut pour l'interpolation du détecteur (≥ 3, multiple du facteur) :
        // 4 en ×2 (2 échantillons de base), 4 en ×4 et 8 en ×8 (1 échantillon de base)
        dly = fac > 1 ? std::max (fac, 4) : 0;
        smBase = std::exp (-1 / (0.02 * srBase));
        reset();
        setParams (p);
        gIn = gInT; gOut = gOutT; allMix = allT;
    }

    void reset()
    {
        grFb = 0; grOv = 0;
        for (auto* hb : { hL, hR }) std::fill (hb, hb + histLen, 0.0);
        hPos = 0;
        hpLx1 = hpLy1 = hpRx1 = hpRy1 = 0;
        grMinShared.store (0); pkShared.store (0);
    }

    void setParams (const FetParams& np)
    {
        p = np;
        // Attack 1..7 : 800 µs → 20 µs ; Release 1..7 : 1100 ms → 50 ms (7 = le plus rapide, comme sur l'original)
        const bool all = p.ratio == 0;
        allT = all ? 1.0 : 0.0;
        // Ratio / seuil fixe / genou : seuil plus haut pour les ratios élevés (comportement de l'original)
        switch (p.ratio)
        {
            case 8:  R = 8;  thr = -11; knee = 4;  break;
            case 12: R = 12; thr = -9;  knee = 3;  break;
            case 20: R = 20; thr = -7;  knee = 2;  break;
            case 0:  R = 20; thr = -10; knee = 10; break;   // All : 12 à 20:1 sur un genou large
            default: R = 4;  thr = -14; knee = 6;  break;
        }
        s = 1 / R - 1; h = knee / 2;
        const double ta = attackSeconds (p.atk) * (all ? allAtkMul : 1.0);
        // Boucle de contre-réaction : au-dessus du seuil, elle accélère l'attaque d'un facteur R.
        // On compense pour que le temps d'attaque mesuré reste celui affiché, quel que soit le ratio.
        const double m = std::exp (-1 / (ta * srHi));
        aA = m * R / (1 + m * (R - 1));
        aOvA = m;
        tRel = releaseSeconds (p.rel) * (all ? allRelMul : 1.0);
        relK = 1 / (tRel * srHi);
        gInT = std::pow (10.0, p.inDb / 20); gOutT = std::pow (10.0, p.outDb / 20);
    }

    int factor() const { return fac; }
    // latence propre du moteur, en échantillons de base (retard du détecteur interpolé) : 0, 2, 1, 1
    int latency() const { return dly / fac; }
    static double attackSeconds (double v)  { return 800e-6 * std::pow (20.0 / 800, (v - 1) / 6); }
    static double releaseSeconds (double v) { return 1.1 * std::pow (0.05 / 1.1, (v - 1) / 6); }

    // Courbe statique de la version web (réduction en dB pour un niveau d'entrée en dB, après Input) :
    // la boucle de contre-réaction est construite pour la reproduire exactement en régime établi.
    static double staticGR (double l, int ratio)
    {
        FetCore c; FetParams q; q.ratio = ratio; c.setParams (q);
        return c.ffCurve (l) + (ratio == 0 ? c.overCurve (l) : 0.0);
    }

    // Partie non linéaire, au rythme haut, sur place (l, r : n échantillons hauts)
    void processHigh (float* l, float* r, int n)
    {
        double grMin = 0;
        const double k20 = std::log (10.0) / 20;
        for (int i = 0; i < n; ++i)
        {
            gIn += (gInT - gIn) * (1 - smHi);
            allMix += (allT - allMix) * (1 - smHi);
            double xl, xr, a, aS;
            bool interior = false;
            if (dly == 0)
            {
                xl = l[i] * gIn; xr = r[i] * gIn;
                a = aS = std::max (std::abs (xl), std::abs (xr));
            }
            else
            {
                // historique : on traite l'échantillon d'il y a « dly » (m) ; crête éventuelle entre m − 1 et m interpolée
                hPos = (hPos + 1) & (histLen - 1);
                hL[hPos] = l[i]; hR[hPos] = r[i];
                const int m = hPos - dly;
                double wl[8], wr[8];
                for (int j = 0; j < 8; ++j) { const int q = (m - 4 + j) & (histLen - 1); wl[j] = hL[q]; wr[j] = hR[q]; }
                xl = wl[4] * gIn; xr = wr[4] * gIn;
                aS = std::max (std::abs (wl[4]), std::abs (wr[4]));
                double ml = 0, mr = 0;
                for (int f = 0; f < 3; ++f)
                {
                    double il = 0, ir = 0;
                    for (int j = 0; j < 8; ++j) { il += interp.c[f][j] * wl[j]; ir += interp.c[f][j] * wr[j]; }
                    ml = std::max (ml, std::abs (il)); mr = std::max (mr, std::abs (ir));
                }
                // seulement une vraie crête intérieure (sinon ce serait un maintien de crête d'un échantillon : biais)
                double pk = aS;
                if (ml > std::max (std::abs (wl[3]), std::abs (wl[4])) && ml > pk) { pk = ml; interior = true; }
                if (mr > std::max (std::abs (wr[3]), std::abs (wr[4])) && mr > pk) { pk = mr; interior = true; }
                a = pk * gIn; aS *= gIn;
            }
            const double x = 20 * std::log10 (a > 1e-6 ? a : 1e-6);                      // attaque : crête vraie
            const double xs = interior ? 20 * std::log10 (aS > 1e-6 ? aS : 1e-6) : x;    // relâchement : niveau instantané
            // relâchement dépendant du programme : plus lent quand la réduction est profonde (recalculé à chaque échantillon) ;
            // exp (−u) par son développement à l'ordre 3 (u < 1e−3 : erreur < 1e−13)
            const double u = relK / (1 + 0.6 * std::min (1.0, -(grFb + grOv) / 20));
            const double aR = 1 - u * (1 - u * (0.5 - u / 6));
            // boucle de contre-réaction : le détecteur lit la sortie de l'étage de gain (x + gr), résolue exactement
            const double ga = solve (x, grFb, aA);
            grFb = ga < grFb ? ga : solve (xs, grFb, aR);
            // « All » : surcompression (la sortie redescend un peu quand on pousse), par anticipation sur l'entrée
            const double tOv = allMix > 1e-4 ? allMix * overCurve (x) : 0.0;
            grOv = tOv + (grOv - tOv) * (tOv < grOv ? aOvA : aR);
            const double gr = grFb + grOv;
            if (gr < grMin) grMin = gr;
            const double g = std::exp (gr * k20);
            // distorsion du FET : harmoniques paires qui augmentent avec la réduction (plus en « All »), bornées
            const double depth = std::min (1.0, -gr / 20);
            const double e = (1 - allMix) * 0.08 * depth + allMix * (0.04 + 0.16 * depth);
            l[i] = (float) stage (xl * g, e);
            r[i] = (float) stage (xr * g, e);
        }
        float cur = grMinShared.load();
        while ((float) grMin < cur && ! grMinShared.compare_exchange_weak (cur, (float) grMin)) {}
    }

    // Partie linéaire, au rythme de base, sur place : transfo (coupe-bas 18 Hz, retire le continu de la distorsion paire) + Output
    void processOut (float* l, float* r, int n)
    {
        double pk = 0;
        for (int i = 0; i < n; ++i)
        {
            gOut += (gOutT - gOut) * (1 - smBase);
            const double yl = l[i], yr = r[i];
            const double hl = hpB0 * (yl - hpLx1) + hpA1 * hpLy1; hpLx1 = yl; hpLy1 = hl;
            const double hr = hpB0 * (yr - hpRx1) + hpA1 * hpRy1; hpRx1 = yr; hpRy1 = hr;
            const double ol = hl * gOut, orr = hr * gOut;
            l[i] = (float) ol; r[i] = (float) orr;
            const double m = std::max (std::abs (ol), std::abs (orr)); if (m > pk) pk = m;
        }
        float cp = pkShared.load();
        while ((float) pk > cp && ! pkShared.compare_exchange_weak (cp, (float) pk)) {}
    }

    float takeGRMin() { return grMinShared.exchange (0.0f); }
    float takePeak()  { return pkShared.exchange (0.0f); }

private:
    // courbe « directe » d'origine (genou quadratique)
    double ffCurve (double x) const
    {
        const double xg = x - thr;
        if (xg <= -h) return 0;
        if (xg >= h) return s * xg;
        const double t = xg + h; return s * t * t / (2 * knee);
    }
    // « All » : surcompression de −0,12 dB par dB au-delà de +8 dB au-dessus du seuil (genou de 4 dB)
    double overCurve (double x) const
    {
        const double xg = x - thr - 8;
        if (xg <= -2) return 0;
        if (xg >= 2) return -allOverSlope * xg;
        const double t = xg + 2; return -allOverSlope * t * t / 8;
    }
    // Boucle sans délai : g = a·g1 + (1 − a)·T(x + g), où T est la loi de la boucle (lue sur la sortie).
    // T est construite pour que l'équilibre soit la courbe d'origine : en posant x' = l'entrée « équivalente »,
    // l'équation devient x' + a·gr(x') = x + a·g1 (fonction strictement croissante : une seule solution, exacte).
    double solve (double x, double g1, double a) const
    {
        const double c = x + a * g1;
        double xp;
        if (c <= thr - h) xp = c;
        else if (c >= thr + h + a * s * h) xp = (c + a * s * thr) / (1 + a * s);
        else
        {
            const double cp = c - thr + h;
            xp = 2 * cp / (1 + std::sqrt (std::max (0.0, 1 + 2 * a * s * cp / knee))) + thr - h;
        }
        return std::min (0.0, xp + ffCurve (xp) - x);
    }
    // étage de gain → FET (H2 bornée) → ampli classe A asymétrique
    static double stage (double y, double e)
    {
        y += e * y * y / (1 + 0.25 * y * y);
        const double t = std::tanh (outK * y) / outK;
        return t + outC2 * t * t;
    }

    // Interpolateur du détecteur : sinc fenêtré (Kaiser β = 6), 8 coefficients, aux positions 1/4, 1/2, 3/4
    // entre x[m−1] et x[m] (échantillons x[m−4] … x[m+3]) ; gain 1 au continu, ±0,1 % jusqu'à 0,23 fs.
    struct Interp
    {
        double c[3][8];
        Interp()
        {
            auto I0 = [] (double v) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (v / 2) / k; s += t * t; } return s; };
            const double beta = 6.0;
            for (int f = 0; f < 3; ++f)
            {
                const double mu = 0.25 * (f + 1);           // position après x[m−1]
                double sum = 0;
                for (int j = 0; j < 8; ++j)
                {
                    const double d = (j - 3) - mu;          // distance de l'échantillon x[m−4+j] au point interpolé
                    const double sn = std::abs (d) < 1e-12 ? 1.0 : std::sin (kPi * d) / (kPi * d);
                    const double r = d / 4.0;
                    c[f][j] = sn * (std::abs (r) < 1 ? I0 (beta * std::sqrt (1 - r * r)) / I0 (beta) : 0.0);
                    sum += c[f][j];
                }
                for (int j = 0; j < 8; ++j) c[f][j] /= sum;
            }
        }
    };
    static constexpr int histLen = 16;
    inline static const Interp interp {};

    double srBase = 48000, srHi = 48000;
    int fac = 1, dly = 0, hPos = 0;
    double hL[histLen] = {}, hR[histLen] = {};
    FetParams p;
    double grFb = 0, grOv = 0;
    double gIn = 1, gOut = 1, gInT = 1, gOutT = 1, allMix = 0, allT = 0;
    double hpLx1 = 0, hpLy1 = 0, hpRx1 = 0, hpRy1 = 0, hpB0 = 1, hpA1 = 0;
    double smHi = 0, smBase = 0;
    double aA = 0, aOvA = 0, tRel = 0.2, relK = 1e-4;
    double R = 4, thr = -14, knee = 6, s = -0.75, h = 3;
    std::atomic<float> grMinShared { 0 }, pkShared { 0 };
};

// Chaîne complète au rythme de base : montée → moteur non linéaire → descente → sortie.
// Les trois suréchantillonneurs sont préparés d'avance (aucune allocation au changement de mode).
class FetChain
{
public:
    static constexpr int maxChunk = 512;
    // facteur pour un choix 0..3 (Off, 2×, 4×, 8×)
    static int factorFor (int choice) { return 1 << std::clamp (choice, 0, 3); }

    void prepare (double baseRate, int choice)
    {
        sr = baseRate;
        for (int s = 1; s <= 3; ++s) os[s - 1].prepare (s, 2, maxChunk);
        setOversampling (choice);
    }
    // à appeler hors traitement ou pendant un fondu à zéro : remet le moteur et les filtres à zéro
    void setOversampling (int choice)
    {
        mode = std::clamp (choice, 0, 3);
        if (mode > 0) os[mode - 1].reset();
        core.prepare (sr, factorFor (mode));
    }
    int oversampling() const { return mode; }
    int latency() const { return latencyFor (mode); }
    // latence de chaque choix : 0 / 71 / 77 / 79 échantillons (suréchantillonneur 69 / 76 / 78 + détecteur 2 / 1 / 1)
    int latencyFor (int choice) const { return choice > 0 ? os[std::clamp (choice, 1, 3) - 1].latency() + (choice == 1 ? 2 : 1) : 0; }
    int maxLatency() const { return latencyFor (3); }

    void setParams (const FetParams& p) { core.setParams (p); }
    FetCore& engine() { return core; }

    // traitement d'un bloc quelconque (découpé en morceaux de maxChunk)
    void process (const float* L, const float* R, float* oL, float* oR, int n)
    {
        for (int off = 0; off < n; off += maxChunk)
        {
            const int m = std::min (maxChunk, n - off);
            if (mode == 0)
            {
                std::copy (L + off, L + off + m, oL + off);
                std::copy (R + off, R + off + m, oR + off);
                core.processHigh (oL + off, oR + off, m);
            }
            else
            {
                const float* in[2] = { L + off, R + off };
                auto& o = os[mode - 1];
                float* const* hi = o.upsample (in, m);
                core.processHigh (hi[0], hi[1], m * o.factor());
                float* out[2] = { oL + off, oR + off };
                o.downsample (out, m);
            }
            core.processOut (oL + off, oR + off, m);
        }
    }

private:
    double sr = 48000;
    int mode = 0;
    os::Oversampler os[3];
    FetCore core;
};
} // namespace audiolab
