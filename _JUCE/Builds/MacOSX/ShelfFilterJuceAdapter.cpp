#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>

template <typename T>
class ShelfFilterJuceAdapter : public juce::dsp::IIR::Filter<T>
{
public:
    ShelfFilterJuceAdapter() : shelfFilter() {}
    
    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        juce::dsp::IIR::Filter<T>::prepare(spec);
        shelfFilter.setup (spec.sampleRate, spec.maximumBlockSize);
    }

    void process (const juce::dsp::ProcessContextReplacing<T>& context) override
    {
        auto& block = context.getInputBlock();
        const auto numChannels = block.getNumChannels();
        const auto numSamples = block.getNumSamples();
        
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            T* channelData = block.getChannelPointer (ch);
            shelfFilter.processBlock (channelData, block, (int)numSamples, (int)ch);
        }
    }

private:
    // Hold an instance of your library's filter
    chowdsp::ShelfFilter<T> shelfFilter;
};
