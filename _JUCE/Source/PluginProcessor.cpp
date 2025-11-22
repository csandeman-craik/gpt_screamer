/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/
#include <cmath>

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GptScreamerAudioProcessor::GptScreamerAudioProcessor()
//#ifndef JucePlugin_PreferredChannelConfigurations
//     : AudioProcessor (BusesProperties()
//                       .withInput  ("Input",  juce::AudioChannelSet::mono(), true)
//                       .withOutput ("Output", juce::AudioChannelSet::mono(), true)
//                       )
//#endif
{
    clipper.functionToUse = [](float x) { return std::tanh(x); };
    apvts.reset(new juce::AudioProcessorValueTreeState(*this, nullptr, "Parameters", createParameterLayout()));
}

GptScreamerAudioProcessor::~GptScreamerAudioProcessor() {}

//==============================================================================
const juce::String GptScreamerAudioProcessor::getName() const { return JucePlugin_Name; }

bool GptScreamerAudioProcessor::acceptsMidi() const { return false; }

bool GptScreamerAudioProcessor::producesMidi() const { return false; }

bool GptScreamerAudioProcessor::isMidiEffect() const { return false; }

double GptScreamerAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int GptScreamerAudioProcessor::getNumPrograms() { return 1; }

int GptScreamerAudioProcessor::getCurrentProgram() { return 0; }

void GptScreamerAudioProcessor::setCurrentProgram (int index) {}

const juce::String GptScreamerAudioProcessor::getProgramName (int index) { return {}; }

void GptScreamerAudioProcessor::changeProgramName (int index, const juce::String& newName) {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout GptScreamerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- TONE Parameter (Range 0.0 to 1.0) ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("TONE", 1),         // Parameter ID and version
        "Tone",                               // User-facing name
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.1f), // Range: start, end, interval
        0.5f                                  // Default value
    ));

    // --- DRIVE Parameter (Range 0 dB to 30 dB) ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DRIVE", 1),        // Parameter ID and version
        "Drive",                              // User-facing name
        juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f, 1.0f), // Range, interval, skew
        0.0f                                  // Default value
    ));

    return layout;
}

void GptScreamerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Create a spec for the oversampled audio
    oversampledRate = sampleRate * 4.0; // 2^2
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = oversampledRate;
    spec.maximumBlockSize = samplesPerBlock * 4;
    spec.numChannels = 1; // Mono

    // Prepare all our components with this spec
    oversampling.initProcessing(samplesPerBlock);
    inputHP.prepare(spec);
    preClipLP.prepare(spec);
    drive.prepare(spec);
    clipper.prepare(spec);
    preToneLP.prepare(spec);
    outputHP.prepare(spec);
    bassShelf.prepare(spec);
    trebShelf.prepare(spec);

    // set initial static filter values
    *inputHP.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(oversampledRate, input_HP_Fc);
    *preClipLP.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, pre_clip_LP_Fc);
    *preToneLP.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, pre_tone_LP_Fc);
    *outputHP.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(oversampledRate, output_HP_Fc);

    // set initial dynamic shelf values
    updateBassShelf(0.5f);
    updateTrebShelf(0.5f);
    
    // Set the drive (60.0f is a gain of +35.5 dB, which is huge!)
    // We set gain in decibels for a more natural knob feel
    drive.setGainDecibels(50.0f);
    
    // prepare the buffer for parallel processing
    trebBuffer.setSize (1, samplesPerBlock * 4, false, true, true);
}

void GptScreamerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // thread-safe read of the slider values
    float currentTone = apvts->getRawParameterValue("TONE")->load();
    if(currentTone != previousTone)
    {
        updateBassShelf(currentTone);
//        updateTrebShelf(currentTone);
        previousTone = currentTone;
    }
    drive.setGainDecibels(apvts->getRawParameterValue("DRIVE")->load());
    
    // up-sample
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::AudioBlock<float> ovBlock;
    ovBlock = oversampling.processSamplesUp(block);
    
    // create a processing context for the oversampled block
    juce::dsp::ProcessContextReplacing<float> ovContext(ovBlock);

    // run the DSP chain (in order)
//    inputHP.process(ovContext);
//    preClipLP.process(ovContext);
    drive.process(ovContext);
//    clipper.process(ovContext);
//    preToneLP.process(ovContext);
    
    // copy the processed ovBlock ready for parallel processing
//    juce::dsp::AudioBlock<float> trebBlock(trebBuffer);
//    trebBlock.copyFrom(ovBlock);
//    juce::dsp::ProcessContextReplacing<float> trebContext(trebBlock);
    
    // process tone-stack
    bassShelf.processBlock(ovBlock);
//    trebShelf.process(trebContext);
    
    // recombine
//    ovBlock.multiplyBy(1.0f - currentTone);
//    trebBlock.multiplyBy(currentTone);
//    ovBlock.add(trebBlock);
    
//    outputHP.process(ovContext);
    
    // down-sample
    oversampling.processSamplesDown(block);
}

void GptScreamerAudioProcessor::updateBassShelf(float tone)
{
    float R_shunt_total_bass = (tone * R_tone_pot) + R_shunt;
    float Fp_bass = 1 / (2 * M_PI * (R_bass + R_shunt_total_bass) * C_shunt);
    float lo_cut_db = 20 * log10(R_shunt_total_bass / (R_bass + R_shunt_total_bass));
    float targetGain = juce::Decibels::decibelsToGain(lo_cut_db);
    DBG("Fp_bass: " << Fp_bass << " | lo_cut_db " << lo_cut_db << " | targetGain: " << targetGain);
    bassShelf.calcCoefs(0.0f, targetGain, Fp_bass, oversampledRate);
}

void GptScreamerAudioProcessor::updateTrebShelf(float tone)
{
    float R_shunt_total_treb = ((1 - tone) * R_tone_pot) + R_shunt;
    float Fp_treb = 1 / (2 * M_PI * R_shunt_total_treb * C_shunt);
    float Av = 1 + R_feed / R_shunt_total_treb;
    float hi_boost_db = 20 * log10(Av);
    float targetGain = juce::Decibels::decibelsToGain(hi_boost_db);
    DBG("Fp_treb: " << Fp_treb << " | hi_boost_db " << hi_boost_db << " | targetGain: " << targetGain);
    *trebShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(oversampledRate, Fp_treb, Q_treb, targetGain);
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
