#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

class PrimaTakePerspectiveAudioProcessor final : public juce::AudioProcessor
{
public:
    PrimaTakePerspectiveAudioProcessor();
    ~PrimaTakePerspectiveAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.25; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float getInputMeter() const noexcept { return inputMeter.load(); }
    float getOutputMeter() const noexcept { return outputMeter.load(); }

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;

    juce::AudioProcessorValueTreeState apvts;

    juce::dsp::ProcessorDuplicator<Filter, Coefficients> toneLow;
    juce::dsp::ProcessorDuplicator<Filter, Coefficients> toneHigh;
    juce::dsp::ProcessorDuplicator<Filter, Coefficients> monoLowFilter;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> delayBuffer;

    int delayWritePosition = 0;
    double currentSampleRate = 44100.0;

    juce::SmoothedValue<float> distanceSmoothed;
    juce::SmoothedValue<float> widthSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> outputSmoothed;

    std::atomic<float> inputMeter { 0.0f };
    std::atomic<float> outputMeter { 0.0f };

    void updatePerspectiveFilters (float distance, float focus, float air);
    void applyEarlyReflection (juce::AudioBuffer<float>& buffer,
                               float amount,
                               float distance);
    void applyStereoWidth (juce::AudioBuffer<float>& buffer, float width);
    void applyMonoLow (juce::AudioBuffer<float>& buffer, float amount);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrimaTakePerspectiveAudioProcessor)
};
