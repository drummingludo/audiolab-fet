#include "PluginEditor.h"

using namespace audiolab;
using juce::String;

static String U (const char* s) { return String::fromUTF8 (s); }

static const char* helpIn   = "Input : le seuil est fixe, c'est en poussant l'entrée qu'on compresse davantage.";
static const char* helpOut  = "Output : rattrapage du niveau après compression. Comparez à niveau égal (bypass).";
static const char* helpAtk  = "Attaque de 800 µs (1) à 20 µs (7). Très rapide : le FET attrape les transitoires.";
static const char* helpRel  = "Relâchement de 1,1 s (1) à 50 ms (7), un peu plus lent quand la réduction est forte.";
static const char* helpRat  = "Ratio 4, 8, 12 ou 20:1. « All » : les 4 boutons enfoncés — ratio extrême, distorsion, pompage.";
static const char* helpMtr  = "Ce que montre l'aiguille : GR = réduction de gain ; +8 / +4 = niveau de sortie "
                              "(0 VU = −14 / −18 dBFS) ; OFF = aiguille au repos.";
static const char* helpMix  = "Dry/Wet : mélange le son traité avec le son direct (compression parallèle).";

AudioLabFETEditor::AudioLabFETEditor (AudioLabFETProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      vu (theme, [&p] { const auto m = p.readMeters(); return VuFace::Reading { m.grDb, m.outDb, m.active }; })
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    auto& ap = proc.apvts;
    auto prm = [&] (const char* id) -> juce::RangedAudioParameter& { return *ap.getParameter (id); };
    auto db1 = [] (float v) { return (v > 0 ? "+" : "") + String (v, 1); };
    // graduation de l'original (1 à 7) en gros, valeur réelle dessous (charte : « 7 · 50 ms »)
    auto posFmt = [] (float v) { const float r = std::round (v); return std::abs (v - r) < 0.05f ? String ((int) r) : String (v, 1); };

    // couleurs de la charte : Input (niveau qui attaque le seuil fixe) = teal, Output et Mix (gains) = teal secondaire,
    // Attack / Release (temps) = orange ; gains : l'arc part de 0 dB
    kIn  = std::make_unique<ArcKnob> (theme, prm (params::input), "Input", "dB", Family::teal, ArcKnob::Size::md, db1, 0.0f, U (helpIn));
    kOut = std::make_unique<ArcKnob> (theme, prm (params::output), "Output", "dB", Family::teal2, ArcKnob::Size::md, db1, 0.0f, U (helpOut));
    kAtk = std::make_unique<ArcKnob> (theme, prm (params::attack), "Attack", "", Family::orange, ArcKnob::Size::sm, posFmt, 1.0f, U (helpAtk));
    kAtk->subFn = [] (float v) { return params::atkValue (v) + " " + params::atkUnit (v); };
    kRel = std::make_unique<ArcKnob> (theme, prm (params::release), "Release", "", Family::orange, ArcKnob::Size::sm, posFmt, 1.0f, U (helpRel));
    kRel->subFn = [] (float v) { return params::relValue (v) + " ms"; };
    kMix = std::make_unique<ArcKnob> (theme, prm (params::mix), "Mix", "%", Family::teal2, ArcKnob::Size::xs,
        [] (float v) { return String (juce::roundToInt (v)); }, 0.0f, U (helpMix));

    ratioSeg.vertical = true;
    ratioSeg.attach (prm (params::ratio));
    ratioSeg.setTooltip (U (helpRat));
    meterSeg.vertical = true;
    meterSeg.setTooltip (U (helpMtr));
    // Boutons de haut en bas : GR, +8, +4, OFF ; modes du VU : 0 = GR, 1 = +4 (−18 dBFS), 2 = +8 (−14 dBFS), 3 = éteint
    vu.setModes ({ -18.0f, -14.0f }, { "OUTPUT  +4", "OUTPUT  +8" }, true);
    static constexpr int btnToMode[4] = { 0, 2, 1, 3 };
    static constexpr int modeToBtn[4] = { 0, 2, 1, 3 };
    const int mode = juce::jlimit (0, 3, (int) proc.ui.getProperty ("meter", 0));
    meterSeg.setSelected (modeToBtn[mode]); vu.setMode (mode);
    meterSeg.onSelect = [this] (int i) { vu.setMode (btnToMode[i]); proc.ui.setProperty ("meter", btnToMode[i], nullptr); };

    bypassAtt = std::make_unique<juce::ParameterAttachment> (prm (params::bypass), [this] (float v) {
        bypassed = v > 0.5f; refreshState(); }, nullptr);
    bypassAtt->sendInitialUpdate();

    for (auto* c : std::initializer_list<juce::Component*> { kIn.get(), kOut.get(), kAtk.get(), kRel.get(), kMix.get(), &vu, &ratioSeg, &meterSeg })
        content.addAndMakeVisible (c);
    const double sc = juce::jlimit (0.6, 2.0, (double) proc.ui.getProperty ("scale", 1.0));   // lu avant les limites
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setResizeLimits (baseW * 6 / 10, baseH * 6 / 10, baseW * 2, baseH * 2);
    setSize (juce::roundToInt (baseW * sc), juce::roundToInt (baseH * sc));
    refreshState();
}

