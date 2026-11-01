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
    auto acc = [] (const Pal& q) { return q.accent; };
    auto ora = [] (const Pal& q) { return q.orange; };
    auto db1 = [] (float v) { return (v > 0 ? "+" : "") + String (v, 1); };

    kIn  = std::make_unique<ArcKnob> (theme, prm (params::input), "Input", "dB", acc, ArcKnob::Size::md, db1, -20.0f, U (helpIn));
    kOut = std::make_unique<ArcKnob> (theme, prm (params::output), "Output", "dB", acc, ArcKnob::Size::md, db1, -30.0f, U (helpOut));
    kAtk = std::make_unique<ArcKnob> (theme, prm (params::attack), "Attack", "ms", ora, ArcKnob::Size::sm,
                                      [] (float v) { return params::atkValue (v); }, 1.0f, U (helpAtk));
    kAtk->unitFn = [] (float v) { return params::atkUnit (v); };
    kRel = std::make_unique<ArcKnob> (theme, prm (params::release), "Release", "ms", ora, ArcKnob::Size::sm,
                                      [] (float v) { return params::relValue (v); }, 1.0f, U (helpRel));
    kMix = std::make_unique<ArcKnob> (theme, prm (params::mix), "Mix", "%", acc, ArcKnob::Size::xs,
        [] (float v) { return String (juce::roundToInt (v)); }, 0.0f, U (helpMix));

    ratioSeg.vertical = true; ratioSeg.gap = 2; ratioSeg.fontSize = 13;
    ratioSeg.attach (prm (params::ratio));
    ratioSeg.setTooltip (U (helpRat));
    meterSeg.vertical = true; meterSeg.gap = 2; meterSeg.fontSize = 13;
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

    body = { kIn.get(), kOut.get(), kAtk.get(), kRel.get(), kMix.get(), &vu, &ratioSeg, &meterSeg };
    for (auto* c : body) content.addAndMakeVisible (c);
    juce::Desktop::getInstance().addDarkModeSettingListener (this);

    applyTheme();
    const double sc = juce::jlimit (0.6, 2.0, (double) proc.ui.getProperty ("scale", 1.0));   // lu avant les limites
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setResizeLimits (baseW * 6 / 10, baseH * 6 / 10, baseW * 2, baseH * 2);
    setSize (juce::roundToInt (baseW * sc), juce::roundToInt (baseH * sc));
    refreshState();
}

AudioLabFETEditor::~AudioLabFETEditor()
{
    juce::Desktop::getInstance().removeDarkModeSettingListener (this);
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AudioLabFETEditor::applyTheme()
{
    const String t = proc.ui.getProperty ("themeOverride", "auto").toString();
    const bool dark = t == "dark" || (t != "light" && juce::Desktop::getInstance().isDarkModeActive());
    theme.setDark (dark);
    lnf.refresh();
    sendLookAndFeelChange();
    content.repaint();
    for (auto* c : content.getChildren()) c->repaint();
}

void AudioLabFETEditor::refreshState()
{
    for (auto* c : body) { c->setAlpha (bypassed ? 0.35f : 1.0f); c->setEnabled (! bypassed); }
    meterSeg.setEnabled (true); meterSeg.setAlpha (1.0f);
    content.repaint();
}

void AudioLabFETEditor::resized()
{
    const float s = (float) getWidth() / baseW;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, baseW, baseH);
    proc.ui.setProperty ("scale", (double) s, nullptr);
}

// Colonnes (centres) : Input 75,5 · Output 202,5 · Attack/Release 313,5 · Ratio 373–425 · VU 449–779 · Meter 803–855
static constexpr float arX = 313.5f, atkY = 87.0f, relY = 169.0f;

void AudioLabFETEditor::Content::paint (juce::Graphics& g)
{
    const auto& p = ed.theme.pal();
    g.fillAll (p.bg2);
    const float a = ed.bypassed ? 0.35f : 1.0f;
    g.setColour (p.text.withMultipliedAlpha (a)); g.setFont (ed.theme.ui (11, 600, 0.06f));
    auto label = [&] (const char* t, float cx, float y) { g.drawText (t, juce::Rectangle<float> (cx - 50, y, 100, 14), juce::Justification::centred, false); };
    label ("ATTACK", arX, atkY - 35 - 20);
    label ("RELEASE", arX, relY + 35 + 6);
    label ("RATIO", 399, 24);
    g.setColour (p.text); label ("METER", 829, 24);
    g.setColour (p.muted.withMultipliedAlpha (a)); g.setFont (ed.theme.ui (11));
    g.drawText (U ("7 = le plus rapide"), juce::Rectangle<float> (arX - 60, relY + 35 + 22, 120, 14), juce::Justification::centred, false);
}

void AudioLabFETEditor::Content::resized()
{
    auto& e = ed;
    // Rangée principale (y 24 → 248)
    e.kIn->setBounds (4, 70, 143, 130);
    e.kOut->setBounds (131, 70, 143, 130);
    e.kAtk->setBounds (juce::roundToInt (arX - 38), juce::roundToInt (atkY - 38), 76, 76);
    e.kRel->setBounds (juce::roundToInt (arX - 38), juce::roundToInt (relY - 38), 76, 76);
    e.ratioSeg.setBounds (373, 46, 52, 202);
    e.vu.setBounds (449, 24, 330, 224);
    const float bh = (202.0f - 6 - 4 * 2) / 5;                     // même hauteur de bouton que Ratio
    e.meterSeg.setBounds (803, 46, 52, juce::roundToInt (6 + 4 * bh + 3 * 2));
    // Petit Mix en bas à droite, sous Meter, aligné sur le bas du VU
    const int kw = juce::roundToInt (e.kMix->idealWidth());
    e.kMix->setBounds (855 - kw, 220, kw, 28);
}
