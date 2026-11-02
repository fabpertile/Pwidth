#include "PluginEditor.h"

using juce::String;
static const int kColW = 250, kLeft = 20, kTop = 130;

void PhaseWidthEditor::styleLabel (juce::Label& l, const String& t, float size)
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (juce::FontOptions (size));
    l.setColour (juce::Label::textColourId, pwui::text());
    l.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (l);
}

void PhaseWidthEditor::styleDial (juce::Slider& s, const String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 16);
    s.setTextValueSuffix (suffix);
    s.setNumDecimalPlacesToDisplay (2);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);
}

void PhaseWidthEditor::resetParam (const String& id)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
}

void PhaseWidthEditor::resetAll()
{
    for (int i = 1; i <= 3; ++i) resetParam ("xo" + String (i));
    for (int b = 1; b <= pw::kBands; ++b) { resetParam ("tgt" + String (b)); resetParam ("amt" + String (b)); }
}

PhaseWidthEditor::PhaseWidthEditor (PhaseWidthProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    styleLabel (title, "PHASE WIDTH 4   -   four-band dynamic stereo width / phase", 15.f);
    title.setColour (juce::Label::textColourId, juce::Colours::white);
    title.setBounds (kLeft, 8, 700, 24);
    styleLabel (xoLabel, "CROSSOVERS (Hz)   low < mid < high", 12.f);
    xoLabel.setBounds (kLeft, 40, 300, 18);

    for (int i = 0; i < 3; ++i)
    {
        auto& s = xo[i];
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setTextBoxIsEditable (true);
        s.setTextValueSuffix (" Hz");
        s.setNumDecimalPlacesToDisplay (0);
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffff3030));      // red text
        s.setColour (juce::Slider::trackColourId, juce::Colour (0xff2a2a30));
        s.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff101014));
        addAndMakeVisible (s);
        aXo[i] = std::make_unique<SliderAtt> (proc.apvts, "xo" + String (i + 1), s);
        s.setDoubleClickReturnValue (true, i == 0 ? 200.0 : (i == 1 ? 1000.0 : 5000.0));
        s.setBounds (kLeft + i * 190, 62, 120, 24);
        addAndMakeVisible (xoReset[i]);
        xoReset[i].setBounds (kLeft + i * 190 + 126, 62, 50, 24);
        xoReset[i].onClick = [this, i] { resetParam ("xo" + String (i + 1)); };
    }

    const char* names[4] = { "below crossover 1", "between crossovers 1 and 2", "between crossovers 2 and 3", "above crossover 3" };
    for (int b = 0; b < pw::kBands; ++b)
    {
        bands[(size_t) b] = std::make_unique<BandUI> (proc.engine, b);
        auto& u = *bands[(size_t) b];
        const String n (b + 1);
        styleLabel (u.header, "BAND " + n + "   (" + names[b] + ")", 12.f);
        u.header.setColour (juce::Label::textColourId, juce::Colours::white);
        styleLabel (u.lblTarget, "phase target  (0 mono - 0.5 none - 1 out of phase)", 11.f);
        styleLabel (u.lblDyn, "amount / attack / decay", 11.f);
        styleLabel (u.lblIn, "measured input phase (3 s average)", 11.f);
        styleLabel (u.lblOut, "measured output phase (3 s average)", 11.f);
        styleLabel (u.lblChange, "change: width (dB), phase in / out", 11.f);
        styleLabel (u.lblGon, "output goniometer", 11.f);

        u.target.setSliderStyle (juce::Slider::LinearHorizontal);
        u.target.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        u.target.setNumDecimalPlacesToDisplay (2);
        u.target.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        u.target.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (u.target);
        styleDial (u.amount, ""); styleDial (u.attack, " s"); styleDial (u.decay, " s");
        u.target.setDoubleClickReturnValue (true, 0.5);
        u.amount.setDoubleClickReturnValue (true, 0.0);
        u.aTarget = std::make_unique<SliderAtt> (proc.apvts, "tgt" + n, u.target);
        u.aAmount = std::make_unique<SliderAtt> (proc.apvts, "amt" + n, u.amount);
        u.aAttack = std::make_unique<SliderAtt> (proc.apvts, "atk" + n, u.attack);
        u.aDecay = std::make_unique<SliderAtt> (proc.apvts, "dec" + n, u.decay);
        addAndMakeVisible (u.solo);
        u.solo.setColour (juce::ToggleButton::textColourId, pwui::text());
        u.aSolo = std::make_unique<ButtonAtt> (proc.apvts, "solo" + n, u.solo);
        addAndMakeVisible (u.resetTarget); addAndMakeVisible (u.resetDial);
        u.resetTarget.onClick = [this, n] { resetParam ("tgt" + n); };
        u.resetDial.onClick = [this, n] { resetParam ("amt" + n); };
        addAndMakeVisible (u.inBar); addAndMakeVisible (u.outBar); addAndMakeVisible (u.change); addAndMakeVisible (u.gon);
    }

    // right-hand panel
    styleLabel (inMeterLabel, "INPUT", 12.f); styleLabel (outMeterLabel, "OUTPUT", 12.f);
    styleLabel (inLR, "L      R", 11.f); styleLabel (outLR, "L      R", 11.f);
    styleLabel (inDb, "-inf", 11.f); styleLabel (outDb, "-inf", 11.f);
    const int mx = kLeft + 4 * kColW + 20;
    inMeterLabel.setBounds (mx, kTop, 80, 18);   outMeterLabel.setBounds (mx + 100, kTop, 80, 18);
    for (int c = 0; c < 2; ++c)
    {
        addAndMakeVisible (inMeter[c]); addAndMakeVisible (outMeter[c]);
        inMeter[c].setBounds (mx + c * 36, kTop + 24, 28, 220);
        outMeter[c].setBounds (mx + 100 + c * 36, kTop + 24, 28, 220);
    }
    inLR.setBounds (mx + 6, kTop + 246, 80, 16);   outLR.setBounds (mx + 106, kTop + 246, 80, 16);
    inDb.setBounds (mx, kTop + 264, 90, 16);       outDb.setBounds (mx + 100, kTop + 264, 90, 16);
    inMeterLabel.setText ("INPUT (peak dB)", juce::dontSendNotification);
    outMeterLabel.setText ("OUTPUT (peak dB)", juce::dontSendNotification);

    addAndMakeVisible (bypass);
    bypass.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    aBypass = std::make_unique<ButtonAtt> (proc.apvts, "bypass", bypass);
    bypass.setBounds (mx, kTop + 310, 190, 26);
    addAndMakeVisible (gonioToggle);
    gonioToggle.setColour (juce::ToggleButton::textColourId, pwui::text());
    gonioToggle.setToggleState (true, juce::dontSendNotification);
    gonioToggle.onClick = [this]
    {
        const bool on = gonioToggle.getToggleState();
        proc.gonioOn.store (on);
        for (auto& b : bands) b->gon.setActive (on);
    };
    gonioToggle.setBounds (mx, kTop + 346, 190, 26);
    addAndMakeVisible (masterReset);
    masterReset.onClick = [this] { resetAll(); };
    masterReset.setBounds (mx, kTop + 392, 190, 30);

    gonioToggle.setToggleState (proc.gonioOn.load(), juce::dontSendNotification);
    setSize (1230, 800);        // only now: setSize() calls resized(), which needs every control to exist
    startTimerHz (30);
}

