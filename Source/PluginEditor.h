#pragma once
#include "PluginProcessor.h"
#include "Widgets.h"
#include "VuMeter.h"

class AudioLabFETEditor : public juce::AudioProcessorEditor,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::AsyncUpdater
{
public:
    explicit AudioLabFETEditor (AudioLabFETProcessor&);
    ~AudioLabFETEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    // Charte V2 (fiche 02) et docs/09, grille de 24 px :
    // Input | Output | Attack / Release | Ratio | VU | Meter (petit Mix sous Meter, en bas à droite)
    // 2.0.0 : panneau « Advanced settings » repliable (comme Lab Comp) : 819 × 248 replié, 819 × 322 déplié
    static constexpr int baseW = 819, baseH = 248, baseHOpen = 322;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (AudioLabFETEditor& e) : ed (e) {}
        void paint (juce::Graphics& g) override;
        void resized() override;
        AudioLabFETEditor& ed;
    };

    void refreshState();
    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void setAdvanced (bool open, bool resizeWindow);
    int currentH() const { return advOpen ? baseHOpen : baseH; }
    juce::String latencyText() const;

    AudioLabFETProcessor& proc;
    audiolab::Theme theme;
    audiolab::ALLookAndFeel lnf { theme };
    juce::TooltipWindow tooltips { nullptr, 600 };
    Content content { *this };

    std::unique_ptr<audiolab::ArcKnob> kIn, kOut, kAtk, kRel, kMix;
    audiolab::VuFace vu;
    audiolab::SegButtons ratioSeg { theme, { "20", "12", "8", "4", "All" } };
    audiolab::SegButtons meterSeg { theme, { "GR", "+8", "+4", "OFF" } };
    audiolab::SegButtons osSeg { theme, { "Off", juce::String::fromUTF8 ("2×"), juce::String::fromUTF8 ("4×"), juce::String::fromUTF8 ("8×") } };
    audiolab::Disclosure advBtn { theme, "Advanced settings" };
    std::unique_ptr<juce::ParameterAttachment> bypassAtt;
    juce::Array<juce::Component*> advItems;
    bool bypassed = false, advOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLabFETEditor)
};
