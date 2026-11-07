#pragma once
#include "PluginProcessor.h"

class ChaosLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChaosLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
};

// Waveform + lane playhead; click or drop a file to load.
class WaveView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    WaveView (ChaosTriggerProcessor& p, int lane) : proc (p), lane (lane) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& f, int, int) override;
    void refreshThumb();
    std::function<void()> onLoaded;
private:
    ChaosTriggerProcessor& proc;
    int lane;
    std::vector<float> peaks;
    const LaneSample* thumbFor = nullptr;
    std::unique_ptr<juce::FileChooser> chooser;
};

struct LaneStrip
{
    juce::Label name;
    juce::TextButton mono { "MONO" }, rev { "REVERSE" }, number;
    std::unique_ptr<WaveView> wave;
    juce::Slider a, d, s, r, pitch, pan, vol;
    juce::Slider dist, haas, crush, smash;
    juce::TextButton chorus { "CHORUS OFF" };
    juce::ToggleButton power { "POWER" };
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
};

class ChaosTriggerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChaosTriggerEditor (ChaosTriggerProcessor&);
    ~ChaosTriggerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void loadForLane (int lane);

    ChaosTriggerProcessor& proc;
    ChaosLookAndFeel lnf;

    juce::Slider pitch, freeRate, swing;
    juce::TextButton retrig { "RETRIG" }, sync { "SYNC" }, legato { "LEGATO" }, freeBtn { "FREE" };
    juce::Label dividerTitle, pickerTitle, pitchLabel, rateLabel, swingLabel;
    juce::OwnedArray<juce::TextButton> divBtns, feelBtns;
    juce::ComboBox pattern;
    juce::TextButton randoom { "RANDOOM" }, loadBtn { "LOAD SAMPLE" }, scrollBtn { "BANK SCROLL" };
    juce::Label nowPlaying;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton keyTrackBtn { "KEY TRACK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> keyTrackAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scrollAtt;
    juce::OwnedArray<juce::TextButton> loadLaneBtns;
    int loadTarget = 0;

    juce::Label masterTitle;
    juce::Slider mDist, mHaas, mCrush, mSmash, ringHz, cutoff, res;
    juce::TextButton ringBtn { "RING MOD" };
    juce::OwnedArray<juce::TextButton> fModeBtns;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ringAtt;

    std::array<LaneStrip, kNumLanes> lanes;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> patAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChaosTriggerEditor)
};
