#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"

class PhaseWidthProcessor : public juce::AudioProcessor
{
public:
    PhaseWidthProcessor();
    ~PhaseWidthProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Phase Width 4"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    pw::Engine engine;
    std::atomic<bool> gonioOn { true };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    pw::Params readParams() const;
    std::atomic<float>* xo[3];
    std::atomic<float>* tgt[pw::kBands];
    std::atomic<float>* amt[pw::kBands];
    std::atomic<float>* atk[pw::kBands];
    std::atomic<float>* dec[pw::kBands];
    std::atomic<float>* solo[pw::kBands];
    std::atomic<float>* bypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseWidthProcessor)
};
