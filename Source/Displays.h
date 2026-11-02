// Displays.h - the custom display components (ports of the Max jsui scripts + meters + goniometer).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Dsp.h"

namespace pwui
{
using juce::Colour;
using juce::Graphics;

inline Colour grey()  { return Colour::fromFloatRGBA (0.55f, 0.55f, 0.58f, 1.f); }
inline Colour green() { return Colour::fromFloatRGBA (0.15f, 0.70f, 0.30f, 1.f); }
inline Colour red()   { return Colour::fromFloatRGBA (0.88f, 0.16f, 0.16f, 1.f); }
inline Colour track() { return Colour::fromFloatRGBA (0.10f, 0.10f, 0.12f, 1.f); }
inline Colour frame() { return Colour::fromFloatRGBA (0.45f, 0.45f, 0.50f, 1.f); }
inline Colour text()  { return Colour::fromFloatRGBA (0.75f, 0.75f, 0.78f, 1.f); }

// colour by size of the width change as a share of full scale: under 1% grey, up to 90% green, beyond red
inline Colour colourFor (float frac)
{
    if (frac <= 0.01f + 1e-6f) return grey();
    if (frac <= 0.9f + 1e-6f) return green();
    return red();
}

// ---------------------------------------------------------------------------------------------
// 0..1 phase reading bar (0 = mono, 0.5 = uncorrelated, 1 = fully out of phase)
class PhaseBar : public juce::Component
{
public:
    void setValue (float v) { v = juce::jlimit (0.f, 1.f, v); if (v != val) { val = v; repaint(); } }
    void paint (Graphics& g) override
    {
        const float w = (float) getWidth(), barH = 16.f;
        g.fillAll (Colour::fromFloatRGBA (0.06f, 0.06f, 0.08f, 1.f));
        g.setColour (track());           g.fillRect (0.f, 0.f, w, barH);
        g.setColour (val < 0.01f ? grey() : Colour::fromFloatRGBA (0.2f, 0.62f, 0.85f, 1.f));
        g.fillRect (0.f, 1.f, val * w, barH - 2.f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawLine (w * 0.25f, 0.f, w * 0.25f, barH); g.drawLine (w * 0.75f, 0.f, w * 0.75f, barH);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawLine (w * 0.5f, 0.f, w * 0.5f, barH, 2.f);
        g.setColour (frame());           g.drawRect (0.5f, 0.5f, w - 1.f, barH - 1.f, 1.f);
        g.setFont (10.f);
        g.setColour (text());
        g.drawText ("mono", 2, (int) barH, 60, 14, juce::Justification::centredLeft);
        g.drawText ("out of phase", getWidth() - 92, (int) barH, 90, 14, juce::Justification::centredRight);
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.drawText (juce::String (val, 2), getWidth() / 2 - 25, (int) barH, 50, 14, juce::Justification::centred);
    }
private:
    float val = 0.f;
};

// ---------------------------------------------------------------------------------------------
// "What did the engine do?": width change in dB (top) and input/output phase markers (bottom)
class ChangeBar : public juce::Component
{
public:
    void setValues (float in, float out)
    {
        in = juce::jlimit (0.f, 1.f, in); out = juce::jlimit (0.f, 1.f, out);
        if (in != pin || out != pout) { pin = in; pout = out; repaint(); }
    }
    void paint (Graphics& g) override
    {
        const float w = (float) getWidth(), h = (float) getHeight(), mid = w * 0.5f;
        const float range = 12.f;
        const float db = 10.f * std::log10 (odds (pout) / odds (pin));
        const float shown = juce::jlimit (-range, range, db);
        const Colour c = colourFor (std::abs (shown) / range);
        const float wx = mid + shown / range * mid;
        const float topH = 16.f, trackY = 46.f, trackH = 8.f, cy = trackY + trackH * 0.5f;
        const float ix = pin * w, ox = pout * w;

        g.fillAll (Colour::fromFloatRGBA (0.06f, 0.06f, 0.08f, 1.f));
        g.setColour (track()); g.fillRect (0.f, 0.f, w, topH);
        g.setColour (c);
        if (wx >= mid) g.fillRect (mid, 1.f, wx - mid, topH - 2.f); else g.fillRect (wx, 1.f, mid - wx, topH - 2.f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawLine (w * 0.25f, 0.f, w * 0.25f, topH); g.drawLine (w * 0.75f, 0.f, w * 0.75f, topH);
        g.setColour (juce::Colours::white.withAlpha (0.9f)); g.drawLine (mid, 0.f, mid, topH, 2.f);
        g.setColour (frame()); g.drawRect (0.5f, 0.5f, w - 1.f, topH - 1.f, 1.f);

        g.setFont (10.f); g.setColour (text());
        g.drawText ("narrower", 2, (int) topH, 60, 13, juce::Justification::centredLeft);
        g.drawText ("wider", getWidth() - 62, (int) topH, 60, 13, juce::Justification::centredRight);
        const juce::String label = std::abs (db) < 0.05f ? "0.0 dB" : ((db > 0 ? "+" : "") + juce::String (db, 1) + " dB");
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.drawText (label, getWidth() / 2 - 40, (int) topH, 80, 13, juce::Justification::centred);

        g.setColour (track()); g.fillRect (0.f, trackY, w, trackH);
        g.setColour (c.withAlpha (0.85f));
        g.fillRect (std::min (ix, ox), trackY + 1.f, std::abs (ox - ix), trackH - 2.f);
        juce::Path tri;                                            // input pointer
        tri.addTriangle (ix - 4.f, trackY - 8.f, ix + 4.f, trackY - 8.f, ix, trackY);
        g.setColour (juce::Colours::white.withAlpha (0.95f)); g.fillPath (tri);
        g.setColour (c); g.fillEllipse (ox - 6.f, cy - 6.f, 12.f, 12.f);   // output dot
        g.setColour (juce::Colours::white.withAlpha (0.9f)); g.drawEllipse (ox - 6.f, cy - 6.f, 12.f, 12.f, 1.f);
        g.setColour (frame()); g.drawRect (0.5f, trackY + 0.5f, w - 1.f, trackH - 1.f, 1.f);

        g.setColour (text());
        g.drawText ("mono", 2, (int) (trackY + trackH + 1), 60, 13, juce::Justification::centredLeft);
        g.drawText ("out of phase", getWidth() - 92, (int) (trackY + trackH + 1), 90, 13, juce::Justification::centredRight);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawText ("in " + juce::String (pin, 2) + "   out " + juce::String (pout, 2), 0, (int) h - 16, (int) w, 14, juce::Justification::centred);
    }
private:
    static float odds (float p) { p = juce::jlimit (0.01f, 0.99f, p); return p / (1.f - p); }
    float pin = 0.5f, pout = 0.5f;
};

// ---------------------------------------------------------------------------------------------
// Goniometer: Lissajous figure of one band's output, rotated 45 degrees (mono = vertical line,
// out of phase = horizontal line, wide stereo = round cloud). The picture is auto-scaled.
class Goniometer : public juce::Component
{
public:
    Goniometer (pw::Engine& e, int band) : engine (e), index (band) {}
    void setActive (bool a) { active = a; repaint(); }
    void paint (Graphics& g) override
    {
        const float w = (float) getWidth(), h = (float) getHeight();
        g.fillAll (Colour::fromFloatRGBA (0.04f, 0.07f, 0.05f, 1.f));
        g.setColour (Colour::fromFloatRGBA (0.2f, 0.35f, 0.25f, 1.f));
        g.drawLine (w * 0.5f, 0.f, w * 0.5f, h); g.drawLine (0.f, h * 0.5f, w, h * 0.5f);
        g.drawRect (0.5f, 0.5f, w - 1.f, h - 1.f, 1.f);
        if (! active) { g.setColour (text()); g.setFont (11.f); g.drawText ("off", getLocalBounds(), juce::Justification::centred); return; }

        const int n = 512;
        const int pos = engine.gonioPos[index].load (std::memory_order_acquire);
        const auto& buf = engine.gonio[(size_t) index];
        float peak = 0.05f;
        for (int i = 0; i < n; ++i)
        {
            const int k = ((pos - 1 - i) % pw::kGonioSize + pw::kGonioSize) % pw::kGonioSize;
            peak = std::max (peak, std::max (std::abs (buf[(size_t) k * 2]), std::abs (buf[(size_t) k * 2 + 1])));
        }
        scale += 0.15f * ((0.9f / peak) - scale);
        const float cx = w * 0.5f, cy = h * 0.5f, half = std::min (w, h) * 0.5f;
        for (int i = 0; i < n; ++i)
        {
            const int k = ((pos - 1 - i) % pw::kGonioSize + pw::kGonioSize) % pw::kGonioSize;
            const float x = buf[(size_t) k * 2] * scale, y = buf[(size_t) k * 2 + 1] * scale;
            g.setColour (Colour::fromFloatRGBA (0.35f, 1.f, 0.55f, 1.f - (float) i / (float) n));
            g.fillRect (cx + x * half - 0.75f, cy - y * half - 0.75f, 1.5f, 1.5f);
        }
    }
private:
    pw::Engine& engine; int index; bool active = true; float scale = 1.f;
};

// ---------------------------------------------------------------------------------------------
// Vertical peak meter, -60 .. +6 dB
class LevelMeter : public juce::Component
{
public:
    void setLevel (float lin) { const float d = lin > 1e-6f ? 20.f * std::log10 (lin) : -90.f; if (d != db) { db = d; repaint(); } }
    float getDb() const { return db; }
    void paint (Graphics& g) override
    {
        const float w = (float) getWidth(), h = (float) getHeight();
        g.fillAll (track()); g.setColour (frame()); g.drawRect (getLocalBounds());
        const float f = juce::jlimit (0.f, 1.f, (db + 60.f) / 66.f);
        g.setColour (db > 0.f ? red() : (db > -6.f ? juce::Colours::orange : green()));
        g.fillRect (1.f, h - 1.f - f * (h - 2.f), w - 2.f, f * (h - 2.f));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        const float y0 = h - 1.f - (60.f / 66.f) * (h - 2.f);          // 0 dB mark
        g.drawLine (0.f, y0, w, y0);
    }
private:
    float db = -90.f;
};
} // namespace pwui
