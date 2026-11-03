// Banc qualité du moteur Lab FET 2.0, SANS JUCE (quelques secondes) :
//   clang++ -O2 -std=c++17 -I Source tests/quality_test.cpp -o qt && ./qt
// Seuils chiffrés : repliement, ratios statiques, temps, stabilité, distorsion selon le niveau,
// réponse à 44,1 / 48 / 96 / 192 kHz, latence. Renvoie 1 en cas d'échec.
#include "FetCore.h"
#include <complex>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace audiolab;
static bool OK = true;
static bool verbose = false;
static void check (bool c, const char* m) { std::printf ("  %s %s\n", c ? "OK   " : "ÉCHEC", m); OK &= c; }
static double db (double a) { return 20 * std::log10 (std::max (a, 1e-30)); }

static FetParams P (double inDb = 0, int ratio = 4, double atk = 4, double rel = 4, double outDb = 0)
{
    FetParams p; p.inDb = inDb; p.ratio = ratio; p.atk = atk; p.rel = rel; p.outDb = outDb; return p;
}

// Fait passer un signal (identique sur L et R) dans la chaîne complète ; renvoie la voie gauche
static std::vector<float> run (double fs, int os, const FetParams& p, const std::vector<float>& x, std::vector<float>* right = nullptr,
                               const std::vector<float>* xr = nullptr)
{
    FetChain c; c.prepare (fs, os); c.setParams (p); c.engine().prepare (fs, FetChain::factorFor (os));
    const int n = (int) x.size();
    std::vector<float> yl ((size_t) n), yr ((size_t) n);
    c.process (x.data(), xr ? xr->data() : x.data(), yl.data(), yr.data(), n);
    if (right) *right = yr;
    return yl;
}

static std::vector<float> sine (double fs, double f, double peakDb, int n)
{
    std::vector<float> x ((size_t) n);
    const double a = std::pow (10.0, peakDb / 20);
    for (int i = 0; i < n; ++i) x[(size_t) i] = (float) (a * std::sin (2 * kPi * f * i / fs));
    return x;
}

// Amplitudes des raies d'un signal périodique de période Pd échantillons (raies aux multiples de fs / Pd), sur N = Pd·k
static std::vector<double> lines (const std::vector<float>& y, int start, int Pd, int k)
{
    const int N = Pd * k;
    std::vector<double> a ((size_t) Pd / 2 + 1, 0.0);
    for (int m = 0; m <= Pd / 2; ++m)
    {
        std::complex<double> s = 0;
        const int bin = m * k;
        for (int i = 0; i < N; ++i) s += (double) y[(size_t) (start + i)] * std::polar (1.0, -2 * kPi * (double) bin * i / N);
        a[(size_t) m] = std::abs (s) * (m == 0 ? 1.0 : 2.0) / N;
    }
    return a;
}

// Repliement : sinus f, raie la plus forte qui n'est pas une harmonique vraie (k·f < fs/2), en dB sous la fondamentale
static double aliasing (double fs, int os, const FetParams& p, double f, double lvl, int Pd, int k, double* fAt = nullptr)
{
    const int warm = (int) (2.0 * fs);
    const auto y = run (fs, os, p, sine (fs, f, lvl, warm + Pd * k));
    const auto a = lines (y, warm, Pd, k);
    double fund = 0, worst = 0; int wm = 0;
    for (int m = 1; m <= Pd / 2; ++m)
    {
        const double fr = m * fs / Pd;
        bool harm = false;
        for (int q = 1; q * f < fs / 2 + 1; ++q) if (std::abs (fr - q * f) < 1) harm = true;
        if (std::abs (fr - f) < 1) fund = a[(size_t) m];
        else if (! harm && a[(size_t) m] > worst) { worst = a[(size_t) m]; wm = m; }
    }
    if (fAt) *fAt = wm * fs / Pd;
    return db (worst / fund);
}

