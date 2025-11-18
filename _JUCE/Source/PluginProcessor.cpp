/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/
#include <cmath>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GptScreamer.h"

//==============================================================================
GptScreamerAudioProcessor::GptScreamerAudioProcessor()
//#ifndef JucePlugin_PreferredChannelConfigurations
//     : AudioProcessor (BusesProperties()
//                       .withInput  ("Input",  juce::AudioChannelSet::mono(), true)
//                       .withOutput ("Output", juce::AudioChannelSet::mono(), true)
//                       )
//#endif
{
    ts.setFs(44100);
    inputHP.setType(juce::dsp::FirstOrderTPTFilterType::highpass);
    preToneLP.setType(juce::dsp::FirstOrderTPTFilterType::lowpass);
    clipper.functionToUse = [](float x) { return std::tanh(x); };
}

GptScreamerAudioProcessor::~GptScreamerAudioProcessor()
{
}

//==============================================================================
const juce::String GptScreamerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool GptScreamerAudioProcessor::acceptsMidi() const
{
    return false;
}

bool GptScreamerAudioProcessor::producesMidi() const
{
    return false;
}

bool GptScreamerAudioProcessor::isMidiEffect() const
{
    return false;
}

double GptScreamerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int GptScreamerAudioProcessor::getNumPrograms()
{
    return 1;
}

int GptScreamerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void GptScreamerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String GptScreamerAudioProcessor::getProgramName (int index)
{
    return {};
}

void GptScreamerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void GptScreamerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Create a spec for the oversampled audio
    double oversampledRate = sampleRate * 4.0; // 2^2

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = oversampledRate;
    spec.maximumBlockSize = samplesPerBlock * 4;
    spec.numChannels = 1; // Mono

    // Prepare all our components with this spec
    oversampling.initProcessing(samplesPerBlock);
    inputHP.prepare(spec);
    preToneLP.prepare(spec);
    clipper.prepare(spec);
    drive.prepare(spec);

    // Set initial values
    inputHP.setCutoffFrequency(723.0f);
    preToneLP.setCutoffFrequency(1500.0f);
    
    // Set the drive (60.0f is a gain of +35.5 dB, which is huge!)
    // We set gain in decibels for a more natural knob feel
    drive.setGainDecibels(35.5f);
}

void GptScreamerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GptScreamerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void GptScreamerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::AudioBlock<float> ovBlock;

    // 1. Up-sample
    ovBlock = oversampling.processSamplesUp(block);
    
    // Create a processing context for the oversampled block
    juce::dsp::ProcessContextReplacing<float> context(ovBlock);

    // 2. Run the DSP chain (in order)
    inputHP.process(context);
    preToneLP.process(context);
    drive.process(context);
    clipper.process(context);
    
    // 3. Down-sample
    oversampling.processSamplesDown(block);
    
    // 4. Clear other channels (if host is stereo)
    for (int ch = 1; ch < block.getNumChannels(); ++ch)
        block.getSingleChannelBlock(ch).clear();
}

//==============================================================================
bool GptScreamerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* GptScreamerAudioProcessor::createEditor()
{
    return new GptScreamerAudioProcessorEditor (*this);
}

//==============================================================================
void GptScreamerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void GptScreamerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GptScreamerAudioProcessor();
}
