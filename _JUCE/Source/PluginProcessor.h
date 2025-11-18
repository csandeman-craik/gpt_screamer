/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>
#include "GptScreamer.h"

//==============================================================================
/**
*/
class GptScreamerAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    GptScreamerAudioProcessor();
    ~GptScreamerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GptScreamerAudioProcessor)
    
    juce::dsp::Oversampling<float> oversampling { 1, 2, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true };
    juce::dsp::FirstOrderTPTFilter<float> inputHP;
    juce::dsp::FirstOrderTPTFilter<float> preToneLP;
    juce::dsp::WaveShaper<float> clipper;
    juce::dsp::Gain<float> drive;
    
    GptScreamer ts;
};
