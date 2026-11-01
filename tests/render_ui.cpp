// Test hors DAW : fait tourner le plugin sur un signal musical et enregistre des captures
// de l'interface (thèmes, modes du VU, bypass, tailles) en PNG.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <thread>

static void fillMusic (juce::AudioBuffer<float>& b, double fs, int64_t& pos, juce::Random& rnd)
{
    for (int i = 0; i < b.getNumSamples(); ++i, ++pos)
    {
        const double t = pos / fs, beat = 0.5, tb = std::fmod (t, beat);
        const int k = (int) (t / beat);
        double s = std::sin (2 * juce::MathConstants<double>::pi * (50 + 120 * std::exp (-tb * 30)) * tb) * std::exp (-tb * 9) * 0.9;
        if (k % 2) s += (rnd.nextFloat() * 2 - 1) * std::exp (-tb * 18) * 0.5;
        s += 0.25 * std::sin (2 * juce::MathConstants<double>::pi * 55 * t);
        s += 0.2 * std::sin (2 * juce::MathConstants<double>::pi * 220 * t + 3 * std::sin (2 * juce::MathConstants<double>::pi * 5 * t));
        b.setSample (0, i, (float) s); b.setSample (1, i, (float) (s * 0.95));
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? argv[1] : "/tmp");
    auto proc = std::make_unique<AudioLabFETProcessor>();
    proc->setPlayConfigDetails (2, 2, 48000, 512);
    proc->prepareToPlay (48000, 512);

    std::atomic<bool> run { true };
    std::thread audio ([&] {
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi; int64_t pos = 0; juce::Random rnd (7);
        while (run) { fillMusic (buf, 48000, pos, rnd); proc->processBlock (buf, midi); std::this_thread::sleep_for (std::chrono::microseconds (10600)); }
    });

    std::unique_ptr<juce::AudioProcessorEditor> ed;
    std::vector<std::pair<int, std::function<void()>>> steps;
    auto shot = [&] (const juce::String& name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = out.getChildFile (name + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
        std::printf ("%s %dx%d\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    auto rebuild = [&]
    {
        if (ed != nullptr) { proc->editorBeingDeleted (ed.get()); ed.reset(); }
        ed.reset (proc->createEditorAndMakeActive()); ed->setVisible (true);
    };
    auto set = [&] (const char* id, float v) { auto* p = proc->apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    auto meter = [&] (int m) { proc->ui.setProperty ("meter", m, nullptr); };

    using namespace audiolab;
    steps.push_back ({ 0,    [&] { meter (0); set (params::input, 12); set (params::output, -6); rebuild(); } });
    steps.push_back ({ 2000, [&] { shot ("01_GR"); meter (2); set (params::attack, 1); set (params::release, 7); rebuild(); } });
    steps.push_back ({ 1500, [&] { shot ("02_sortie_plus8_atk1_rel7"); meter (0); set (params::ratio, 4); set (params::attack, 7); set (params::input, 24); set (params::mix, 50); rebuild(); } });
    steps.push_back ({ 2000, [&] { shot ("03_allbuttons_mix50"); meter (3); rebuild(); } });
    steps.push_back ({ 1500, [&] { shot ("04_meter_off"); set (params::bypass, 1); rebuild(); } });
    steps.push_back ({ 800,  [&] { shot ("05_bypass"); set (params::bypass, 0); meter (1); rebuild();
                                   ed->setSize (AudioLabFETEditor::baseW * 6 / 10, AudioLabFETEditor::baseH * 6 / 10); } });
    steps.push_back ({ 1000, [&] { shot ("06_60pc_plus4"); ed->setSize (AudioLabFETEditor::baseW * 2, AudioLabFETEditor::baseH * 2); } });
    steps.push_back ({ 1000, [&] { shot ("07_200pc"); juce::MessageManager::getInstance()->stopDispatchLoop(); } });

    std::function<void (size_t)> next = [&] (size_t i)
    {
        if (i >= steps.size()) return;
        juce::Timer::callAfterDelay (std::max (1, steps[i].first), [&, i] { steps[i].second(); next (i + 1); });
    };
    next (0);
    juce::MessageManager::getInstance()->runDispatchLoop();

    run = false; audio.join();
    proc->editorBeingDeleted (ed.get()); ed.reset();
    proc->releaseResources();
    return 0;
}
