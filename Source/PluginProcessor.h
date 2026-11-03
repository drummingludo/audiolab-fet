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

    // Latence annoncée pour un choix de suréchantillonnage (affichée dans les réglages avancés)
    int latencyForChoice (int choice) const { return constantLatency ? chain.maxLatency() : chain.latencyFor (choice); }
    double sampleRateForDisplay() const { return getSampleRate() > 0 ? getSampleRate() : 48000.0; }

private:
    audiolab::FetParams currentParams() const;
    int osChoice() const { return juce::jlimit (0, 3, juce::roundToInt (getParam (audiolab::params::oversampling))); }
    void applyOversampling (int choice);

    audiolab::FetChain chain;
    juce::AudioProcessorParameter* bypassParam = nullptr;
    std::vector<float> wetL, wetR;
    // signal sec recalé sur la latence (Dry/Wet et bypass) ; retard de complément du traité (latence fixe en AAX)
    static constexpr int ringLen = 256;
    std::vector<float> dryL, dryR, padL, padR;
    int ringW = 0, latency = 0, wetPad = 0;
    bool constantLatency = false;
    // changement de suréchantillonnage : fondu de sortie (10 ms) → bascule → fondu d'entrée
    float swGain = 1, swStep = 0; int swDir = 0;
    juce::SmoothedValue<float> mixSm, bypassSm;
    double msOut = 0, msCoef = 0;                   // niveau de sortie pour le VU (moyenne quadratique, 10 ms)
    std::atomic<float> outDb { -100 };
    std::atomic<juce::uint32> lastBlockMs { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLabFETProcessor)
};