PhaseWidthEditor::~PhaseWidthEditor() { stopTimer(); }

void PhaseWidthEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromFloatRGBA (0.08f, 0.08f, 0.10f, 1.f));
    g.setColour (juce::Colour::fromFloatRGBA (0.2f, 0.2f, 0.24f, 1.f));
    g.drawHorizontalLine (kTop - 14, (float) kLeft, (float) getWidth() - kLeft);
    for (int b = 1; b < pw::kBands; ++b)
        g.drawVerticalLine (kLeft + b * kColW - 8, (float) kTop, (float) getHeight() - 20);
    g.drawVerticalLine (kLeft + 4 * kColW + 8, (float) kTop, (float) getHeight() - 20);
}

void PhaseWidthEditor::resized()
{
    for (const auto& band : bands)
        if (band == nullptr) return;                       // controls not created yet

    for (int b = 0; b < pw::kBands; ++b)
    {
        auto& u = *bands[(size_t) b];
        const int x = kLeft + b * kColW;
        int y = kTop;
        u.header.setBounds (x, y, 235, 20);                              y += 24;
        u.lblTarget.setBounds (x, y, 235, 16);                           y += 18;
        u.target.setBounds (x, y, 180, 24);
        u.resetTarget.setBounds (x + 186, y, 46, 24);                    y += 28;
        u.solo.setBounds (x, y, 70, 22);                                 y += 28;
        u.lblDyn.setBounds (x, y, 235, 16);                              y += 18;
        u.amount.setBounds (x, y, 62, 78);
        u.attack.setBounds (x + 62, y, 62, 78);
        u.decay.setBounds (x + 124, y, 62, 78);
        u.resetDial.setBounds (x + 190, y + 4, 42, 22);                  y += 84;
        u.lblIn.setBounds (x, y, 235, 16);                               y += 18;
        u.inBar.setBounds (x, y, 180, 32);                               y += 40;
        u.lblOut.setBounds (x, y, 235, 16);                              y += 18;
        u.outBar.setBounds (x, y, 180, 32);                              y += 40;
        u.lblChange.setBounds (x, y, 235, 16);                           y += 18;
        u.change.setBounds (x, y, 180, 82);                              y += 90;
        u.lblGon.setBounds (x, y, 235, 16);                              y += 18;
        u.gon.setBounds (x, y, 180, 180);
    }
}