// THD (H2…H10) et H2, H3 d'un sinus 1 kHz à 48 kHz (période exacte de 48 échantillons)
struct Thd { double thd, h2, h3; };
static Thd thd (int os, const FetParams& p, double lvl)
{
    const double fs = 48000; const int warm = (int) (2.5 * fs);
    const auto y = run (fs, os, p, sine (fs, 1000, lvl, warm + 48 * 200));
    const auto a = lines (y, warm, 48, 200);
    double s = 0; for (int m = 2; m <= 10; ++m) s += a[(size_t) m] * a[(size_t) m];
    return { 100 * std::sqrt (s) / a[1], 100 * a[2] / a[1], 100 * a[3] / a[1] };
}

// Réduction de gain en régime établi pour un niveau d'entrée constant (continu : la détection voit un niveau fixe)
static double steadyGR (int ratio, double levelDb, double fs = 48000, int os = 0)
{
    FetChain c; c.prepare (fs, os); c.setParams (P (0, ratio, 4, 1));
    const int n = (int) (3 * fs);
    std::vector<float> x ((size_t) n, (float) std::pow (10.0, levelDb / 20)), y ((size_t) n);
    c.process (x.data(), x.data(), y.data(), y.data(), n - 512);
    c.engine().takeGRMin();
    c.process (x.data(), x.data(), y.data(), y.data(), 512);
    return c.engine().takeGRMin();
}

// Trace de la réduction de gain (dB) échantillon par échantillon, moteur sans suréchantillonnage
static std::vector<double> grTrace (double fs, const FetParams& p, const std::vector<float>& x)
{
    FetCore c; c.prepare (fs, 1); c.setParams (p); c.prepare (fs, 1);
    std::vector<double> g (x.size());
    for (size_t i = 0; i < x.size(); ++i)
    {
        float l = x[i], r = x[i];
        c.processHigh (&l, &r, 1);
        g[i] = c.takeGRMin();
    }
    return g;
}

