// Tests du plugin hors DAW : sauvegarde / rappel, bypass, mono, Dry/Wet, blocs de tailles variées.
#include "PluginProcessor.h"
#include <cstdio>

static bool OK = true;
static void check (bool c, const char* msg) { std::printf ("  %s %s\n", c ? "OK   " : "ÉCHEC", msg); OK &= c; }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    using namespace audiolab;
    auto set = [] (AudioLabFETProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    auto sine = [] (juce::AudioBuffer<float>& b, int64_t& pos, double amp)
    {
        for (int i = 0; i < b.getNumSamples(); ++i, ++pos)
            for (int c = 0; c < b.getNumChannels(); ++c)
                b.setSample (c, i, (float) (amp * std::sin (2 * juce::MathConstants<double>::pi * 1000 * pos / 48000.0)));
    };

    std::puts ("\nPlugin Audio Lab FET");
    {
        AudioLabFETProcessor a; set (a, params::input, 7.3f); set (a, params::output, -4.5f); set (a, params::attack, 2.5f); set (a, params::release, 6.2f); set (a, params::ratio, 4); set (a, params::mix, 42);
        a.ui.setProperty ("meter", 2, nullptr);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        AudioLabFETProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (b.getParam (params::input) - 7.3f) < 1e-4 && std::abs (b.getParam (params::output) + 4.5f) < 1e-4
               && std::abs (b.getParam (params::attack) - 2.5f) < 1e-4 && std::abs (b.getParam (params::release) - 6.2f) < 1e-4
               && juce::roundToInt (b.getParam (params::ratio)) == 4 && std::abs (b.getParam (params::mix) - 42) < 1e-4 && (int) b.ui.getProperty ("meter") == 2,
               "sauvegarde / rappel de la session (réglages + affichage du VU)");
    }
    {
        AudioLabFETProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        set (p, params::bypass, 1);
        juce::AudioBuffer<float> b (2, 512), ref (2, 512); juce::MidiBuffer m; int64_t pos = 0, pr = 0;
        bool same = true;
        for (int k = 0; k < 50; ++k) { sine (b, pos, 0.5); sine (ref, pr, 0.5); p.processBlock (b, m);
            if (k > 5) for (int i = 0; i < 512; ++i) same &= juce::exactlyEqual (b.getSample (0, i), ref.getSample (0, i)); }
        check (same, "bypass : la sortie est exactement l'entrée");
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
        // Dry/Wet 0 % : son direct (à la précision float près)
        AudioLabFETProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        set (p, params::mix, 0); set (p, params::input, 24);
        juce::AudioBuffer<float> b (2, 512), ref (2, 512); juce::MidiBuffer m; int64_t pos = 0, pr = 0; float err = 0;
        for (int k = 0; k < 40; ++k) { sine (b, pos, 0.5); sine (ref, pr, 0.5); p.processBlock (b, m);
            if (k > 5) for (int i = 0; i < 512; ++i) err = std::max (err, std::abs (b.getSample (0, i) - ref.getSample (0, i))); }
        check (err < 1e-6f, "Dry/Wet à 0 % : son direct");
    }
    {
        // VU de sortie : sinus de crête −18 dBFS → −18 dBFS (convention AES17), donc 0 VU en mode +4
        AudioLabFETProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        set (p, params::mix, 0);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; int64_t pos = 0;
        for (int k = 0; k < 100; ++k) { sine (b, pos, std::pow (10.0, -18 / 20.0)); p.processBlock (b, m); }
        const float o = p.readMeters().outDb;
        char msg[160]; std::snprintf (msg, sizeof msg, "niveau de sortie pour le VU : sinus de crête −18 dBFS lu à %.2f dBFS (0 VU en mode +4)", o);
        check (std::abs (o + 18) < 0.1f, msg);
    }
    {
        // Le plugin sonne exactement comme le moteur testé contre Audio Lab (ici : All buttons, input +18, Mix 100 %)
        AudioLabFETProcessor p; p.setPlayConfigDetails (2, 2, 48000, 256); p.prepareToPlay (48000, 256);
        set (p, params::ratio, 4); set (p, params::input, 18); set (p, params::output, -6); set (p, params::attack, 6); set (p, params::release, 3);
        FetCore core; core.prepare (48000);
        FetParams fp; fp.inDb = 18; fp.outDb = -6; fp.atk = 6; fp.rel = 3; fp.ratio = 0; core.setParams (fp);
        juce::AudioBuffer<float> b (2, 256), ref (2, 256); juce::MidiBuffer m; int64_t pos = 0, pr = 0; float err = 0;
        for (int k = 0; k < 100; ++k)
        {
            sine (b, pos, 0.3); sine (ref, pr, 0.3); p.processBlock (b, m);
            core.process (ref.getReadPointer (0), ref.getReadPointer (1), ref.getWritePointer (0), ref.getWritePointer (1), 256);
            for (int i = 0; i < 256; ++i) err = std::max (err, std::abs (b.getSample (0, i) - ref.getSample (0, i)));
        }
        char msg[160]; std::snprintf (msg, sizeof msg, "ratio « All » : le plugin = le moteur vérifié contre Audio Lab (écart max %.1e)", err);
        check (err < 1e-6f, msg);
    }
    std::printf ("\nRÉSULTAT : %s\n", OK ? "tout est conforme" : "ÉCHECS ci-dessus");
    return OK ? 0 : 1;
}
