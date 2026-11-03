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

// Bascule du suréchantillonnage (sortie à zéro pendant le fondu) : moteur remis à zéro, latence mise à jour
void AudioLabFETProcessor::applyOversampling (int choice)
{
    chain.setOversampling (choice);
    chain.setParams (currentParams());
    const int own = chain.latency();
    const int lat = constantLatency ? chain.maxLatency() : own;
    wetPad = lat - own;
    std::fill (padL.begin(), padL.end(), 0.0f);
    std::fill (padR.begin(), padR.end(), 0.0f);
    if (lat != latency) { latency = lat; setLatencySamples (lat); }
}

void AudioLabFETProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Pro Tools n'aime pas les changements de latence : en AAX, latence fixe (celle du 8×), complétée par un retard
    if (wrapperType == wrapperType_AAX) constantLatency = true;
    chain.prepare (sampleRate, osChoice());
    for (auto* v : { &dryL, &dryR, &padL, &padR }) v->assign ((size_t) ringLen, 0.0f);
    ringW = 0;
    latency = -1;
    applyOversampling (osChoice());
    swGain = 1; swDir = 0; swStep = (float) (1.0 / (0.010 * sampleRate));
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

    chain.setParams (currentParams());
    mixSm.setTargetValue (getParam (params::mix) / 100.0f);
    bypassSm.setTargetValue (getParam (params::bypass) > 0.5f ? 1.0f : 0.0f);
    // nouveau choix de suréchantillonnage : fondu de sortie, bascule, fondu d'entrée (aucun clic)
    if (osChoice() != chain.oversampling() && swDir == 0) swDir = -1;

    float* L = buffer.getWritePointer (0);
    float* R = nIn > 1 ? buffer.getWritePointer (1) : nullptr;
    const int chunkMax = std::min ((int) wetL.size(), 128);   // petits morceaux : la bascule tombe juste après la fin du fondu
    const int rm = ringLen - 1;
    for (int off = 0; off < total; off += chunkMax)
    {
        const int n = std::min (chunkMax, total - off);
        if (swDir < 0 && swGain <= 0.0f) { applyOversampling (osChoice()); swDir = 1; }
        const float* inR = R != nullptr ? R + off : L + off;   // mono : R = L (comme inp[1]||inp[0])
        chain.process (L + off, inR, wetL.data(), wetR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const float xl0 = L[off + i], xr0 = inR[i];
            // sec retardé de la latence (Dry/Wet sans filtre en peigne, bypass aligné)
            dryL[(size_t) ringW] = xl0; dryR[(size_t) ringW] = xr0;
            const int rd = (ringW - latency) & rm;
            const float xl = dryL[(size_t) rd], xr = dryR[(size_t) rd];
            // traité retardé du complément (AAX : latence fixe)
            padL[(size_t) ringW] = wetL[(size_t) i]; padR[(size_t) ringW] = wetR[(size_t) i];
            const int rp = (ringW - wetPad) & rm;
            const float wl = padL[(size_t) rp], wr = padR[(size_t) rp];
            ringW = (ringW + 1) & rm;
            const float m = mixSm.getNextValue(), b = bypassSm.getNextValue();
            // Dry/Wet à puissance constante (compression parallèle), puis bypass en fondu
            const float gd = std::cos (m * juce::MathConstants<float>::halfPi), gw = std::sin (m * juce::MathConstants<float>::halfPi);
            const float pl = xl * gd + wl * gw, pr = xr * gd + wr * gw;
            float ol = pl * (1 - b) + xl * b, orr = pr * (1 - b) + xr * b;
            if (swDir != 0)
            {
                swGain = juce::jlimit (0.0f, 1.0f, swGain + (float) swDir * swStep);
                if (swDir > 0 && swGain >= 1.0f) swDir = 0;
                ol *= swGain; orr *= swGain;
            }
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
    const float gr = chain.engine().takeGRMin();
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