void PhaseWidthEditor::timerCallback()
{
    const float dt = 1.f / 30.f;
    const float aSlow = 1.f - std::exp (-dt / 3.0f), aFast = 1.f - std::exp (-dt / 0.3f);
    auto& e = proc.engine;
    for (int b = 0; b < pw::kBands; ++b)
    {
        auto& u = *bands[(size_t) b];
        const float in = e.inPhase[b].load(), out = e.outPhase[b].load();
        u.inSlow += aSlow * (in - u.inSlow);   u.outSlow += aSlow * (out - u.outSlow);
        u.inFast += aFast * (in - u.inFast);   u.outFast += aFast * (out - u.outFast);
        u.inBar.setValue (u.inSlow);  u.outBar.setValue (u.outSlow);
        u.change.setValues (u.inFast, u.outFast);
        if (gonioToggle.getToggleState()) u.gon.repaint();
    }
    for (int c = 0; c < 2; ++c)
    {
        const float pi = e.peakIn[c].exchange (0.f), po = e.peakOut[c].exchange (0.f);
        inHold[c] = std::max (pi, inHold[c] * std::pow (10.f, -20.f * dt / 20.f));      // fall at 20 dB per second
        outHold[c] = std::max (po, outHold[c] * std::pow (10.f, -20.f * dt / 20.f));
        inMeter[c].setLevel (inHold[c]);  outMeter[c].setLevel (outHold[c]);
    }
    auto fmt = [] (float a, float b)
    {
        auto one = [] (float v) { return v > 1e-5f ? String (20.f * std::log10 (v), 1) : String ("-inf"); };
        return one (a) + "   " + one (b);
    };
    inDb.setText (fmt (inHold[0], inHold[1]), juce::dontSendNotification);
    outDb.setText (fmt (outHold[0], outHold[1]), juce::dontSendNotification);
}
