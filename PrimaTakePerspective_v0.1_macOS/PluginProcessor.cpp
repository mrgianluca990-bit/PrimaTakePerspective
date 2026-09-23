#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

PrimaTakePerspectiveAudioProcessor::PrimaTakePerspectiveAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
PrimaTakePerspectiveAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "distance", "Distance",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f }, 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "focus", "Focus",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 55.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "air", "Air",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f }, 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "reflections", "Reflections",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 18.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "width", "Width",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 0.1f }, 100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "monolow", "Mono Low",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 35.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "output", "Output",
        juce::NormalisableRange<float> { -18.0f, 6.0f, 0.01f }, 0.0f, "dB"));

    return { params.begin(), params.end() };
}

bool PrimaTakePerspectiveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

void PrimaTakePerspectiveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const auto channels = static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()));
    juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32> (samplesPerBlock),
        channels
    };

    toneLow.prepare (spec);
    toneHigh.prepare (spec);
    monoLowFilter.prepare (spec);

    toneLow.reset();
    toneHigh.reset();
    monoLowFilter.reset();

    dryBuffer.setSize (static_cast<int> (channels), samplesPerBlock);

    const int maxDelaySamples = static_cast<int> (sampleRate * 0.08) + samplesPerBlock + 8;
    delayBuffer.setSize (static_cast<int> (channels), maxDelaySamples);
    delayBuffer.clear();
    delayWritePosition = 0;

    distanceSmoothed.reset (sampleRate, 0.05);
    widthSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.reset (sampleRate, 0.03);
    outputSmoothed.reset (sampleRate, 0.03);

    distanceSmoothed.setCurrentAndTargetValue (0.0f);
    widthSmoothed.setCurrentAndTargetValue (1.0f);
    mixSmoothed.setCurrentAndTargetValue (1.0f);
    outputSmoothed.setCurrentAndTargetValue (1.0f);
}

void PrimaTakePerspectiveAudioProcessor::updatePerspectiveFilters (float distance,
                                                                  float focus,
                                                                  float air)
{
    // distance: -1 = near, +1 = far
    const float farAmount = juce::jmax (0.0f, distance);
    const float nearAmount = juce::jmax (0.0f, -distance);

    // Farther = less top and a little more low-mid body.
    // Nearer = slightly leaner lows and more upper presence.
    const float lowDb = farAmount * 2.4f - nearAmount * 1.8f;
    const float highDb = (-farAmount * 6.0f)
                       + (nearAmount * 3.5f)
                       + air * 4.5f
                       + (focus - 0.5f) * 2.0f;

    *toneLow.state = *Coefficients::makeLowShelf (
        currentSampleRate,
        220.0,
        0.72f,
        juce::Decibels::decibelsToGain (lowDb));

    *toneHigh.state = *Coefficients::makeHighShelf (
        currentSampleRate,
        4200.0,
        0.72f,
        juce::Decibels::decibelsToGain (highDb));

    *monoLowFilter.state = *Coefficients::makeLowPass (
        currentSampleRate,
        160.0,
        0.707f);
}

void PrimaTakePerspectiveAudioProcessor::applyEarlyReflection (
    juce::AudioBuffer<float>& buffer,
    float amount,
    float distance)
{
    if (amount <= 0.0001f || delayBuffer.getNumSamples() <= 1)
        return;

    const int numSamples = buffer.getNumSamples();
    const int delaySize = delayBuffer.getNumSamples();

    const float farAmount = juce::jmax (0.0f, distance);
    const float delayMs = 7.0f + farAmount * 25.0f;
    const int delaySamples = juce::jlimit (
        1, delaySize - 2,
        static_cast<int> (currentSampleRate * delayMs / 1000.0));

    const float reflectionGain = amount * (0.10f + farAmount * 0.20f);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto* delay = delayBuffer.getWritePointer (ch);

        int writePos = delayWritePosition;

        for (int i = 0; i < numSamples; ++i)
        {
            int readPos = writePos - delaySamples;
            if (readPos < 0)
                readPos += delaySize;

            const float delayed = delay[readPos];
            const float input = data[i];

            delay[writePos] = input;
            data[i] = input + delayed * reflectionGain;

            if (++writePos >= delaySize)
                writePos = 0;
        }
    }

    delayWritePosition += numSamples;
    delayWritePosition %= delaySize;
}

