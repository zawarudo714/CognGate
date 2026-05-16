/*
  ==============================================================================
    CognGate – Cognitive Brain Training Audio Gate VST3 Plugin
    PluginProcessor.cpp

    Core DSP: A two-state (AUDIO / SILENCE) autonomous gate that cycles
    indefinitely using only an internal sample counter. No host transport
    dependency whatsoever.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CognGateAudioProcessor::CognGateAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#else
    :
#endif
      apvts (*this, nullptr, "CognGateParams", createParameterLayout()),
      rng (std::random_device{}()),
      leewayDist (0.1f, 5.0f)   // default upper bound; updated each cycle
{
}

CognGateAudioProcessor::~CognGateAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
CognGateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { PARAM_SILENCE_DURATION, 1 },
        "Silence Duration",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f),
        2.0f,
        juce::AudioParameterFloatAttributes().withLabel ("sec")));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { PARAM_MAX_LEEWAY, 1 },
        "Max Leeway Wait",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f),
        5.0f,
        juce::AudioParameterFloatAttributes().withLabel ("sec")));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { PARAM_SMOOTHNESS, 1 },
        "Smoothness",
        juce::NormalisableRange<float> (1.0f, 100.0f, 0.1f, 0.5f),
        10.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    return { params.begin(), params.end() };
}

//==============================================================================
void CognGateAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Seed with fresh entropy every time playback is prepared
    rng.seed (std::random_device{}());

    // Start in AUDIO state with a freshly rolled leeway
    currentState.store (GateState::AUDIO);
    targetGain   = 1.0f;
    workingGain  = 1.0f;
    currentGain.store (1.0f);

    // Read initial parameter values
    paramSilenceDuration.store (apvts.getRawParameterValue (PARAM_SILENCE_DURATION)->load());
    paramMaxLeeway.store       (apvts.getRawParameterValue (PARAM_MAX_LEEWAY)->load());
    paramSmoothness.store      (apvts.getRawParameterValue (PARAM_SMOOTHNESS)->load());

    recalcGainStep();

    samplesRemaining = rollNewLeewayInSamples();
}

void CognGateAudioProcessor::releaseResources()
{
    // Nothing to free
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CognGateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

//==============================================================================
//  CORE DSP – processBlock
//  This is the autonomous, sample-counting state machine.
//  It does NOT call getPlayHead() or reference any host transport at all.
//==============================================================================
void CognGateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples             = buffer.getNumSamples();

    // Clear any output channels that don't have corresponding inputs
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    // ── Snapshot user parameters (lock-free reads) ──────────────────────────
    const float silenceDur = apvts.getRawParameterValue (PARAM_SILENCE_DURATION)->load();
    const float maxLeeway  = apvts.getRawParameterValue (PARAM_MAX_LEEWAY)->load();
    const float smoothMs   = apvts.getRawParameterValue (PARAM_SMOOTHNESS)->load();

    paramSilenceDuration.store (silenceDur);
    paramMaxLeeway.store       (maxLeeway);
    paramSmoothness.store      (smoothMs);

    // Recalculate the per-sample gain ramp step
    const float smoothSec = smoothMs * 0.001f;
    const float smoothSamples = static_cast<float>(currentSampleRate) * smoothSec;
    gainStepPerSample = (smoothSamples > 0.0f) ? (1.0f / smoothSamples) : 1.0f;

    // ── Per-sample processing loop ──────────────────────────────────────────
    for (int i = 0; i < numSamples; ++i)
    {
        // ── State transition check ──────────────────────────────────────────
        if (samplesRemaining <= 0)
        {
            if (currentState.load() == GateState::AUDIO)
            {
                // Transition: AUDIO → SILENCE
                currentState.store (GateState::SILENCE);
                targetGain = 0.0f;
                samplesRemaining = static_cast<int64_t>(silenceDur * currentSampleRate);
            }
            else
            {
                // Transition: SILENCE → AUDIO
                currentState.store (GateState::AUDIO);
                targetGain = 1.0f;

                // Update the leeway distribution upper bound to match the knob
                const float upperBound = std::max (0.1f, maxLeeway);
                leewayDist = std::uniform_real_distribution<float>(0.1f, upperBound);

                samplesRemaining = rollNewLeewayInSamples();
            }
        }

        // ── Linear gain ramp toward targetGain ──────────────────────────────
        if (workingGain < targetGain)
        {
            workingGain += gainStepPerSample;
            if (workingGain > targetGain)
                workingGain = targetGain;
        }
        else if (workingGain > targetGain)
        {
            workingGain -= gainStepPerSample;
            if (workingGain < targetGain)
                workingGain = targetGain;
        }

        // ── Apply gain to every channel ─────────────────────────────────────
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            buffer.getWritePointer (ch)[i] *= workingGain;
        }

        // ── Decrement counter ───────────────────────────────────────────────
        --samplesRemaining;
    }

    // Publish current gain for the editor's meter
    currentGain.store (workingGain);
}

//==============================================================================
int64_t CognGateAudioProcessor::rollNewLeewayInSamples()
{
    const float randomSeconds = leewayDist (rng);
    return static_cast<int64_t>(randomSeconds * currentSampleRate);
}

void CognGateAudioProcessor::recalcGainStep()
{
    const float smoothMs  = paramSmoothness.load();
    const float smoothSec = smoothMs * 0.001f;
    const float smoothSamples = static_cast<float>(currentSampleRate) * smoothSec;
    gainStepPerSample = (smoothSamples > 0.0f) ? (1.0f / smoothSamples) : 1.0f;
}

//==============================================================================
juce::AudioProcessorEditor* CognGateAudioProcessor::createEditor()
{
    return new CognGateAudioProcessorEditor (*this);
}

bool CognGateAudioProcessor::hasEditor() const { return true; }

//==============================================================================
const juce::String CognGateAudioProcessor::getName() const { return JucePlugin_Name; }
bool CognGateAudioProcessor::acceptsMidi()  const { return false; }
bool CognGateAudioProcessor::producesMidi() const { return false; }
bool CognGateAudioProcessor::isMidiEffect() const { return false; }
double CognGateAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int CognGateAudioProcessor::getNumPrograms()                          { return 1; }
int CognGateAudioProcessor::getCurrentProgram()                       { return 0; }
void CognGateAudioProcessor::setCurrentProgram (int)                  {}
const juce::String CognGateAudioProcessor::getProgramName (int)       { return {}; }
void CognGateAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void CognGateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CognGateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CognGateAudioProcessor();
}
