#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class PerspectiveLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PerspectiveLookAndFeel();

    void drawRotarySlider (juce::Graphics&,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override;
};

class PrimaTakePerspectiveAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit PrimaTakePerspectiveAudioProcessorEditor (PrimaTakePerspectiveAudioProcessor&);
    ~PrimaTakePerspectiveAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PrimaTakePerspectiveAudioProcessor& processor;
    PerspectiveLookAndFeel look;

    struct Knob
    {
        juce::Slider slider;
        juce::Label name;
        juce::Label hint;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    Knob distance, focus, air, reflections, width, monoLow, mix, output;

    std::array<Knob*, 8> knobs {
        &distance, &focus, &air, &reflections,
        &width, &monoLow, &mix, &output
    };

    void setupKnob (Knob&,
                    const juce::String&,
                    const juce::String&,
                    const juce::String&);

    void timerCallback() override;
    void drawMeter (juce::Graphics&, juce::Rectangle<float>, float,
                    juce::Colour, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrimaTakePerspectiveAudioProcessorEditor)
};
