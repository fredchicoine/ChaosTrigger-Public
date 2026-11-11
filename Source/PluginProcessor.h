#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Fx.h"
#include <array>
#include <atomic>
#include <memory>

static constexpr int kNumLanes = 4;
static constexpr int kNumPatterns = 25;   // 24 orderings + "66" RANDOOM

// The 24 fixed lane orders (0-based lane indices) shown in the Chaos Picker.
extern const int kPatterns[24][4];

struct LaneSample
{
    juce::AudioBuffer<float> buffer;   // always 2 channels
    double sampleRate = 44100.0;
    juce::String name;
    juce::String path;
};

class ChaosTriggerProcessor : public juce::AudioProcessor
{
public:
    ChaosTriggerProcessor();
    ~ChaosTriggerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chaos Trigger"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Message thread only.
    bool loadSample (int lane, const juce::File& file);
    bool loadFactorySample (int lane);
    std::atomic<int> playingPattern { 0 };
    juce::MidiKeyboardState keyboardState;   // on-screen keyboard + computer keys   // pattern actually playing (differs from CHAOS PICKER in BANK SCROLL)
    std::shared_ptr<const LaneSample> getSample (int lane) const { return std::atomic_load (&samples[(size_t) lane]); }

    juce::AudioProcessorValueTreeState apvts;

    // GUI feedback
    std::atomic<int> activeLane { -1 };
    std::array<std::atomic<float>, kNumLanes> playPos {};     // 0..1
    std::array<std::atomic<float>, kNumLanes> envLevel {};

    static juce::String laneId (int lane, const char* p) { return "l" + juce::String (lane + 1) + "_" + p; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties makeBuses();
    FxSettings readFx (const juce::String& prefix, bool master) const;
    double currentStepLengthSamples (int stepIndex);
    void advanceStep();
    void startNote (bool retrigger);
    bool loadFromReader (int lane, std::unique_ptr<juce::AudioFormatReader>, const juce::String& name, const juce::String& path);
    int scrollPattern = 0;
    int currentNote = 60;                    // C3 = original pitch
    static constexpr double grainLenSec = 0.06;
    struct GrainState { double phase = 0; double anchor[2] {}; double lastPh[2] { 1, 1 }; };
    std::array<GrainState, kNumLanes> grains {};

    juce::AudioFormatManager formatManager;
    std::array<std::shared_ptr<const LaneSample>, kNumLanes> samples;

    double sr = 44100.0;
    std::array<juce::ADSR, kNumLanes> env;
    juce::ADSR masterEnv;
    std::array<double, kNumLanes> lanePos {};      // read position in source samples
    std::array<juce::SmoothedValue<float>, kNumLanes> gainL, gainR;

    int heldNotes = 0;
    bool running = false;
    int stepIndex = 0;          // position inside the pattern (0..3)
    long stepCount = 0;         // total steps since note on (for swing)
    double samplesToNextStep = 0.0;
    int currentLane = -1;
    juce::Random rng;
    juce::AudioBuffer<float> laneBuf;           // 2 channels per lane
    std::array<FxChain, kNumLanes> laneFx, masterFx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChaosTriggerProcessor)
};
