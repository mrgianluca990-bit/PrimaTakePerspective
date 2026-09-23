#include "PluginEditor.h"

PerspectiveLookAndFeel::PerspectiveLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff3f5f7));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff10151b));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void PerspectiveLookAndFeel::drawRotarySlider (
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> (
        static_cast<float> (x),
        static_cast<float> (y),
        static_cast<float> (width),
        static_cast<float> (height)).reduced (10.0f);

    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto dial = juce::Rectangle<float> (0, 0, size, size).withCentre (bounds.getCentre());

    const auto centre = dial.getCentre();
    const float angle = rotaryStartAngle
                      + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const float radius = dial.getWidth() * 0.5f;
    const float ringRadius = radius - 5.0f;
    const float ringWidth = juce::jmax (3.5f, size * 0.045f);

    juce::Colour accent (0xff7fe7ff);

    const auto name = slider.getName();
    if (name == "DISTANCE")
        accent = juce::Colour (0xffffb35c);
    else if (name == "FOCUS")
        accent = juce::Colour (0xffff7b66);
    else if (name == "AIR")
        accent = juce::Colour (0xff99efff);
    else if (name == "REFLECTIONS")
        accent = juce::Colour (0xffb69cff);
    else if (name == "WIDTH")
        accent = juce::Colour (0xff63d8ff);

    g.setColour (juce::Colour (0x77000000));
    g.fillEllipse (dial.translated (0.0f, 6.0f).expanded (3.0f));

    juce::Path bg;
    bg.addCentredArc (centre.x, centre.y,
                      ringRadius, ringRadius,
                      0.0f,
                      rotaryStartAngle,
                      rotaryEndAngle,
                      true);

    g.setColour (juce::Colour (0xff28313b));
    g.strokePath (bg, juce::PathStrokeType (
        ringWidth,
        juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));

    juce::Path active;
    active.addCentredArc (centre.x, centre.y,
                          ringRadius, ringRadius,
                          0.0f,
                          rotaryStartAngle,
                          angle,
                          true);

    g.setColour (accent);
    g.strokePath (active, juce::PathStrokeType (
        ringWidth,
        juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));

    auto knob = dial.reduced (ringWidth + 8.0f);

    juce::ColourGradient grad (
        juce::Colour (0xff3c4651),
        knob.getCentreX(), knob.getY(),
        juce::Colour (0xff11161c),
        knob.getCentreX(), knob.getBottom(),
        false);

    g.setGradientFill (grad);
    g.fillEllipse (knob);

    g.setColour (juce::Colour (0xff53606d));
    g.drawEllipse (knob, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f,
                                 -knob.getHeight() * 0.42f,
                                 3.0f,
                                 knob.getHeight() * 0.30f,
                                 1.5f);

    g.setColour (juce::Colour (0xfff6f7f8));
    g.fillPath (pointer,
                juce::AffineTransform::rotation (angle)
                    .translated (centre.x, centre.y));

    g.setColour (accent);
    g.fillEllipse (juce::Rectangle<float> (4, 4).withCentre (centre));
}

PrimaTakePerspectiveAudioProcessorEditor::PrimaTakePerspectiveAudioProcessorEditor (
    PrimaTakePerspectiveAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&look);

    setupKnob (distance,    "DISTANCE",    "near  ↔  far",        "distance");
    setupKnob (focus,       "FOCUS",       "forward clarity",     "focus");
    setupKnob (air,         "AIR",         "top perspective",     "air");
    setupKnob (reflections, "REFLECTIONS", "early environment",   "reflections");
    setupKnob (width,       "WIDTH",       "stereo scale",        "width");
    setupKnob (monoLow,     "MONO LOW",    "low-end anchor",      "monolow");
    setupKnob (mix,         "MIX",         "parallel amount",     "mix");
    setupKnob (output,      "OUTPUT",      "final trim",          "output");

    setResizable (true, true);
    setResizeLimits (860, 520, 1450, 900);
    setSize (1120, 640);

    startTimerHz (30);
}

PrimaTakePerspectiveAudioProcessorEditor::~PrimaTakePerspectiveAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PrimaTakePerspectiveAudioProcessorEditor::setupKnob (
    Knob& knob,
    const juce::String& name,
    const juce::String& hint,
    const juce::String& parameterID)
{
    knob.slider.setName (name);
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setRotaryParameters (
        juce::MathConstants<float>::pi * 1.22f,
        juce::MathConstants<float>::pi * 2.78f,
        true);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 25);
    addAndMakeVisible (knob.slider);

    knob.name.setText (name, juce::dontSendNotification);
    knob.name.setJustificationType (juce::Justification::centred);
    knob.name.setColour (juce::Label::textColourId, juce::Colour (0xfff4f6f8));
    knob.name.setFont (juce::Font (14.0f, juce::Font::bold));
    addAndMakeVisible (knob.name);

    knob.hint.setText (hint.toUpperCase(), juce::dontSendNotification);
    knob.hint.setJustificationType (juce::Justification::centred);
    knob.hint.setColour (juce::Label::textColourId, juce::Colour (0xff75808c));
    knob.hint.setFont (juce::Font (9.5f));
    addAndMakeVisible (knob.hint);

    knob.attachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.getAPVTS(), parameterID, knob.slider);
}