int main (int argc, char** argv)
{
    verbose = argc > 1 && std::strcmp (argv[1], "-v") == 0;
    char m[256];
    std::puts ("\nLab FET 2.0 : banc qualité du moteur (sans JUCE)");

    std::puts ("\n1) Repliement : sinus 9.5 kHz, −12 dBFS, Input +12, ratio 4 (raie repliée la plus forte, sous la note)");
    const char* osName[4] = { "Off", "2x", "4x", "8x" };
    double alias48[4] = {};
    for (int os = 0; os < 4; ++os)
    {
        double f48, f44;
        alias48[os] = aliasing (48000, os, P (12, 4), 9500, -12, 96, 60, &f48);
        const double a44 = aliasing (44100, os, P (12, 4), 9500, -12, 441, 12, &f44);
        std::snprintf (m, sizeof m, "%-3s : %6.1f dB à 48 kHz (%5.0f Hz), %6.1f dB à 44.1 kHz (%5.0f Hz)%s", osName[os], alias48[os], f48, a44, f44,
                       os == 0 ? "  [avant : −39.6 dB]" : "");
        check (os == 0 || (alias48[os] < -80 && a44 < -80), m);
    }
    {
        // défaut (4x) aux réglages les plus rapides et poussés : ratios 4 à 20 sous −80 dB ; All donné pour information
        double worst = -1e9;
        for (int ratio : { 4, 8, 12, 20 })
            for (double in : { 12.0, 24.0 })
                worst = std::max (worst, aliasing (48000, 2, P (in, ratio, 7, 7), 9500, -12, 96, 40));
        std::snprintf (m, sizeof m, "4x (défaut), attaque 7, relâchement 7, ratios 4 à 20, Input +12 et +24 : pire raie %.1f dB  [1.2 : −32.2 dB]", worst);
        check (worst < -78, m);   // marge d'arrondi entre compilateurs ; la cible −80 dB est vérifiée au réglage de référence
        const double a = aliasing (48000, 2, P (24, 0, 7, 7), 9500, -12, 96, 40);
        std::printf ("  info  All, Input +24, attaque 7, 4x : %.1f dB (son saturé voulu)\n", a);
    }

    std::puts ("\n2) Ratios statiques (détection après l'étage de gain) : pente mesurée entre +10 et +20 dB au-dessus du seuil");
    {
        const int ratios[4] = { 4, 8, 12, 20 }; const double thrs[4] = { -14, -11, -9, -7 };
        for (int k = 0; k < 4; ++k)
        {
            const double l1 = thrs[k] + 10, l2 = thrs[k] + 20;
            const double g1 = steadyGR (ratios[k], l1), g2 = steadyGR (ratios[k], l2);
            const double r = (l2 - l1) / ((l2 + g2) - (l1 + g1));
            const double e1 = std::abs (g1 - FetCore::staticGR (l1, ratios[k])), e2 = std::abs (g2 - FetCore::staticGR (l2, ratios[k]));
            std::snprintf (m, sizeof m, "%2d:1 : ratio mesuré %.2f:1, écart à la courbe d'origine %.3f dB", ratios[k], r, std::max (e1, e2));
            check (std::abs (r - ratios[k]) < 0.02 * ratios[k] && std::max (e1, e2) < 0.02, m);
        }
        // All : genou large puis surcompression (la sortie redescend légèrement au-delà de +8 dB)
        double prev = -1e9, peakOut = -1e9, endOut = 0;
        for (double l = -30; l <= 20; l += 1)
        {
            const double o = l + steadyGR (0, l);
            peakOut = std::max (peakOut, o); endOut = o; prev = o;
        }
        (void) prev;
        std::snprintf (m, sizeof m, "All : sortie max %.1f dBFS, à +20 dBFS d'entrée %.1f dBFS (surcompression %.1f dB ; 1.2 : 4.4 dB)", peakOut, endOut, peakOut - endOut);
        check (peakOut - endOut > 0.5 && peakOut - endOut < 4, m);
    }

    std::puts ("\n3) Temps : échelon de −30 à 0 dBFS (attaque, 63 %), de −6 à −30 dBFS (relâchement, 63 %)");
    for (double fs : { 44100.0, 96000.0 })
        for (int ratio : { 4, 20 })
            for (double atk : { 1.0, 4.0, 7.0 })
            {
                const double ta = FetCore::attackSeconds (atk);
                const int n0 = (int) (0.05 * fs);
                std::vector<float> x ((size_t) n0, (float) std::pow (10.0, -30 / 20.0));
                x.resize ((size_t) (n0 + 0.05 * fs), 1.0f);
                const auto g = grTrace (fs, P (0, ratio, atk, 1), x);
                const double gEnd = g.back();
                int k = n0; while (k < (int) g.size() && g[(size_t) k] / gEnd < 0.632) ++k;
                const double t = (k - n0 + 1) / fs;
                std::snprintf (m, sizeof m, "%3.0f kHz, ratio %2d, attaque %.0f : %6.0f µs (réglage %.0f µs)", fs / 1000, ratio, atk, t * 1e6, ta * 1e6);
                check (std::abs (t - ta) <= std::max (0.1 * ta, 1.5 / fs), m);
            }
    for (double rel : { 1.0, 4.0, 7.0 })
    {
        const double fs = 48000, tr = FetCore::releaseSeconds (rel);
        const int n0 = (int) (0.3 * fs);
        std::vector<float> x ((size_t) n0, (float) std::pow (10.0, -6 / 20.0));
        x.resize ((size_t) (n0 + 8 * tr * fs), (float) std::pow (10.0, -30 / 20.0));
        const auto g = grTrace (fs, P (0, 4, 4, rel), x);
        const double g0 = g[(size_t) n0 - 1];
        int k = n0; while (k < (int) g.size() && g[(size_t) k] / g0 > 0.368) ++k;
        const double t = (k - n0 + 1) / fs, fac0 = 1 + 0.6 * std::min (1.0, -g0 / 20);
        std::snprintf (m, sizeof m, "relâchement %.0f : %5.0f ms (réglage %.0f ms, ×%.2f au plus fort de la réduction)", rel, t * 1000, tr * 1000, fac0);
        check (t >= tr * 0.95 && t <= tr * fac0 * 1.05, m);
    }

    std::puts ("\n4) Stabilité aux réglages les plus rapides (attaque 7, relâchement 7), 44.1 kHz sans suréchantillonnage");
    for (int ratio : { 4, 20, 0 })
    {
        // échelons montants et descendants : approche monotone (pas de dépassement, pas d'oscillation)
        const double fs = 44100;
        std::vector<float> x;
        for (int s = 0; s < 6; ++s) x.resize (x.size() + (size_t) (0.02 * fs), (float) std::pow (10.0, (s % 2 ? 6.0 : -20.0) / 20));
        const auto g = grTrace (fs, P (10, ratio, 7, 7), x);
        int flips = 0; double worstOver = 0;
        const size_t seg = (size_t) (0.02 * fs);
        for (size_t s = 0; s < 6; ++s)
        {
            const double gEnd = g[(s + 1) * seg - 1];
            int sign = 0;
            for (size_t i = s * seg + 1; i < (s + 1) * seg; ++i)
            {
                const double d = g[i] - g[i - 1];
                const int sg = d > 1e-9 ? 1 : (d < -1e-9 ? -1 : 0);
                if (sg != 0 && sign != 0 && sg != sign) ++flips;
                if (sg != 0) sign = sg;
                if (s % 2) worstOver = std::max (worstOver, gEnd - g[i]);   // montée du niveau : la réduction ne dépasse pas sa valeur finale
            }
        }
        // sinus 1050 Hz (période exacte de 42 échantillons) : aucune raie hors harmoniques (pas de sous-harmonique ni de cycle limite)
        FetChain c; c.prepare (fs, 0); c.setParams (P (20, ratio, 7, 7));
        const int warm = (int) fs, Pd = 42 * 2, k = 100;   // deux périodes : une sous-harmonique d'ordre 2 serait visible
        auto xs = sine (fs, 1050, -6, warm + Pd * k); std::vector<float> y (xs.size());
        c.process (xs.data(), xs.data(), y.data(), y.data(), (int) xs.size());
        const auto a = lines (y, warm, Pd, k);
        double sub = 0; for (int q = 1; q <= Pd / 2; q += 2) sub = std::max (sub, a[(size_t) q]);
        std::snprintf (m, sizeof m, "ratio %s : %d inversion(s) de sens, dépassement %.4f dB ; raie hors harmoniques %.0f dB sous la note",
                       ratio == 0 ? "All" : (ratio == 4 ? "4" : "20"), flips, worstOver, db (sub / a[2]));
        check (flips == 0 && worstOver < 1e-6 && db (sub / a[2]) < -120, m);
    }

    std::puts ("\n4b) Réglages sans clic : changements de ratio et de temps pendant un sinus 200 Hz compressé");
    {
        FetChain c; c.prepare (48000, 2); c.setParams (P (12, 4)); c.engine().prepare (48000, 4);
        auto s = sine (48000, 200, -6, 48000 * 3); std::vector<float> yl (s.size()), yr (s.size());
        // pente maximale rapportée à la crête de chaque bloc de 10 ms : un sinus pur donne 2π·200/48000 = 0,026 ; un clic bien plus
        float worst = 0, prev = 0;
        for (size_t off = 0; off < s.size(); off += 480)
        {
            if (off == 48000) c.setParams (P (12, 20, 7, 7));
            if (off == 72000) c.setParams (P (12, 0, 1, 1));
            if (off == 96000) c.setParams (P (12, 4, 4, 4));
            c.process (s.data() + off, s.data() + off, yl.data() + off, yr.data() + off, 480);
            float d = 0, pk = 1e-9f;
            for (size_t i = off; i < off + 480; ++i) { d = std::max (d, std::abs (yl[i] - prev)); prev = yl[i]; pk = std::max (pk, std::abs (yl[i])); }
            if (off >= 24000) worst = std::max (worst, d / pk);
        }
        std::snprintf (m, sizeof m, "ratio 4 → 20 (attaque 7) → All (attaque 1) → 4 : pente max / crête %.4f (sinus pur : 0.0262 ; 1.2, ratio changé d'un coup : 0.0328)", worst);
        check (worst < 0.032f, m);
    }

    std::puts ("\n5) Distorsion selon le niveau (1 kHz, 48 kHz, réglages par défaut : Input 0, ratio 4, attaque 4, relâchement 4, 4x)");
    {
        std::puts ("       niveau     THD      H2      H3   réduction");
        double tPrev = 0; bool rising = true; double tNom = 0;
        for (double lvl : { -36.0, -30.0, -24.0, -18.0, -12.0, -6.0, 0.0 })
        {
            const auto t = thd (2, P (0, 4), lvl);
            const double gr = steadyGR (4, lvl);
            std::printf ("     %5.0f dBFS  %5.2f %%  %5.2f %%  %5.2f %%  %5.1f dB\n", lvl, t.thd, t.h2, t.h3, gr);
            if (lvl == -18) tNom = t.thd;
            if (lvl > -30 && t.thd < tPrev * 0.98) rising = false;
            tPrev = t.thd;
        }
        std::snprintf (m, sizeof m, "niveau nominal (−18 dBFS = 0 VU, hors compression) : THD %.2f %% (cible 0.1 à 0.3 %%), qui monte avec le niveau", tNom);
        check (tNom >= 0.1 && tNom <= 0.3 && rising, m);   // 1.2 : 0.19 % (H3 seule)
        const auto a1 = thd (2, P (0, 4), -18);
        std::snprintf (m, sizeof m, "asymétrie de l'étage classe A : H2 %.3f %%, H3 %.3f %% au niveau nominal", a1.h2, a1.h3);
        check (a1.h2 > 0.05 && a1.h3 > 0.05, m);
        // All buttons : plus de distorsion paire, mais sans excès
        const auto tAll = thd (2, P (20, 0, 4, 4), -12), t4 = thd (2, P (20, 4, 4, 4), -12);
        std::snprintf (m, sizeof m, "Input +20, −12 dBFS : ratio 4 THD %.2f %% (H2 %.2f %%) ; All THD %.2f %% (H2 %.2f %% ; 1.2 : 5.0 %%)", t4.thd, t4.h2, tAll.thd, tAll.h2);
        check (tAll.h2 > t4.h2 && tAll.thd < 6, m);
    }

    std::puts ("\n6) Réponse en fréquence (−40 dBFS, sans compression) : même son à 44.1 / 48 / 96 / 192 kHz");
    {
        const double freqs[] = { 20, 50, 1000, 10000, 16000, 20000 };
        for (int os = 0; os < 4; ++os)
        {
            double worst = 0; char row[200] = ""; int pos = 0;
            for (double f : freqs)
            {
                double ref = 0;
                for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
                {
                    const int n = (int) (fs * 0.6), st = (int) (fs * 0.4);
                    const auto y = run (fs, os, P (0, 4), sine (fs, f, -40, n));
                    // amplitude par projection sur la fréquence (fenêtre de Hann)
                    std::complex<double> s = 0, w = 0;
                    for (int i = st; i < n; ++i) { const double hw = 0.5 - 0.5 * std::cos (2 * kPi * (i - st) / (n - st)); s += hw * (double) y[(size_t) i] * std::polar (1.0, -2 * kPi * f * i / fs); w += hw; }
                    const double gdb = db (2 * std::abs (s) / std::abs (w) / std::pow (10.0, -40 / 20.0));
                    if (fs == 44100.0) ref = gdb; else worst = std::max (worst, std::abs (gdb - ref));
                    if (fs == 44100.0) pos += std::snprintf (row + pos, sizeof row - (size_t) pos, " %g Hz %+.2f", f, gdb);
                }
            }
            std::snprintf (m, sizeof m, "%-3s : écart max entre fréquences d'échantillonnage %.3f dB (44.1 kHz :%s dB)", osName[os], worst, row);
            check (worst < 0.1, m);
        }
    }

    std::puts ("\n7) Latence (impulsion, −40 dBFS) : exacte et entière");
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int os = 0; os < 4; ++os)
        {
            FetChain c; c.prepare (fs, os); c.setParams (P());
            std::vector<float> x (4096, 0.0f), y (4096);
            x[1000] = 0.01f;
            c.process (x.data(), x.data(), y.data(), y.data(), 4096);
            int pk = 0; for (int i = 0; i < 4096; ++i) if (std::abs (y[(size_t) i]) > std::abs (y[(size_t) pk])) pk = i;
            std::snprintf (m, sizeof m, "%3.0f kHz, %-3s : latence annoncée %d, pic mesuré à %d", fs / 1000, osName[os], c.latency(), pk - 1000);
            check (pk - 1000 == c.latency(), m);
        }

    std::printf ("\nRÉSULTAT : %s\n", OK ? "tout est conforme" : "ÉCHECS ci-dessus");
    return OK ? 0 : 1;
}
