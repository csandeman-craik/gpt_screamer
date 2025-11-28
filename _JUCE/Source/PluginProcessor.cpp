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
    clipper.functionToUse = [](float x) {
//        DBG("input x is: " << x);
        float v = x * 3;
        double Vth = 0.55;
        double k = 8.0;
        double a = fabs(v);
        double s = (v >= 0.0) ? 1.0 : -1.0;
        if (a <= Vth) return v;
        double tail = (1.0 - std::exp(-k * (a - Vth))) / k;
        return static_cast<float>(s * (Vth + tail));
    };
//    clipper.functionToUse = [](float x) { return std::tanh(x); };
//    clipper.functionToUse = [](float x) { return antiParallel1N914(x); };
    
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
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), // Range: start, end, interval
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
    const float sampleRateMultiplier = 8.0f;
    oversampledRate = sampleRate * sampleRateMultiplier; // 2^3
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = oversampledRate;
    spec.maximumBlockSize = samplesPerBlock * sampleRateMultiplier;
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
    outputLimiter.prepare(spec);
    outputLimiter.setThreshold (-0.3f); // dB
    outputLimiter.setRelease (50.0f);
    
    // set initial static filter values
    *inputHP.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(oversampledRate, input_HP_Fc);
    *preClipLP.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, pre_clip_LP_Fc);
    *preToneLP.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, pre_tone_LP_Fc);
    *outputHP.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(oversampledRate, output_HP_Fc);

    // set initial dynamic shelf values
    const float rampDurationSeconds = 0.001f; // 1 millisecond
    smoothedTone.reset (oversampledRate, rampDurationSeconds);
    smoothedTone.setCurrentAndTargetValue (previousTone);
    updateBassShelf(previousTone);
    updateTrebShelf(previousTone);
    
    // Set the drive (60.0f is a gain of +35.5 dB, which is huge!)
    // We set gain in decibels for a more natural knob feel
    drive.setGainDecibels(50.0f);
    
    // prepare the buffer for parallel processing
    trebBuffer.setSize (1, samplesPerBlock * sampleRateMultiplier, false, true, true);
    bassBuffer.setSize (1, samplesPerBlock * sampleRateMultiplier, false, true, true);
}

inline float gTaper(float t) noexcept
{
    return t * t * (3.0f - 2.0f * t); // smoothstep: 3t^2 - 2t^3
}

inline float map_inverse_extreme_taper(float t) noexcept
{
    float knobBreakPoint = 0.5f;
    float trebleStretchPercent = 0.90f;
    
    // clamp the input to the [0, 1] range for safety
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    if (t <= knobBreakPoint)
    {
        float u = t / knobBreakPoint; // map to 0.0 - 1.0
        const float P = 0.5f; // Exponential factor: Try P = 0.25 for a very steep start.
        float u_curved = std::pow(u, P); // anti-log (exponential) curve.
        return u_curved * trebleStretchPercent; // scale to output range 0..trebleStretchPercent
    }
    else
    {
        float u = (t - knobBreakPoint) / (1 - knobBreakPoint); // normalized 0..1 (from 0.5 -> 1.0)
        return trebleStretchPercent + u * (1 - trebleStretchPercent); // Start_Value + u * Output_Span
    }
}

void GptScreamerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // thread-safe read of the slider values
    float rawTone = apvts->getRawParameterValue("TONE")->load();
    float currentTone = map_inverse_extreme_taper(rawTone); // doesn't seem to require smoothing!
    
//    float newToneTarget = map_inverse_extreme_taper(rawTone);
//    smoothedTone.setTargetValue(newToneTarget);

    if(currentTone != previousTone)
    {
        updateBassShelf(currentTone);
        updateTrebShelf(currentTone);
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
    inputHP.process(ovContext);
    preClipLP.process(ovContext);
    drive.process(ovContext);
    clipper.process(ovContext);
    preToneLP.process(ovContext);

    juce::dsp::AudioBlock<float> bassBlock(bassBuffer);
    bassBlock.copyFrom(ovBlock);
    juce::dsp::ProcessContextReplacing<float> bassContext(bassBlock);
    
    juce::dsp::AudioBlock<float> trebBlock(trebBuffer);
    trebBlock.copyFrom(ovBlock);
    juce::dsp::ProcessContextReplacing<float> trebContext(trebBlock);
    
    // process tone-stack
    bassShelf.process(bassContext);
    trebShelf.process(trebContext);

    // recombine
    ovBlock.add(bassBlock.multiplyBy(currentBassBoostFactor * -1.0f));
    ovBlock.add(trebBlock.multiplyBy(currentTrebBoostFactor));
    
    // process, limit and output
    outputHP.process(ovContext);
    outputLimiter.process(ovContext);
    oversampling.processSamplesDown(block);
}

void GptScreamerAudioProcessor::updateBassShelf(float tone)
{
    float R_shunt_total_bass = (tone * R_tone_pot) + R_shunt;
    float Fp_bass = 1 / (2 * M_PI * (R_bass + R_shunt_total_bass) * C_shunt);
    
    if (std::isinf(Fp_bass) || std::isnan(Fp_bass))
        {
            DBG("!!! NaN/INF DETECTED in Fp_bass !!!");
            DBG("R_shunt_total_bass: " << R_shunt_total_bass);
            
            // Safety: Clamp to a known-safe value to prevent the crash
            // and allow the plugin to continue running for better debugging.
            Fp_bass = 20.0f; // Use a reasonable low frequency (e.g., 20 Hz)
        }
    
    currentBassBoostFactor = 1.0f - (R_shunt_total_bass / (R_bass + R_shunt_total_bass));
    DBG("Fp_bass: " << Fp_bass << " | currentBassBoostFactor " << currentBassBoostFactor);
    
    *bassShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(oversampledRate, Fp_bass);
}

void GptScreamerAudioProcessor::updateTrebShelf(float tone)
{
    float R_shunt_total_treb = ((1 - tone) * R_tone_pot) + R_shunt;
    float Fp_treb = 1 / (2 * M_PI * R_shunt_total_treb * C_shunt);
    
    if (std::isinf(Fp_treb) || std::isnan(Fp_treb))
        {
            DBG("!!! NaN/INF DETECTED in Fp_treb !!!");
            DBG("R_shunt_total_treb: " << R_shunt_total_treb);

            // Safety: Clamp to a known-safe value to prevent the crash.
            // Use a reasonable high frequency (e.g., 20 kHz)
            Fp_treb = 20000.0f;
        }

    currentTrebBoostFactor = R_feed / R_shunt_total_treb;
    DBG("Fp_treb: " << Fp_treb << " | currentTrebBoostFactor " << currentTrebBoostFactor);

    *trebShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(oversampledRate, Fp_treb);
}

void GptScreamerAudioProcessor::releaseResources() {}

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
