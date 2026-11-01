#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FetCore.h"
#include "Params.h"

class AudioLabFETProcessor : public juce::AudioProcessor
{
public:
    AudioLabFETProcessor();
    ~AudioLabFETProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree ui;   // thème (tests), mode du VU-mètre, taille de la fenêtre
    float getParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    // Mesures pour le VU-mètre (thread audio → interface)
    struct Meters { float grDb = 0, outDb = -100; bool active = false; };
    Meters readMeters();

private:
    audiolab::FetParams currentParams() const;

    audiolab::FetCore core;
    juce::AudioProcessorParameter* bypassParam = nullptr;
    std::vector<float> wetL, wetR;
    juce::SmoothedValue<float> mixSm, bypassSm;
    double msOut = 0, msCoef = 0;                   // niveau de sortie pour le VU (moyenne quadratique, 10 ms)
    std::atomic<float> outDb { -100 };
    std::atomic<juce::uint32> lastBlockMs { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLabFETProcessor)
};
