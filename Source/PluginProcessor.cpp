#include "PluginProcessor.h"
#include "PluginEditor.h"

using juce::String;

juce::AudioProcessorValueTreeState::ParameterLayout PhaseWidthProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using BP = juce::AudioParameterBool;
    using PID = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const float xoDef[3] = { 200.f, 1000.f, 5000.f };
    for (int i = 0; i < 3; ++i)
        layout.add (std::make_unique<FP> (PID { "xo" + String (i + 1), 1 }, "Crossover " + String (i + 1),
                                          juce::NormalisableRange<float> (20.f, 20000.f, 0.f, 0.3f), xoDef[i],
                                          juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    for (int b = 1; b <= pw::kBands; ++b)
    {
        const String n (b);
        layout.add (std::make_unique<FP> (PID { "tgt" + n, 1 }, "Band " + n + " Phase Target", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
        layout.add (std::make_unique<FP> (PID { "amt" + n, 1 }, "Band " + n + " Amount", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f));
        layout.add (std::make_unique<FP> (PID { "atk" + n, 1 }, "Band " + n + " Attack", juce::NormalisableRange<float> (0.f, 2.f, 0.001f), 0.f,
                                          juce::AudioParameterFloatAttributes().withLabel ("s")));
        layout.add (std::make_unique<FP> (PID { "dec" + n, 1 }, "Band " + n + " Decay", juce::NormalisableRange<float> (0.f, 2.f, 0.001f), 0.f,
                                          juce::AudioParameterFloatAttributes().withLabel ("s")));
        layout.add (std::make_unique<BP> (PID { "solo" + n, 1 }, "Band " + n + " Solo", false));
    }
    layout.add (std::make_unique<BP> (PID { "bypass", 1 }, "Bypass", false));
    return layout;
}

PhaseWidthProcessor::PhaseWidthProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < 3; ++i) xo[i] = apvts.getRawParameterValue ("xo" + String (i + 1));
    for (int b = 0; b < pw::kBands; ++b)
    {
        const String n (b + 1);
        tgt[b] = apvts.getRawParameterValue ("tgt" + n);
        amt[b] = apvts.getRawParameterValue ("amt" + n);
        atk[b] = apvts.getRawParameterValue ("atk" + n);
        dec[b] = apvts.getRawParameterValue ("dec" + n);
        solo[b] = apvts.getRawParameterValue ("solo" + n);
    }
    bypass = apvts.getRawParameterValue ("bypass");
}

bool PhaseWidthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PhaseWidthProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

pw::Params PhaseWidthProcessor::readParams() const
{
    pw::Params p;
    for (int i = 0; i < 3; ++i) p.crossover[i] = xo[i]->load();
    for (int b = 0; b < pw::kBands; ++b)
    {
        p.target[b] = tgt[b]->load(); p.amount[b] = amt[b]->load();
        p.attack[b] = atk[b]->load(); p.decay[b] = dec[b]->load();
        p.solo[b] = solo[b]->load() > 0.5f;
    }
    p.bypass = bypass->load() > 0.5f;
    p.gonioOn = gonioOn.load();
    return p;
}

void PhaseWidthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2) return;
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples(), readParams());
}

juce::AudioProcessorEditor* PhaseWidthProcessor::createEditor() { return new PhaseWidthEditor (*this); }

void PhaseWidthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void PhaseWidthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PhaseWidthProcessor(); }
