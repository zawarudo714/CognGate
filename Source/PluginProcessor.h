/*
  ==============================================================================
    CognGate – Cognitive Brain Training Audio Gate VST3 Plugin
    PluginProcessor.h

    Designed for system-wide use via Equalizer APO on Windows.
    Uses a fully autonomous, sample-counting state machine that requires
    NO host transport, tempo clock, or DAW timeline.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <random>

//==============================================================================
class CognGateAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    CognGateAudioProcessor();
    ~CognGateAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Public accessor for the APVTS so the Editor can bind to it
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Read-only state for the Editor's visual feedback
    enum class GateState { AUDIO, SILENCE };
    GateState getCurrentGateState() const { return currentState.load(); }
    float getCurrentGainValue() const { return currentGain.load(); }

private:
    //==============================================================================
    // Parameter IDs
    static constexpr const char* PARAM_SILENCE_DURATION = "silenceDuration";
    static constexpr const char* PARAM_MAX_LEEWAY       = "maxLeeway";
    static constexpr const char* PARAM_SMOOTHNESS        = "smoothness";

    // APVTS setup
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Atomic parameter cache (updated each processBlock from APVTS)
    std::atomic<float> paramSilenceDuration { 2.0f };
    std::atomic<float> paramMaxLeeway       { 5.0f };
    std::atomic<float> paramSmoothness      { 10.0f };

    //==============================================================================
    // DSP State Machine
    std::atomic<GateState> currentState { GateState::AUDIO };

    // The gain multiplier applied sample-by-sample (atomic for editor reads)
    std::atomic<float> currentGain { 1.0f };

    // Internal non-atomic working copy used exclusively inside processBlock
    float workingGain = 1.0f;

    // Sample counter: counts DOWN to zero, then triggers state transition
    int64_t samplesRemaining = 0;

    // The current target gain for the active state (1.0 for AUDIO, 0.0 for SILENCE)
    float targetGain = 1.0f;

    // Per-sample gain increment magnitude, recalculated when smoothness changes
    float gainStepPerSample = 0.0f;

    // Cached sample rate
    double currentSampleRate = 44100.0;

    //==============================================================================
    // Random number generation – seeded from hardware entropy
    std::mt19937 rng;
    std::uniform_real_distribution<float> leewayDist;

    // Roll a new random leeway duration in samples
    int64_t rollNewLeewayInSamples();

    // Recalculate the gain ramp step from the smoothness parameter
    void recalcGainStep();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CognGateAudioProcessor)
};
