/*
  ==============================================================================
    CognGate – Cognitive Brain Training Audio Gate VST3 Plugin
    PluginEditor.h

    Dark-themed editor with three parameter sliders and a real-time
    state / gain indicator.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class CognGateAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit CognGateAudioProcessorEditor (CognGateAudioProcessor&);
    ~CognGateAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    //==============================================================================
    CognGateAudioProcessor& processorRef;

    // Custom LookAndFeel for the dark theme
    class DarkLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        DarkLookAndFeel();
    };

    DarkLookAndFeel darkLnf;

    // ── Sliders ─────────────────────────────────────────────────────────────
    juce::Slider silenceSlider;
    juce::Slider leewaySlider;
    juce::Slider smoothnessSlider;

    juce::Label silenceLabel;
    juce::Label leewayLabel;
    juce::Label smoothnessLabel;

    // Value labels showing current number
    juce::Label silenceValueLabel;
    juce::Label leewayValueLabel;
    juce::Label smoothnessValueLabel;

    // ── APVTS attachments ───────────────────────────────────────────────────
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> silenceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> leewayAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smoothnessAttach;

    // ── State display ───────────────────────────────────────────────────────
    juce::Label stateLabel;
    juce::Label gainLabel;

    // Helper to configure a slider row
    void setupSlider (juce::Slider& slider, juce::Label& label, juce::Label& valueLabel,
                      const juce::String& labelText, const juce::String& suffix);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CognGateAudioProcessorEditor)
};
