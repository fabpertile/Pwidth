#pragma once
#include "PluginProcessor.h"
#include "Displays.h"

class PhaseWidthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PhaseWidthEditor (PhaseWidthProcessor&);
    ~PhaseWidthEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct BandUI
    {
        BandUI (pw::Engine& e, int band) : gon (e, band) {}
        juce::Label header, lblTarget, lblDyn, lblIn, lblOut, lblChange, lblGon;
        juce::Slider target, amount, attack, decay;
        juce::ToggleButton solo { "solo" };
        juce::TextButton resetTarget { "reset" }, resetDial { "reset" };
        pwui::PhaseBar inBar, outBar;
        pwui::ChangeBar change;
        pwui::Goniometer gon;
        std::unique_ptr<SliderAtt> aTarget, aAmount, aAttack, aDecay;
        std::unique_ptr<ButtonAtt> aSolo;
        // smoothed readings: slow = 3 s average (number readouts / bars), fast = 0.3 s (change display)
        float inSlow = 0.5f, outSlow = 0.5f, inFast = 0.5f, outFast = 0.5f;
    };

    void timerCallback() override;
    void resetParam (const juce::String& id);
    void resetAll();
    void styleLabel (juce::Label&, const juce::String& text, float size = 12.f);
    void styleDial (juce::Slider&, const juce::String& suffix);

    PhaseWidthProcessor& proc;
    std::array<std::unique_ptr<BandUI>, pw::kBands> bands;

    juce::Label title, xoLabel, inMeterLabel, outMeterLabel, inDb, outDb, inLR, outLR;
    juce::Slider xo[3];
    juce::TextButton xoReset[3] { juce::TextButton ("reset"), juce::TextButton ("reset"), juce::TextButton ("reset") };
    std::unique_ptr<SliderAtt> aXo[3];
    juce::ToggleButton bypass { "BYPASS" }, gonioToggle { "goniometers on" };
    juce::TextButton masterReset { "MASTER RESET ALL" };
    std::unique_ptr<ButtonAtt> aBypass;
    pwui::LevelMeter inMeter[2], outMeter[2];
    float inHold[2] = { 0, 0 }, outHold[2] = { 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseWidthEditor)
};
