// VU-mètre à aiguille d'Audio Lab sur une face encastrée (ombre intérieure, façon matériel analogique).
// Mode 0 : réduction de gain. Modes suivants : niveau de sortie référencé (+4, +8, +10…). Dernier mode facultatif : éteint.
// Aiguille : réponse VU normalisée du 2e ordre (99 % de la déviation en 300 ms, dépassement ≈ 1,3 %).
#pragma once
#include "Theme.h"
#include <functional>
#include <vector>

namespace audiolab
{
class VuFace : public juce::Component, private juce::Timer
{
public:
    struct Reading { float grDb = 0; float outDb = -100; bool active = false; };

    VuFace (Theme& t, std::function<Reading()> source) : theme (t), src (std::move (source))
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (60);
    }
    ~VuFace() override { stopTimer(); }

    // refs : niveau (dBFS, convention AES17 : sinus crête = valeur efficace + 3 dB) qui donne 0 VU pour chaque mode de sortie
    void setModes (std::vector<float> refsDbfs, juce::StringArray outLabels, bool withOff)
    {
        refs = std::move (refsDbfs); labels = std::move (outLabels); hasOff = withOff;
    }
    void setMode (int m) { mode = m; repaint(); }
    int getMode() const { return mode; }

    void paint (juce::Graphics& g) override
    {
        const auto& p = theme.pal();
        const float W = (float) getWidth(), H = (float) getHeight();
        const juce::Rectangle<float> face (0.5f, 0.5f, W - 1, H - 1);
        juce::Path panel; panel.addRoundedRectangle (face, 12);
        g.setColour (p.cv2); g.fillPath (panel);
        g.saveState();
        g.reduceClipRegion (panel);
        {
            // ombre intérieure : décalée de 4 px vers le bas, flou ≈ 5,5 px, opacité 50 %
            juce::Path frame; frame.addRectangle (face.expanded (40)); frame.addRoundedRectangle (face, 12);
            frame.setUsingNonZeroWinding (false);
            juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 11, { 0, 4 }).drawForPath (g, frame);
        }
        // cadran dessiné dans un repère 330 × 200 (comme le SVG d'Audio Lab), posé sur le bas de la face :
        // l'axe de l'aiguille reste caché sous le bord, comme dans Audio Lab
        const float s = std::min (W / 330.0f, H / 200.0f), ox = (W - 330 * s) / 2, oy = H - 200 * s;
        g.addTransform (juce::AffineTransform::scale (s).translated (ox, oy));
        const float cx = 165, cy = 215, R = 150;
        auto ang = [] (float t) { return (-48 + 96 * t) * juce::MathConstants<float>::pi / 180; };
        auto P = [&] (float t, float r) { return juce::Point<float> (cx + std::sin (ang (t)) * r, cy - std::cos (ang (t)) * r); };
        { juce::Path a; a.addCentredArc (cx, cy, R, R, 0, ang (0), ang (pos (0)), true); g.setColour (p.text.withAlpha (0.8f)); g.strokePath (a, juce::PathStrokeType (2)); }
        { juce::Path a; a.addCentredArc (cx, cy, R, R, 0, ang (pos (0)), ang (1), true); g.setColour (p.red); g.strokePath (a, juce::PathStrokeType (4)); }
        g.setFont (theme.ui (11, 600));
        for (auto& q : pts)
        {
            const float vu = q[0], t = pos (vu);
            const auto p1 = P (t, R - 1), p2 = P (t, R - 10), tp = P (t, R + 13);
            g.setColour (vu > 0 ? p.red : p.text); g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.5f);
            g.setColour (vu > 0 ? p.red : p.dim);
            const juce::String lab = (int) vu == 0 ? "0" : (vu > 0 ? "+" + juce::String ((int) vu) : juce::String ((int) vu));
            g.drawText (lab, juce::Rectangle<float> (tp.x - 15, tp.y - 7, 30, 14), juce::Justification::centred, false);
        }
        for (int vu = -20; vu <= 3; ++vu)
        {
            bool major = false; for (auto& q : pts) if ((int) q[0] == vu) major = true;
            if (major) continue;
            const float t = pos ((float) vu); const auto p1 = P (t, R - 1), p2 = P (t, R - 6);
            g.setColour (p.muted); g.drawLine (p1.x, p1.y, p2.x, p2.y, 1);
        }
        g.setColour (p.text); g.setFont (theme.ui (20, 700, 0.15f));
        g.drawText ("VU", juce::Rectangle<float> (cx - 50, 121, 100, 24), juce::Justification::centred, false);
        g.setColour (p.dim); g.setFont (theme.ui (10, 600, 0.15f));
        g.drawText (modeText(), juce::Rectangle<float> (cx - 100, 146, 200, 14), juce::Justification::centred, false);
        g.setFont (theme.ui (12, 600));
        g.drawText (valueText(), juce::Rectangle<float> (176, 181, 140, 14), juce::Justification::centredRight, false);
        const float a = ang (pos ((float) needle));
        g.setColour (p.accent);
        juce::Path nd; nd.startNewSubPath (cx, cy); nd.lineTo (cx + std::sin (a) * (R + 4), cy - std::cos (a) * (R + 4));
        g.strokePath (nd, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (p.text); g.fillEllipse (cx - 14, cy - 14, 28, 28);
        g.restoreState();
        g.setColour (p.grid); g.strokePath (panel, juce::PathStrokeType (1));
    }

