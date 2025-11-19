/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GptScreamerAudioProcessorEditor::GptScreamerAudioProcessorEditor (GptScreamerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
}

GptScreamerAudioProcessorEditor::~GptScreamerAudioProcessorEditor()
{
}

//==============================================================================
void GptScreamerAudioProcessorEditor::paint (juce::Graphics& g)
{
    // 1. Set up the Tone Slider
    toneSlider.setSliderStyle(juce::Slider::Rotary);
    toneSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 50, 20);
    addAndMakeVisible(toneSlider);
    toneAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        *audioProcessor.apvts, "TONE", toneSlider));

    // 2. Set up the Drive Slider
    driveSlider.setSliderStyle(juce::Slider::Rotary);
    driveSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 50, 20);
    addAndMakeVisible(driveSlider);
    driveAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        *audioProcessor.apvts, "DRIVE", driveSlider));
    
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("GPT Screamer!", getLocalBounds(), juce::Justification::centred, 1);
}

void GptScreamerAudioProcessorEditor::resized()
{
    // Set bounds for the Tone Slider (e.g., in the top left quadrant)
    toneSlider.setBounds(50, 50, 150, 150);
        
    // Set bounds for the Drive Slider (e.g., in the top right quadrant)
    driveSlider.setBounds(200, 50, 150, 150);
}