void PrimaTakePerspectiveAudioProcessorEditor::drawMeter (
    juce::Graphics& g,
    juce::Rectangle<float> area,
    float value,
    juce::Colour colour,
    const juce::String& label)
{
    value = juce::jlimit (0.0f, 1.0f, value);

    g.setColour (juce::Colour (0xff0d1217));
    g.fillRoundedRectangle (area, 4.0f);

    auto fill = area.reduced (3.0f);
    fill.setWidth (fill.getWidth() * value);

    g.setColour (colour);
    g.fillRoundedRectangle (fill, 3.0f);

    g.setColour (juce::Colour (0xff77828e));
    g.setFont (juce::Font (9.0f, juce::Font::bold));
    g.drawText (label,
                static_cast<int> (area.getX()),
                static_cast<int> (area.getY() - 14.0f),
                static_cast<int> (area.getWidth()),
                12,
                juce::Justification::centredLeft);
}

void PrimaTakePerspectiveAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient bg (
        juce::Colour (0xff17202a),
        bounds.getCentreX(), bounds.getY(),
        juce::Colour (0xff080c11),
        bounds.getCentreX(), bounds.getBottom(),
        false);

    g.setGradientFill (bg);
    g.fillAll();

    auto content = bounds.reduced (28.0f);

    g.setColour (juce::Colour (0xfff4f6f8));
    g.setFont (juce::Font (31.0f, juce::Font::bold));
    g.drawText ("PRIMA TAKE",
                content.getX(), content.getY(),
                260.0f, 38.0f,
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff7fe7ff));
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawText ("PERSPECTIVE",
                content.getX() + 2.0f,
                content.getY() + 42.0f,
                160.0f, 20.0f,
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff6f7a86));
    g.setFont (juce::Font (11.0f));
    g.drawText ("MOVE A SOURCE THROUGH DEPTH — WITHOUT REACHING FOR A REVERB FIRST",
                content.getX() + 165.0f,
                content.getY() + 42.0f,
                content.getWidth() - 330.0f,
                20.0f,
                juce::Justification::centredLeft);

    auto meterBox = juce::Rectangle<float> (
        content.getRight() - 240.0f,
        content.getY() + 6.0f,
        240.0f, 64.0f);

    drawMeter (g, meterBox.removeFromTop (13.0f),
               processor.getInputMeter(),
               juce::Colour (0xff7fe7ff),
               "INPUT");

    meterBox.removeFromTop (18.0f);

    drawMeter (g, meterBox.removeFromTop (13.0f),
               processor.getOutputMeter(),
               juce::Colour (0xffffb35c),
               "OUTPUT");

    content.removeFromTop (88.0f);

    const float rowGap = 14.0f;
    auto topRow = content.removeFromTop ((content.getHeight() - rowGap) * 0.52f);
    content.removeFromTop (rowGap);
    auto bottomRow = content;

    auto drawCards = [&g] (juce::Rectangle<float> row, int count)
    {
        const float gap = 12.0f;
        const float w = (row.getWidth() - gap * static_cast<float> (count - 1))
                      / static_cast<float> (count);

        for (int i = 0; i < count; ++i)
        {
            auto card = juce::Rectangle<float> (
                row.getX() + static_cast<float> (i) * (w + gap),
                row.getY(), w, row.getHeight());

            juce::ColourGradient cardGrad (
                juce::Colour (0xff18212a),
                card.getCentreX(), card.getY(),
                juce::Colour (0xff10161d),
                card.getCentreX(), card.getBottom(),
                false);

            g.setGradientFill (cardGrad);
            g.fillRoundedRectangle (card, 16.0f);

            g.setColour (juce::Colour (0xff2a3540));
            g.drawRoundedRectangle (card, 16.0f, 1.0f);
        }
    };

    drawCards (topRow, 4);
    drawCards (bottomRow, 4);

    g.setColour (juce::Colour (0xff4f5a66));
    g.setFont (juce::Font (9.0f, juce::Font::bold));
    g.drawText ("DEPTH  •  CLARITY  •  EARLY SPACE  •  WIDTH",
                28, getHeight() - 20, getWidth() - 56, 14,
                juce::Justification::centred);
}

void PrimaTakePerspectiveAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (28);
    area.removeFromTop (88);

    const int rowGap = 14;
    auto topRow = area.removeFromTop ((area.getHeight() - rowGap) / 2);
    area.removeFromTop (rowGap);
    auto bottomRow = area;

    auto layoutRow = [] (juce::Rectangle<int> row,
                         std::array<Knob*, 4> rowKnobs)
    {
        const int gap = 12;
        const int width = (row.getWidth() - gap * 3) / 4;

        for (auto* knob : rowKnobs)
        {
            auto cell = row.removeFromLeft (width).reduced (8, 8);

            knob->name.setBounds (cell.removeFromTop (25));
            knob->hint.setBounds (cell.removeFromTop (16));
            cell.removeFromTop (2);
            knob->slider.setBounds (cell);

            row.removeFromLeft (gap);
        }
    };

    layoutRow (topRow, { &distance, &focus, &air, &reflections });
    layoutRow (bottomRow, { &width, &monoLow, &mix, &output });
}

void PrimaTakePerspectiveAudioProcessorEditor::timerCallback()
{
    repaint();
}