private:
    bool isOff() const { return hasOff && mode > (int) refs.size(); }

    juce::String modeText() const
    {
        if (isOff()) return "METER OFF";
        if (mode == 0) return "GAIN REDUCTION";
        return labels[mode - 1];
    }
    juce::String valueText() const
    {
        if (isOff()) return {};
        if (mode == 0) return juce::String (std::abs (grShown) < 0.05 ? 0.0 : -grShown, 1) + " dB GR";
        return (outShown > 0 ? "+" : "") + juce::String (outShown, 1) + " VU";
    }

    void timerCallback() override
    {
        const auto r = src();
        const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        const double dt = std::min (0.1, lastT > 0 ? now - lastT : 1.0 / 60);
        lastT = now;
        const float gr = r.active ? std::min (0.0f, r.grDb) : 0.0f;
        // mémoire courte : la réduction la plus forte depuis la dernière lecture redescend en 60 ms
        grSm += (gr - grSm) * (1 - std::exp (-dt / 0.06));
        grShown = (float) grSm;
        double target;
        if (isOff()) target = -22;
        else if (mode == 0) target = grSm;
        else
        {
            const int k = juce::jlimit (0, (int) refs.size() - 1, mode - 1);
            target = (r.active ? r.outDb : -100) - refs[(size_t) k];
            outShown += (float) (((r.active ? r.outDb : -100) - refs[(size_t) k] - outShown) * (1 - std::exp (-dt / 0.3)));
        }
        // aiguille VU normalisée (2e ordre) : 99 % en 300 ms, dépassement ≈ 1,3 %
        const double wn = 13.42, z = 0.81;
        target = std::max (-23.0, std::min (6.0, target));
        for (double t = 0; t < dt; t += 0.001)
        {
            const double h1 = std::min (0.001, dt - t);
            const double acc = wn * wn * (target - needle) - 2 * z * wn * vel;
            vel += acc * h1; needle += vel * h1;
        }
        repaint();
    }

    static float pos (float vu)
    {
        if (vu <= -20) return 0;
        if (vu >= 3) return 1;
        for (int i = 1; i < 11; ++i)
            if (vu <= pts[i][0])
            {
                const float a = pts[i - 1][0], pa = pts[i - 1][1], b = pts[i][0], pb = pts[i][1];
                return pa + (pb - pa) * (vu - a) / (b - a);
            }
        return 1;
    }
    static constexpr float pts[11][2] = { { -20, 0 }, { -10, .2f }, { -7, .32f }, { -5, .44f }, { -3, .57f }, { -2, .64f },
                                          { -1, .71f }, { 0, .78f }, { 1, .855f }, { 2, .93f }, { 3, 1 } };

    Theme& theme;
    std::function<Reading()> src;
    std::vector<float> refs;
    juce::StringArray labels;
    bool hasOff = false;
    int mode = 0;
    double needle = -20, vel = 0, grSm = 0, lastT = 0;
    float grShown = 0, outShown = -20;
};
} // namespace audiolab
