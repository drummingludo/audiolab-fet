#include "PluginEditor.h"

using namespace audiolab;
using juce::String;

static String U (const char* s) { return String::fromUTF8 (s); }

static const char* helpIn   = "Input: the threshold is fixed, so pushing the input harder gives more compression.";
static const char* helpOut  = "Output: level make-up after compression. Compare at equal loudness (bypass).";
static const char* helpAtk  = "Attack from 800 µs (1) to 20 µs (7). Very fast: the FET catches the transients.";
static const char* helpRel  = "Release from 1.1 s (1) to 50 ms (7), a little slower when the reduction is heavy.";
static const char* helpRat  = "Ratio 4, 8, 12 or 20:1. \"All\": all 4 buttons pressed in: 12 to 20:1, slower attack (transients bite), "
                              "faster release, more distortion.";
static const char* helpMtr  = "What the needle shows: GR = gain reduction; +8 / +4 = output level "
                              "(0 VU = −14 / −18 dBFS); OFF = needle at rest.";
static const char* helpMix  = "Dry/Wet: blends the processed sound with the direct sound (parallel compression).";
static const char* helpOs   = "Oversampling: the compressor runs at 2, 4 or 8 times the session rate, so fast gain changes and "
                              "saturation don't fold back as aliasing. 4× is the default; Off = no latency, lowest CPU.";

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

    // Réglages avancés (2.0.0) : suréchantillonnage, latence affichée à côté
    osSeg.onSurface = true;
    osSeg.attach (prm (params::oversampling));
    osSeg.setTooltip (U (helpOs));
    advBtn.setTooltip (U ("Show / hide Oversampling"));
    advBtn.onClick = [this] { setAdvanced (! advOpen, true); };
    advItems = { &osSeg };

    for (auto* c : std::initializer_list<juce::Component*> { kIn.get(), kOut.get(), kAtk.get(), kRel.get(), kMix.get(), &vu, &ratioSeg, &meterSeg, &advBtn })
        content.addAndMakeVisible (c);
    for (auto* c : advItems) content.addAndMakeVisible (c);
    proc.apvts.addParameterListener (params::oversampling, this);
    const double sc = juce::jlimit (0.6, 2.0, (double) proc.ui.getProperty ("scale", 1.0));   // lu avant les limites
    setAdvanced ((bool) proc.ui.getProperty ("adv", false), false);   // panneau avancé caché par défaut
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) baseW / currentH());
    setResizeLimits (baseW * 6 / 10, currentH() * 6 / 10, baseW * 2, currentH() * 2);
    setSize (juce::roundToInt (baseW * sc), juce::roundToInt (currentH() * sc));
    refreshState();
}

AudioLabFETEditor::~AudioLabFETEditor()
{
    proc.apvts.removeParameterListener (params::oversampling, this);
    cancelPendingUpdate();
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AudioLabFETEditor::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }
void AudioLabFETEditor::handleAsyncUpdate() { content.repaint(); }

// Ouvre / ferme le panneau « Advanced settings » : la fenêtre grandit ou rétrécit de 74 px (à l'échelle)
void AudioLabFETEditor::setAdvanced (bool open, bool resizeWindow)
{
    advOpen = open;
    proc.ui.setProperty ("adv", open, nullptr);
    advBtn.open = open; advBtn.repaint();
    for (auto* c : advItems) c->setVisible (open);
    if (resizeWindow)
    {
        const double s = (double) getWidth() / baseW;   // échelle actuelle, lue avant de changer les limites
        getConstrainer()->setFixedAspectRatio ((double) baseW / currentH());
        setResizeLimits (baseW * 6 / 10, currentH() * 6 / 10, baseW * 2, currentH() * 2);
        setSize (juce::roundToInt (baseW * s), juce::roundToInt (currentH() * s));
    }
    resized();
    content.repaint();
}

// « Latency 77 smp (1.6 ms) » pour le choix affiché ; « Off » : aucune latence
juce::String AudioLabFETEditor::latencyText() const
{
    const int choice = osSeg.getSelected();
    const int n = proc.latencyForChoice (choice);
    if (n == 0) return U ("No latency");
    const double ms = 1000.0 * n / proc.sampleRateForDisplay();
    return "Latency " + String (n) + " smp (" + String (ms, ms < 10 ? 2 : 1) + " ms)";
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
    content.setBounds (0, 0, baseW, currentH());
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
    g.drawText (U ("7 = fastest"), juce::Rectangle<float> (arX - 60, relY + 35 + 22, 120, 14), juce::Justification::centred, false);
    if (! ed.advOpen) return;
    // panneau des réglages avancés (« Surface », coins 12, marge intérieure 16) : une ligne de 26 px
    g.setColour (p.surface); g.fillRoundedRectangle (24, 240, 771, 58, 12);
    g.setColour (p.text2); g.setFont (ed.theme.ui (12));
    g.drawText ("Oversampling", juce::Rectangle<float> (40, 256, 90, 26), juce::Justification::centredLeft, false);
    g.setColour (p.soft);
    g.drawText (ed.latencyText(), juce::Rectangle<float> ((float) ed.osSeg.getRight() + 16, 256, 260, 26), juce::Justification::centredLeft, false);
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
    // flèche « Advanced settings » en bas à gauche, sur la même ligne que le petit Mix
    e.advBtn.setBounds (16, 196, juce::roundToInt (e.advBtn.idealWidth()), 28);
    // panneau avancé (y 240 → 298) : libellé 90 px puis sélecteur
    e.osSeg.setBounds (40 + 98, 256, juce::roundToInt (e.osSeg.idealWidth()), 26);
}
