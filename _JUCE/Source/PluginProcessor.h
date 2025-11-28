/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>
#include <chowdsp_filters/LowerOrderFilters/chowdsp_FirstOrderFilters.h>

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
    
    //==============================================================================
    void updateBassShelf(float tone);
    void updateTrebShelf(float tone);

    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GptScreamerAudioProcessor)
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // oversampler 2^3
    juce::dsp::Oversampling<float> oversampling { 1, 3, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true };
    float oversampledRate;
    
    // limiter
    juce::dsp::Limiter<float> outputLimiter;
    
    // these are static filters which won't change
    juce::dsp::IIR::Filter<float> inputHP;
    juce::dsp::IIR::Filter<float> preClipLP;
    juce::dsp::IIR::Filter<float> preToneLP;
    juce::dsp::IIR::Filter<float> outputHP;
    
    // dynamic shelving that will change depending on tone - internal smoothing
//    chowdsp::ShelfFilter<float> bassShelf;
    juce::dsp::IIR::Filter<float> bassShelf;
    float currentBassBoostFactor = 0.0f;
    

//    chowdsp::ShelfFilter<float> trebShelf;
    juce::dsp::IIR::Filter<float> trebShelf;
    float currentTrebBoostFactor = 0.0f; // Represents (Av - 1)
    
    // clipping components
    juce::dsp::WaveShaper<float> clipper;
    juce::dsp::Gain<float> drive; // has internal smoothing
    
    // buffer for the parallel processed treble boost
    juce::AudioBuffer<float> bassBuffer;
    juce::AudioBuffer<float> trebBuffer;
    
    // const for algorithms
    const float input_HP_Fc = 723.4f;
    const float pre_clip_LP_Fc = 15000.0f;
    const float pre_tone_LP_Fc = 723.4f;
    const float output_HP_Fc = 30.0f;
    const float R_bass = 1000.0f; // 1KΩ
    const float R_tone_pot = 20000.0f; // 20KΩ
    const float R_feed = 1000.0f; // 1KΩ
    const float R_shunt = 220.0f; // 220Ω
    const float C_shunt = 220e-9f; // 220nF
    
    juce::SmoothedValue<float> smoothedTone { 0.5f }; // Initialize with default
    float previousTone = 0.5f;
};
