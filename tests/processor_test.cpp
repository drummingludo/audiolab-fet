// Tests du plugin hors DAW : sauvegarde / rappel, anciennes sessions, bypass et Dry/Wet recalés sur la latence,
// mono, latence par mode de suréchantillonnage, bascule sans clic, plugin = moteur testé.
#include "PluginProcessor.h"
#include <cstdio>

static bool OK = true;
static void check (bool c, const char* msg) { std::printf ("  %s %s\n", c ? "OK   " : "ÉCHEC", msg); OK &= c; }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    using namespace audiolab;
    auto set = [] (AudioLabFETProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    auto sine = [] (juce::AudioBuffer<float>& b, int64_t& pos, double amp, double f = 1000)
    {
        for (int i = 0; i < b.getNumSamples(); ++i, ++pos)
            for (int c = 0; c < b.getNumChannels(); ++c)
                b.setSample (c, i, (float) (amp * std::sin (2 * juce::MathConstants<double>::pi * f * pos / 48000.0)));
    };
    char msg[200];

    std::puts ("\nPlugin Lab FET 2.0");
    {
        AudioLabFETProcessor a; set (a, params::input, 7.3f); set (a, params::output, -4.5f); set (a, params::attack, 2.5f); set (a, params::release, 6.2f);
        set (a, params::ratio, 4); set (a, params::mix, 42); set (a, params::oversampling, 3);
        a.ui.setProperty ("meter", 2, nullptr); a.ui.setProperty ("adv", true, nullptr);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        AudioLabFETProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (b.getParam (params::input) - 7.3f) < 1e-4 && std::abs (b.getParam (params::output) + 4.5f) < 1e-4
               && std::abs (b.getParam (params::attack) - 2.5f) < 1e-4 && std::abs (b.getParam (params::release) - 6.2f) < 1e-4
               && juce::roundToInt (b.getParam (params::ratio)) == 4 && std::abs (b.getParam (params::mix) - 42) < 1e-4
               && juce::roundToInt (b.getParam (params::oversampling)) == 3
               && (int) b.ui.getProperty ("meter") == 2 && (bool) b.ui.getProperty ("adv"),
               "sauvegarde / rappel de la session (réglages, suréchantillonnage, affichage du VU, panneau avancé)");
    }
    {
        // session de la version 1.2 : pas de paramètre « oversampling » → valeur par défaut (4×), les autres réglages relus
        AudioLabFETProcessor a; set (a, params::input, 9.0f); set (a, params::ratio, 1);
        auto st = a.apvts.copyState();
        for (int i = st.getNumChildren(); --i >= 0;)
            if (st.getChild (i).getProperty ("id").toString() == params::oversampling) st.removeChild (i, nullptr);
        juce::MemoryBlock mb;
        if (auto xml = st.createXml()) juce::AudioProcessor::copyXmlToBinary (*xml, mb);
        AudioLabFETProcessor b; set (b, params::oversampling, 0);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (juce::roundToInt (b.getParam (params::oversampling)) == params::oversamplingDefault
               && std::abs (b.getParam (params::input) - 9.0f) < 1e-4 && juce::roundToInt (b.getParam (params::ratio)) == 1,
               "session 1.2 (sans les nouveaux réglages) : rouverte, suréchantillonnage à sa valeur par défaut (4x)");
    }
    {
        // latence annoncée par mode : 0 / 71 / 77 / 79 échantillons
        int lat[4];
        for (int os = 0; os < 4; ++os)
        {
            AudioLabFETProcessor p; set (p, params::oversampling, (float) os);
            p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512); lat[os] = p.getLatencySamples();
        }
        std::snprintf (msg, sizeof msg, "latence annoncée : Off %d, 2x %d, 4x %d, 8x %d échantillons", lat[0], lat[1], lat[2], lat[3]);
        check (lat[0] == 0 && lat[1] == 71 && lat[2] == 77 && lat[3] == 79, msg);
    }
    for (int os : { 0, 2 })
    {
        // bypass : la sortie est exactement l'entrée retardée de la latence annoncée
        AudioLabFETProcessor p; set (p, params::oversampling, (float) os);
        p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        set (p, params::bypass, 1); set (p, params::input, 20);
        const int L = p.getLatencySamples();
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; int64_t pos = 0;
        std::vector<float> in, out;
        for (int k = 0; k < 50; ++k) { sine (b, pos, 0.5); for (int i = 0; i < 512; ++i) in.push_back (b.getSample (0, i)); p.processBlock (b, m);
            for (int i = 0; i < 512; ++i) out.push_back (b.getSample (1, i)); }
        bool same = true;
        for (size_t i = 6 * 512; i < out.size(); ++i) same &= juce::exactlyEqual (out[i], in[i - (size_t) L]);
        std::snprintf (msg, sizeof msg, "bypass (%s) : sortie = entrée retardée de %d échantillons, au bit près", os ? "4x" : "Off", L);
        check (same, msg);
    }
    {
        // Dry/Wet 0 % : son direct recalé (aucun filtre en peigne quand on mélange)
        AudioLabFETProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        set (p, params::mix, 0); set (p, params::input, 24);
        const int L = p.getLatencySamples();
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; int64_t pos = 0; float err = 0;
        std::vector<float> in, out;
        for (int k = 0; k < 40; ++k) { sine (b, pos, 0.5); for (int i = 0; i < 512; ++i) in.push_back (b.getSample (0, i)); p.processBlock (b, m);
            for (int i = 0; i < 512; ++i) out.push_back (b.getSample (0, i)); }
        for (size_t i = 6 * 512; i < out.size(); ++i) err = std::max (err, std::abs (out[i] - in[i - (size_t) L]));
        std::snprintf (msg, sizeof msg, "Dry/Wet à 0 %% (4x) : son direct retardé de %d échantillons (écart %.1e)", L, err);
        check (err < 1e-7f, msg);
    }
    {
        // Dry/Wet 50 % sans compression (Input −20) : sec et traité en phase, pas de creux en peigne (bruit blanc)
        AudioLabFETProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        set (p, params::mix, 50); set (p, params::input, -20); set (p, params::output, 20);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; juce::Random rnd (3);
        double ein = 0, eout = 0;
        for (int k = 0; k < 200; ++k)
        {
            for (int i = 0; i < 512; ++i) { const float v = 0.05f * (rnd.nextFloat() * 2 - 1); b.setSample (0, i, v); b.setSample (1, i, v); if (k > 20) ein += v * v; }
            p.processBlock (b, m);
            if (k > 20) for (int i = 0; i < 512; ++i) eout += b.getSample (0, i) * b.getSample (0, i);
        }
        // sec × cos 45° + traité × sin 45°, en phase : 0,707 + 0,707 = 1,414 → +3,01 dB ; décalés (peigne) : 0 dB en moyenne
        const double g = 10 * std::log10 (eout / ein);
        std::snprintf (msg, sizeof msg, "Dry/Wet 50 %% sur bruit, sans compression : %+.2f dB (sec et traité en phase : +3.01 dB attendus)", g);
        check (std::abs (g - 3.01) < 0.15, msg);
    }
    {
        AudioLabFETProcessor p; p.setPlayConfigDetails (1, 1, 48000, 300); p.prepareToPlay (48000, 300);
        set (p, params::input, 20);
        juce::AudioBuffer<float> b (1, 300); juce::MidiBuffer m; int64_t pos = 0; float mx = 0, gr = 0;
        for (int k = 0; k < 400; ++k) { sine (b, pos, 0.5); p.processBlock (b, m); mx = std::max (mx, b.getMagnitude (0, 0, 300));
            gr = std::min (gr, p.readMeters().grDb); }
        check (std::isfinite (mx) && mx > 0.01f && gr < -6, "mono (1 voie, blocs de 300) : compression active, sortie saine");
    }
    {
        // VU de sortie : sinus de crête −18 dBFS → −18 dBFS (convention AES17), donc 0 VU en mode +4
        AudioLabFETProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        set (p, params::mix, 0);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; int64_t pos = 0;
        for (int k = 0; k < 100; ++k) { sine (b, pos, std::pow (10.0, -18 / 20.0)); p.processBlock (b, m); }
        const float o = p.readMeters().outDb;
        std::snprintf (msg, sizeof msg, "niveau de sortie pour le VU : sinus de crête −18 dBFS lu à %.2f dBFS (0 VU en mode +4)", o);
        check (std::abs (o + 18) < 0.1f, msg);
    }
    {
        // Le plugin sonne exactement comme le moteur testé par le banc qualité (ici : All buttons, input +18, Mix 100 %, 4x)
        AudioLabFETProcessor p; p.setPlayConfigDetails (2, 2, 48000, 256); p.prepareToPlay (48000, 256);
        set (p, params::ratio, 4); set (p, params::input, 18); set (p, params::output, -6); set (p, params::attack, 6); set (p, params::release, 3);
        FetChain chain; chain.prepare (48000, params::oversamplingDefault);
        FetParams fp; fp.inDb = 18; fp.outDb = -6; fp.atk = 6; fp.rel = 3; fp.ratio = 0; chain.setParams (fp);
        juce::AudioBuffer<float> b (2, 256), ref (2, 256), out (2, 256); juce::MidiBuffer m; int64_t pos = 0, pr = 0; float err = 0;
        for (int k = 0; k < 100; ++k)
        {
            sine (b, pos, 0.3); sine (ref, pr, 0.3); p.processBlock (b, m);
            chain.process (ref.getReadPointer (0), ref.getReadPointer (1), out.getWritePointer (0), out.getWritePointer (1), 256);
            if (k > 10) for (int i = 0; i < 256; ++i) err = std::max (err, std::abs (b.getSample (0, i) - out.getSample (0, i)));
        }
        std::snprintf (msg, sizeof msg, "ratio « All » : le plugin = le moteur du banc qualité (écart max %.1e)", err);
        check (err < 1e-6f, msg);
    }
    {
        // bascule du suréchantillonnage pendant la lecture : fondu (pas de clic), nouvelle latence annoncée
        AudioLabFETProcessor p; p.setPlayConfigDetails (2, 2, 48000, 480); p.prepareToPlay (48000, 480);
        set (p, params::input, 10);
        juce::AudioBuffer<float> b (2, 480); juce::MidiBuffer m; int64_t pos = 0;
        float prev = 0, jump = 0;
        for (int k = 0; k < 200; ++k)
        {
            if (k == 100) set (p, params::oversampling, 0);
            sine (b, pos, 0.3, 200); p.processBlock (b, m);
            for (int i = 0; i < 480; ++i) { const float v = b.getSample (0, i); if (k > 50) jump = std::max (jump, std::abs (v - prev)); prev = v; }
        }
        // pente maximale d'un sinus 200 Hz d'amplitude ≈ 0,3 : 2π·200/48000·0,3 ≈ 0,008 par échantillon
        std::snprintf (msg, sizeof msg, "bascule 4x → Off en lecture : saut max %.4f par échantillon (sinus seul : 0.008), latence %d", jump, p.getLatencySamples());
        check (jump < 0.012f && p.getLatencySamples() == 0, msg);
    }
    std::printf ("\nRÉSULTAT : %s\n", OK ? "tout est conforme" : "ÉCHECS ci-dessus");
    return OK ? 0 : 1;
}
