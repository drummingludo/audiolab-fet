#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace audiolab;

AudioLabFETProcessor::AudioLabFETProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AudioLabFET", params::createLayout()),
      ui ("ui")
{
    bypassParam = apvts.getParameter (params::bypass);
    ui.setProperty ("meter", 0, nullptr);
    ui.setProperty ("scale", 1.0, nullptr);
}

bool AudioLabFETProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

FetParams AudioLabFETProcessor::currentParams() const
{
    // Comme fetParams() dans Audio Lab
    FetParams p;
    p.inDb = getParam (params::input);
    p.outDb = getParam (params::output);
    p.atk = getParam (params::attack);
    p.rel = getParam (params::release);
    p.ratio = params::ratioValues[juce::jlimit (0, 4, juce::roundToInt (getParam (params::ratio)))];
    return p;
}

void AudioLabFETProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    core.prepare (sampleRate);
    core.setParams (currentParams());
    const int block = std::max (512, samplesPerBlock);
    wetL.assign ((size_t) block, 0.0f);
    wetR.assign ((size_t) block, 0.0f);
    mixSm.reset (sampleRate, 0.02);
    mixSm.setCurrentAndTargetValue (getParam (params::mix) / 100.0f);
    bypassSm.reset (sampleRate, 0.02);
    bypassSm.setCurrentAndTargetValue (getParam (params::bypass) > 0.5f ? 1.0f : 0.0f);
    msOut = 0;
    msCoef = std::exp (-1.0 / (0.010 * sampleRate));
}

void AudioLabFETProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels();
    for (int ch = nIn; ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, total);
    if (nIn == 0 || total == 0) return;

    core.setParams (currentParams());
    mixSm.setTargetValue (getParam (params::mix) / 100.0f);
    bypassSm.setTargetValue (getParam (params::bypass) > 0.5f ? 1.0f : 0.0f);

    float* L = buffer.getWritePointer (0);
    float* R = nIn > 1 ? buffer.getWritePointer (1) : nullptr;
    const int chunkMax = (int) wetL.size();
    for (int off = 0; off < total; off += chunkMax)
    {
        const int n = std::min (chunkMax, total - off);
        const float* inR = R != nullptr ? R + off : L + off;   // mono : R = L (comme inp[1]||inp[0])
        core.process (L + off, inR, wetL.data(), wetR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const float xl = L[off + i], xr = inR[i];
            const float m = mixSm.getNextValue(), b = bypassSm.getNextValue();
            // Dry/Wet à puissance constante (compression parallèle), puis bypass en fondu
            const float gd = std::cos (m * juce::MathConstants<float>::halfPi), gw = std::sin (m * juce::MathConstants<float>::halfPi);
            const float pl = xl * gd + wetL[(size_t) i] * gw, pr = xr * gd + wetR[(size_t) i] * gw;
            const float ol = pl * (1 - b) + xl * b, orr = pr * (1 - b) + xr * b;
            L[off + i] = ol;
            if (R != nullptr) R[off + i] = orr;
            // niveau de sortie (AES17 : un sinus de crête A donne 20·log10(A) dBFS)
            const double sq = R != nullptr ? (double) ol * ol + (double) orr * orr : 2.0 * ol * ol;
            msOut = sq + (msOut - sq) * msCoef;
        }
    }
    outDb.store ((float) (10 * std::log10 (std::max (1e-12, msOut))));
    lastBlockMs.store (juce::Time::getMillisecondCounter());
}

AudioLabFETProcessor::Meters AudioLabFETProcessor::readMeters()
{
    Meters m;
    m.active = juce::Time::getMillisecondCounter() - lastBlockMs.load() < 250;
    const float gr = core.takeGRMin();
    m.grDb = getParam (params::bypass) > 0.5f ? 0.0f : gr;
    m.outDb = outDb.load();
    return m;
}

void AudioLabFETProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto old = state.getChildWithName ("ui"); old.isValid()) state.removeChild (old, nullptr);
    state.appendChild (ui.createCopy(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AudioLabFETProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.hasType (apvts.state.getType())) return;
        if (auto u = state.getChildWithName ("ui"); u.isValid())
        {
            ui.copyPropertiesAndChildrenFrom (u, nullptr);
            state.removeChild (u, nullptr);
        }
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* AudioLabFETProcessor::createEditor() { return new AudioLabFETEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AudioLabFETProcessor(); }