AudioLabFETEditor::~AudioLabFETEditor()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AudioLabFETEditor::refreshState()
{
    // Charte V2 : en bypass, la lampe du VU s'éteint ; les réglages restent accessibles
    vu.setLampOff (bypassed);
}

void AudioLabFETEditor::resized()
{
    const float s = (float) getWidth() / baseW;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, baseW, baseH);
    proc.ui.setProperty ("scale", (double) s, nullptr);
}

// Colonnes (centres) : Input 75,5 · Output 202,5 · Attack/Release 313,5 · Ratio 373–425 · VU 449–719 · Meter 743–795
static constexpr float arX = 313.5f, atkY = 79.0f, relY = 153.0f;

void AudioLabFETEditor::Content::paint (juce::Graphics& g)
{
    const auto& p = ed.theme.pal();
    g.fillAll (p.bg);
    g.setColour (p.text); g.setFont (ed.theme.ui (11, 600, 0.06f));
    auto label = [&] (const char* t, float cx, float y) { g.drawText (t, juce::Rectangle<float> (cx - 50, y, 100, 14), juce::Justification::centred, false); };
    label ("ATTACK", arX, atkY - 35 - 20);
    label ("RELEASE", arX, relY + 35 + 6);
    label ("RATIO", 399, 24);
    label ("METER", 769, 24);
    g.setColour (p.text2); g.setFont (ed.theme.ui (11));
    g.drawText (U ("7 = le plus rapide"), juce::Rectangle<float> (arX - 60, relY + 35 + 22, 120, 14), juce::Justification::centred, false);
}

void AudioLabFETEditor::Content::resized()
{
    auto& e = ed;
    // Rangée principale (y 24 → 224) ; potentiomètres centrés sur le VU (y 124)
    e.kIn->setBounds (4, 68, 143, 130);
    e.kOut->setBounds (131, 68, 143, 130);
    e.kAtk->setBounds (juce::roundToInt (arX - 38), juce::roundToInt (atkY - 38), 76, 76);
    e.kRel->setBounds (juce::roundToInt (arX - 38), juce::roundToInt (relY - 38), 76, 76);
    e.ratioSeg.setBounds (373, 46, 52, 178);
    e.vu.setBounds (449, 24, 270, 200);
    const float bh = (178.0f - 6 - 4 * 2) / 5;                     // même hauteur de segment que Ratio
    e.meterSeg.setBounds (743, 46, 52, juce::roundToInt (6 + 4 * bh + 3 * 2));
    // Petit Mix en bas à droite, sous Meter, aligné sur le bas du VU
    const int kw = juce::roundToInt (e.kMix->idealWidth());
    e.kMix->setBounds (795 - kw, 196, kw, 28);
}
