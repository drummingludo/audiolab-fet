// Paramètres du plugin « Audio Lab FET » (compresseur FET d'Audio Lab, type 1176)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FetCore.h"

namespace audiolab::params
{
inline constexpr const char* input   = "input";
inline constexpr const char* output  = "output";
inline constexpr const char* attack  = "attack";
inline constexpr const char* release = "release";
inline constexpr const char* ratio   = "ratio";
inline constexpr const char* mix     = "mix";
inline constexpr const char* bypass  = "bypass";
// 2.0.0 (réglages avancés)
inline constexpr const char* oversampling = "oversampling";
inline constexpr int oversamplingDefault = 2;   // 4× : repliement < −80 dB à tous les ratios et toutes les attaques

// Boutons de ratio, de haut en bas comme sur Audio Lab ; « All » = les 4 boutons enfoncés (0 pour le moteur)
inline constexpr int ratioValues[5] = { 20, 12, 8, 4, 0 };

// Temps affichés comme dans Audio Lab (fetKnobs)
inline juce::String atkValue (float v)
{
    const double us = FetCore::attackSeconds (v) * 1e6;
    if (us < 100) return juce::String (juce::roundToInt (us));
    auto s = juce::String (us / 1000, 2);
    return s.endsWithChar ('0') ? s.dropLastCharacters (1) : s;
}
inline juce::String atkUnit (float v) { return FetCore::attackSeconds (v) * 1e6 < 100 ? juce::String::fromUTF8 ("µs") : juce::String ("ms"); }
inline juce::String relValue (float v) { return juce::String (juce::roundToInt (FetCore::releaseSeconds (v) * 1000)); }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto fl = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit,
                   std::function<String (float, int)> txt)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
            AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (std::move (txt))));
    };
    // Mêmes plages que les potentiomètres d'Audio Lab (fetKnobs) ; Attack / Release : positions 1 à 7 (7 = le plus rapide)
    fl (input, "Input", NormalisableRange<float> (-20, 30, 0.1f), 0, "dB",
        [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; });
    fl (output, "Output", NormalisableRange<float> (-30, 20, 0.1f), 0, "dB",
        [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; });
    fl (attack, "Attack", NormalisableRange<float> (1, 7, 0.01f), 4, "",
        [] (float v, int) { return atkValue (v) + " " + atkUnit (v); });
    fl (release, "Release", NormalisableRange<float> (1, 7, 0.01f), 4, "",
        [] (float v, int) { return relValue (v) + " ms"; });
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ratio, 1 }, "Ratio", StringArray { "20", "12", "8", "4", "All" }, 3));
    fl (mix, "Dry/Wet", NormalisableRange<float> (0, 100, 0.1f), 100, "%",
        [] (float v, int) { return String (roundToInt (v)) + " %"; });
    l.add (std::make_unique<AudioParameterBool> (ParameterID { bypass, 1 }, "Bypass", false));
    // 2.0.0 : suréchantillonnage du moteur (Off = aucune latence)
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { oversampling, 2 }, "Oversampling",
        StringArray { "Off", String::fromUTF8 ("2×"), String::fromUTF8 ("4×"), String::fromUTF8 ("8×") }, oversamplingDefault));
    return l;
}
} // namespace audiolab::params
