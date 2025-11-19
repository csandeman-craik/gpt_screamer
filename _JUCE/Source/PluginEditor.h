/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class GptScreamerAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    GptScreamerAudioProcessorEditor (GptScreamerAudioProcessor&);
    ~GptScreamerAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider toneSlider;
    juce::Slider driveSlider;
        
    // Attachments connect the Slider's position to the APVTS value
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    
    GptScreamerAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GptScreamerAudioProcessorEditor)
};
