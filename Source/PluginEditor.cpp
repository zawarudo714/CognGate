/*
  ==============================================================================
    CognGate – Cognitive Brain Training Audio Gate VST3 Plugin
    PluginEditor.cpp

    Dark-themed editor: three parameter sliders, real-time state indicator,
    and a horizontal gain bar that pulses with the gate cycle.
  ==============================================================================
*/

#include "PluginEditor.h"

//==============================================================================
//  DarkLookAndFeel
//==============================================================================
CognGateAudioProcessorEditor::DarkLookAndFeel::DarkLookAndFeel()
{
    // Overall colour scheme
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff1a1a2e));

    // Slider colours
    setColour (juce::Slider::backgroundColourId,          juce::Colour (0xff16213e));
    setColour (juce::Slider::trackColourId,               juce::Colour (0xff0f3460));
    setColour (juce::Slider::thumbColourId,               juce::Colour (0xff00d2ff));
    setColour (juce::Slider::textBoxTextColourId,          juce::Colours::white);
    setColour (juce::Slider::textBoxBackgroundColourId,    juce::Colour (0xff0d1b2a));
    setColour (juce::Slider::textBoxOutlineColourId,       juce::Colour (0xff0f3460));

    // Label colours
    setColour (juce::Label::textColourId, juce::Colour (0xffe0e0e0));
}

//==============================================================================
//  Editor Constructor
//==============================================================================
CognGateAudioProcessorEditor::CognGateAudioProcessorEditor (CognGateAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&darkLnf);
    setSize (480, 380);

    // ── Silence Duration ────────────────────────────────────────────────────
    setupSlider (silenceSlider, silenceLabel, silenceValueLabel, "Silence Duration", " sec");
    silenceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "silenceDuration", silenceSlider);

    // ── Max Leeway Wait ─────────────────────────────────────────────────────
    setupSlider (leewaySlider, leewayLabel, leewayValueLabel, "Max Leeway Wait", " sec");
    leewayAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "maxLeeway", leewaySlider);

    // ── Smoothness ──────────────────────────────────────────────────────────
    setupSlider (smoothnessSlider, smoothnessLabel, smoothnessValueLabel, "Smoothness", " ms");
    smoothnessAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "smoothness", smoothnessSlider);

    // ── State indicator labels ──────────────────────────────────────────────
    stateLabel.setJustificationType (juce::Justification::centred);
    stateLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    stateLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00d2ff));
    addAndMakeVisible (stateLabel);

    gainLabel.setJustificationType (juce::Justification::centredLeft);
    gainLabel.setFont (juce::Font (13.0f));
    gainLabel.setColour (juce::Label::textColourId, juce::Colour (0xffaaaaaa));
    addAndMakeVisible (gainLabel);

    // 30 Hz UI refresh for the state indicator
    startTimerHz (30);
}

CognGateAudioProcessorEditor::~CognGateAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void CognGateAudioProcessorEditor::setupSlider (juce::Slider& slider,
                                                  juce::Label& label,
                                                  juce::Label& valueLabel,
                                                  const juce::String& labelText,
                                                  const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 26);
    slider.setTextValueSuffix (suffix);
    addAndMakeVisible (slider);

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (juce::Font (14.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
    addAndMakeVisible (label);

    valueLabel.setJustificationType (juce::Justification::centredRight);
    valueLabel.setFont (juce::Font (12.0f));
    valueLabel.setColour (juce::Label::textColourId, juce::Colour (0xff888888));
    addAndMakeVisible (valueLabel);
}

//==============================================================================
void CognGateAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background gradient
    auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d1b2a), 0.0f, 0.0f,
                                              juce::Colour (0xff1a1a2e), 0.0f, bounds.getHeight(),
                                              false));
    g.fillRect (bounds);

    // Title
    g.setColour (juce::Colour (0xff00d2ff));
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText ("CognGate", getLocalBounds().removeFromTop (44), juce::Justification::centred);

    // Subtitle
    g.setColour (juce::Colour (0xff667788));
    g.setFont (juce::Font (11.0f));
    g.drawText ("Cognitive Brain Training Gate", getLocalBounds().removeFromTop (62).removeFromBottom (18),
                juce::Justification::centred);

    // ── Gain bar ────────────────────────────────────────────────────────────
    const float gain = processorRef.getCurrentGainValue();
    auto barArea = getLocalBounds().removeFromBottom (44).reduced (20, 10).toFloat();

    // Track background
    g.setColour (juce::Colour (0xff16213e));
    g.fillRoundedRectangle (barArea, 4.0f);

    // Filled portion
    auto fillArea = barArea.withWidth (barArea.getWidth() * gain);
    auto barColour = (processorRef.getCurrentGateState() == CognGateAudioProcessor::GateState::AUDIO)
                         ? juce::Colour (0xff00d2ff)
                         : juce::Colour (0xffff4466);
    g.setColour (barColour.withAlpha (0.85f));
    g.fillRoundedRectangle (fillArea, 4.0f);

    // Border
    g.setColour (juce::Colour (0xff0f3460));
    g.drawRoundedRectangle (barArea, 4.0f, 1.0f);
}

//==============================================================================
void CognGateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (66); // space for title + subtitle

    const int rowH       = 56;
    const int labelH     = 18;
    const int sliderH    = 30;
    const int labelWidth = 160;

    auto layoutRow = [&](juce::Slider& slider, juce::Label& label, juce::Label& /*valLabel*/)
    {
        auto row = area.removeFromTop (rowH);
        auto top = row.removeFromTop (labelH);
        label.setBounds (top.removeFromLeft (labelWidth));
        slider.setBounds (row.removeFromTop (sliderH));
    };

    layoutRow (silenceSlider, silenceLabel, silenceValueLabel);
    layoutRow (leewaySlider, leewayLabel, leewayValueLabel);
    layoutRow (smoothnessSlider, smoothnessLabel, smoothnessValueLabel);

    area.removeFromTop (8);
    stateLabel.setBounds (area.removeFromTop (28));
    gainLabel.setBounds  (area.removeFromTop (18));

    // The bottom 44 px is reserved for the painted gain bar
}

//==============================================================================
void CognGateAudioProcessorEditor::timerCallback()
{
    // Update state indicator text
    auto state = processorRef.getCurrentGateState();
    float gain = processorRef.getCurrentGainValue();

    if (state == CognGateAudioProcessor::GateState::AUDIO)
        stateLabel.setText (juce::String::fromUTF8 ("\xe2\x96\xb6  AUDIO PLAYING"), juce::dontSendNotification);
    else
        stateLabel.setText (juce::String::fromUTF8 ("\xe2\x9c\x96  SILENCE"), juce::dontSendNotification);

    stateLabel.setColour (juce::Label::textColourId,
                          state == CognGateAudioProcessor::GateState::AUDIO
                              ? juce::Colour (0xff00d2ff)
                              : juce::Colour (0xffff4466));

    gainLabel.setText ("Gain: " + juce::String (gain, 3), juce::dontSendNotification);

    repaint();
}
