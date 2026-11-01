#pragma once
#include "PluginProcessor.h"
#include "Widgets.h"
#include "VuMeter.h"

class AudioLabFETEditor : public juce::AudioProcessorEditor,
                          private juce::DarkModeSettingListener
{
public:
    explicit AudioLabFETEditor (AudioLabFETProcessor&);
    ~AudioLabFETEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    // Grille de 24 px (docs/06_GUI_Plugin_Compresseur.md) :
    // Input | Output | Attack / Release | Ratio | VU | Meter (petit Mix sous Meter, en bas à droite)
    static constexpr int baseW = 879, baseH = 272;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (AudioLabFETEditor& e) : ed (e) {}
        void paint (juce::Graphics& g) override;
        void resized() override;
        AudioLabFETEditor& ed;
    };

    void applyTheme();
    void refreshState();
    void darkModeSettingChanged() override { applyTheme(); }

    AudioLabFETProcessor& proc;
    audiolab::Theme theme;
    audiolab::ALLookAndFeel lnf { theme };
    juce::TooltipWindow tooltips { nullptr, 600 };
    Content content { *this };

    std::unique_ptr<audiolab::ArcKnob> kIn, kOut, kAtk, kRel, kMix;
    audiolab::VuFace vu;
    audiolab::SegButtons ratioSeg { theme, { "20", "12", "8", "4", "All" } };
    audiolab::SegButtons meterSeg { theme, { "GR", "+8", "+4", "OFF" } };
    std::unique_ptr<juce::ParameterAttachment> bypassAtt;
    juce::Array<juce::Component*> body;
    bool bypassed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLabFETEditor)
};