void PrimaTakePerspectiveAudioProcessor::applyStereoWidth (
    juce::AudioBuffer<float>& buffer,
    float width)
{
    if (buffer.getNumChannels() < 2)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float mid = 0.5f * (left[i] + right[i]);
        const float side = 0.5f * (left[i] - right[i]) * width;

        left[i] = mid + side;
        right[i] = mid - side;
    }
}

void PrimaTakePerspectiveAudioProcessor::applyMonoLow (
    juce::AudioBuffer<float>& buffer,
    float amount)
{
    if (buffer.getNumChannels() < 2 || amount <= 0.0001f)
        return;

    juce::AudioBuffer<float> lowCopy;
    lowCopy.makeCopyOf (buffer, true);

    juce::dsp::AudioBlock<float> lowBlock (lowCopy);
    juce::dsp::ProcessContextReplacing<float> lowContext (lowBlock);
    monoLowFilter.process (lowContext);

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    const auto* lowL = lowCopy.getReadPointer (0);
    const auto* lowR = lowCopy.getReadPointer (1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float monoLow = 0.5f * (lowL[i] + lowR[i]);

        l[i] += (monoLow - lowL[i]) * amount;
        r[i] += (monoLow - lowR[i]) * amount;
    }
}

void PrimaTakePerspectiveAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    dryBuffer.setSize (buffer.getNumChannels(), numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    float inPeak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, numSamples));

    inputMeter.store (juce::jlimit (0.0f, 1.0f, inPeak));

    const float distance = apvts.getRawParameterValue ("distance")->load() / 100.0f;
    const float focus = apvts.getRawParameterValue ("focus")->load() / 100.0f;
    const float air = apvts.getRawParameterValue ("air")->load() / 100.0f;
    const float reflections = apvts.getRawParameterValue ("reflections")->load() / 100.0f;
    const float width = apvts.getRawParameterValue ("width")->load() / 100.0f;
    const float monoLow = apvts.getRawParameterValue ("monolow")->load() / 100.0f;
    const float mix = apvts.getRawParameterValue ("mix")->load() / 100.0f;
    const float outputDb = apvts.getRawParameterValue ("output")->load();

    distanceSmoothed.setTargetValue (distance);
    widthSmoothed.setTargetValue (width);
    mixSmoothed.setTargetValue (mix);
    outputSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputDb));

    const float d = distanceSmoothed.getNextValue();
    const float w = widthSmoothed.getNextValue();

    updatePerspectiveFilters (d, focus, air);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    toneLow.process (context);
    toneHigh.process (context);

    applyEarlyReflection (buffer, reflections, d);

    // Farther sources become slightly narrower; near can stay wide.
    const float distanceWidth = juce::jlimit (0.55f, 1.25f,
                                              w * (1.0f - juce::jmax (0.0f, d) * 0.28f));
    applyStereoWidth (buffer, distanceWidth);
    applyMonoLow (buffer, monoLow);

    for (int i = 0; i < numSamples; ++i)
    {
        const float wet = mixSmoothed.getNextValue();
        const float dry = 1.0f - wet;
        const float out = outputSmoothed.getNextValue();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float processed = buffer.getSample (ch, i);
            const float original = dryBuffer.getSample (ch, i);
            buffer.setSample (ch, i, (processed * wet + original * dry) * out);
        }
    }

    float outPeak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, 0, numSamples));

    outputMeter.store (juce::jlimit (0.0f, 1.0f, outPeak));
}

void PrimaTakePerspectiveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PrimaTakePerspectiveAudioProcessor::setStateInformation (
    const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessorEditor* PrimaTakePerspectiveAudioProcessor::createEditor()
{
    return new PrimaTakePerspectiveAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PrimaTakePerspectiveAudioProcessor();
}
