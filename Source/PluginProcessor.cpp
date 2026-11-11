#include "PluginProcessor.h"
#include "PluginEditor.h"
#if CHAOS_HAS_DEMO_SAMPLES
 #include "BinaryData.h"
#endif

const int kPatterns[24][4] = {
    {0,1,2,3},{0,1,3,2},{0,2,1,3},{0,2,3,1},{0,3,1,2},{0,3,2,1},
    {1,0,2,3},{1,0,3,2},{1,2,0,3},{1,2,3,0},{1,3,0,2},{1,3,2,0},
    {2,0,1,3},{2,0,3,1},{2,1,0,3},{2,1,3,0},{2,3,0,1},{2,3,1,0},
    {3,0,1,2},{3,0,2,1},{3,1,0,2},{3,1,2,0},{3,2,0,1},{3,2,1,0} };

juce::AudioProcessor::BusesProperties ChaosTriggerProcessor::makeBuses()
{
    auto b = juce::AudioProcessor::BusesProperties();
    for (int i = 0; i < kNumLanes; ++i)
        b = b.withOutput ("Lane " + juce::String (i + 1), juce::AudioChannelSet::stereo(), i == 0);
    return b;
}

ChaosTriggerProcessor::ChaosTriggerProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();
    for (int i = 0; i < kNumLanes; ++i) loadFactorySample (i);
    for (auto& p : playPos) p = 0.0f;
    for (auto& e : envLevel) e = 0.0f;
}

juce::AudioProcessorValueTreeState::ParameterLayout ChaosTriggerProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto addFx = [&l] (const String& pre, const String& n)
    {
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { pre + "dist", 1 }, n + "DIST", NormalisableRange<float> (0, 1), 0.0f));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { pre + "haas", 1 }, n + "HAAS (ms)", NormalisableRange<float> (0, 100, 0.1f), 0.0f));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { pre + "crush", 1 }, n + "CRUSH (bits)", NormalisableRange<float> (1, 24, 0.01f), 24.0f));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { pre + "smash", 1 }, n + "SMASH", NormalisableRange<float> (0, 1), 0.0f));
    };
    addFx ("m_", "MASTER ");
    {
        NormalisableRange<float> t (0.0f, 8.0f, 0.001f, 0.3f);
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { "m_a", 1 }, "MASTER ENV A (Attack)",  t, 0.001f));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { "m_d", 1 }, "MASTER ENV D (Decay)",   t, 0.1f));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { "m_s", 1 }, "MASTER ENV S (Sustain)", NormalisableRange<float> (0, 1), 1.0f));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { "m_r", 1 }, "MASTER ENV R (Release)", t, 0.05f));
    }
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "m_ring", 1 }, "MASTER RING MOD", false, AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return String (v ? "ON" : "OFF"); })));
    l.add (std::make_unique<AudioParameterFloat>(ParameterID { "m_ringHz", 1 }, "MASTER RING HZ",
                                                 NormalisableRange<float> (20.0f, 4000.0f, 0.1f, 0.3f), 440.0f));
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "m_fmode", 1 }, "MASTER FILTER",
                                                  StringArray { "OFF", "LP 24dB", "BP 6dB", "HP 12dB" }, 0));
    l.add (std::make_unique<AudioParameterFloat>(ParameterID { "m_cutoff", 1 }, "MASTER CUTOFF",
                                                 NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.25f), 20000.0f));
    l.add (std::make_unique<AudioParameterFloat>(ParameterID { "m_res", 1 }, "MASTER RES", NormalisableRange<float> (0, 1), 0.0f));

    l.add (std::make_unique<AudioParameterInt>  (ParameterID { "pitch", 1 }, "PITCH", -12, 12, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "legato", 1 }, "RETRIG / LEGATO", false, AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return String (v ? "LEGATO" : "RETRIG"); })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "free", 1 }, "SYNC / FREE", false, AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return String (v ? "FREE" : "SYNC"); })));
    l.add (std::make_unique<AudioParameterFloat>(ParameterID { "freeRate", 1 }, "FREE RATE (Hz)",
                                                 NormalisableRange<float> (0.5f, 40.0f, 0.01f, 0.4f), 8.0f));
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "division", 1 }, "CHAOS DIVIDER",
                                                  StringArray { "1/128","1/64","1/32","1/16","1/8","1/4","1/2","1/1" }, 3));
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "feel", 1 }, "CHAOS DIVIDER FEEL",
                                                  StringArray { "STRAIGHT","TRIPLET","DOTTED","SWING" }, 0));
    l.add (std::make_unique<AudioParameterFloat>(ParameterID { "swing", 1 }, "SWING AMOUNT",
                                                 NormalisableRange<float> (0.0f, 0.75f), 0.33f));
    StringArray pats;
    for (int i = 0; i < 24; ++i)
    {
        String s; for (int k = 0; k < 4; ++k) s << (kPatterns[i][k] + 1);
        pats.add (String (i + 1) + " - " + s);
    }
    pats.add ("66 - RANDOOM");
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "pattern", 1 }, "CHAOS PICKER", pats, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "keytrack", 1 }, "KEY TRACK", true,
        AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return String (v ? "ON" : "OFF"); })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "scroll", 1 }, "BANK SCROLL", false,
        AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return String (v ? "ON" : "OFF"); })));

    for (int i = 0; i < kNumLanes; ++i)
    {
        auto n = "LANE " + String (i + 1) + " ";
        NormalisableRange<float> t (0.0f, 4.0f, 0.001f, 0.3f);
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { laneId (i, "a"), 1 }, n + "A (Attack)",  t, 0.002f));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { laneId (i, "d"), 1 }, n + "D (Decay)",   t, 0.1f));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { laneId (i, "s"), 1 }, n + "S (Sustain)", NormalisableRange<float> (0, 1), 1.0f));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { laneId (i, "r"), 1 }, n + "R (Release)", t, 0.02f));
        l.add (std::make_unique<AudioParameterInt>  (ParameterID { laneId (i, "pitch"), 1 }, n + "PITCH", -12, 12, 0));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { laneId (i, "pan"), 1 }, n + "PAN", NormalisableRange<float> (-1, 1), 0.0f));
        l.add (std::make_unique<AudioParameterFloat>(ParameterID { laneId (i, "vol"), 1 }, n + "VOL (dB)",
                                                     NormalisableRange<float> (-60.0f, 6.0f, 0.1f, 2.0f), 0.0f));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { laneId (i, "power"), 1 }, n + "POWER", true, AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return String (v ? "ON" : "OFF"); })));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { laneId (i, "mono"), 1 }, n + "MONO", false, AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return String (v ? "ON" : "OFF"); })));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { laneId (i, "rev"), 1 }, n + "REVERSE", false, AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return String (v ? "ON" : "OFF"); })));
        addFx (laneId (i, ""), n);
        l.add (std::make_unique<AudioParameterChoice>(ParameterID { laneId (i, "chorus"), 1 }, n + "CHORUS",
                                                      StringArray { "OFF", "I", "II", "I+II" }, 0));
    }
    return l;
}

bool ChaosTriggerProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    for (auto& b : layout.outputBuses)
        if (! b.isDisabled() && b != juce::AudioChannelSet::stereo())
            return false;
    return layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ChaosTriggerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (int i = 0; i < kNumLanes; ++i)
    {
        env[(size_t) i].setSampleRate (sr);
        masterEnv.setSampleRate (sr);
        masterEnv.reset();
        env[(size_t) i].reset();
        gainL[(size_t) i].reset (sr, 0.02);
        gainR[(size_t) i].reset (sr, 0.02);
    }
    running = false;
    heldNotes = 0;
    laneBuf.setSize (kNumLanes * 2, juce::jmax (1, samplesPerBlock));
    for (auto& f : laneFx) f.prepare (sr, samplesPerBlock);
    for (auto& f : masterFx) f.prepare (sr, samplesPerBlock);
}

FxSettings ChaosTriggerProcessor::readFx (const juce::String& pre, bool master) const
{
    auto v = [this, &pre] (const char* id) { return apvts.getRawParameterValue (pre + id)->load(); };
    FxSettings s;
    s.dist = v ("dist"); s.haasMs = v ("haas"); s.bits = v ("crush"); s.smash = v ("smash");
    if (master)
    {
        s.ring = v ("ring") > 0.5f; s.ringHz = v ("ringHz");
        s.filterMode = (int) v ("fmode"); s.cutoff = v ("cutoff"); s.res = v ("res");
    }
    else s.chorus = (int) v ("chorus");
    return s;
}

bool ChaosTriggerProcessor::loadSample (int lane, const juce::File& file)
{
    return loadFromReader (lane, std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (file)),
                           file.getFileName(), file.getFullPathName());
}

#if CHAOS_HAS_DEMO_SAMPLES
// Built-in demo loops, compiled into the plugin (Resources/Samples).
static const struct { const char* data; int size; const char* name; } kFactory[kNumLanes] = {
    { BinaryData::Lane1_AU_HDT2_150_hihat_sparrow_wav,      BinaryData::Lane1_AU_HDT2_150_hihat_sparrow_wavSize,      "AU_HDT2_150_hihat_sparrow.wav" },
    { BinaryData::Lane2_DS_HT_150_drum_break_great_wav,     BinaryData::Lane2_DS_HT_150_drum_break_great_wavSize,     "DS_HT_150_drum_break_great.wav" },
    { BinaryData::Lane3_FL_BJ_174_Synth_Pad_Sonic_Gm_wav,   BinaryData::Lane3_FL_BJ_174_Synth_Pad_Sonic_Gm_wavSize,   "FL_BJ_174_Synth_Pad_Sonic_Gm.wav" },
    { BinaryData::Lane4_ZEN_STR_120_pad_synth_silvia_Gm_wav,BinaryData::Lane4_ZEN_STR_120_pad_synth_silvia_Gm_wavSize,"ZEN_STR_120_pad_synth_silvia_Gm.wav" } };

bool ChaosTriggerProcessor::loadFactorySample (int lane)
{
    const auto& f = kFactory[lane];
    auto stream = std::make_unique<juce::MemoryInputStream> (f.data, (size_t) f.size, false);
    return loadFromReader (lane, std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (std::move (stream))),
                           f.name, "factory:" + juce::String (lane));
}

#else
bool ChaosTriggerProcessor::loadFactorySample (int) { return false; }   // public edition: no demo loops
#endif

bool ChaosTriggerProcessor::loadFromReader (int lane, std::unique_ptr<juce::AudioFormatReader> reader,
                                            const juce::String& name, const juce::String& path)
{
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    auto s = std::make_shared<LaneSample>();
    const auto len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 600));
    s->buffer.setSize (2, len);
    reader->read (&s->buffer, 0, len, 0, true, true);
    if (reader->numChannels == 1)
        s->buffer.copyFrom (1, 0, s->buffer, 0, 0, len);
    s->sampleRate = reader->sampleRate;
    s->name = name;
    s->path = path;

    std::shared_ptr<const LaneSample> c = s;
    std::atomic_store (&samples[(size_t) lane], c);
    return true;
}

double ChaosTriggerProcessor::currentStepLengthSamples (int idx)
{
    const bool freeRun = apvts.getRawParameterValue ("free")->load() > 0.5f;
    double len;
    if (freeRun)
    {
        len = sr / (double) apvts.getRawParameterValue ("freeRate")->load();
    }
    else
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;
        static const double divBeats[] = { 4.0/128, 4.0/64, 4.0/32, 4.0/16, 4.0/8, 4.0/4, 4.0/2, 4.0 };
        const int d = (int) apvts.getRawParameterValue ("division")->load();
        len = divBeats[juce::jlimit (0, 7, d)] * 60.0 / bpm * sr;
    }

    switch ((int) apvts.getRawParameterValue ("feel")->load())
    {
        case 1: len *= 2.0 / 3.0; break;                       // triplet
        case 2: len *= 1.5; break;                             // dotted
        case 3: {                                              // swing: long-short pairs
            const double sw = apvts.getRawParameterValue ("swing")->load();
            len *= (idx % 2 == 0) ? (1.0 + sw) : (1.0 - sw);
            break; }
        default: break;
    }
    return juce::jmax (16.0, len);
}

void ChaosTriggerProcessor::advanceStep()
{
    // Lanes that can be played: powered and loaded.
    auto playable = [this] (int lane)
    {
        return apvts.getRawParameterValue (laneId (lane, "power"))->load() > 0.5f
            && std::atomic_load (&samples[(size_t) lane]) != nullptr;
    };

    const int userPat = (int) apvts.getRawParameterValue ("pattern")->load();
    const bool scroll = apvts.getRawParameterValue ("scroll")->load() > 0.5f;
    // BANK SCROLL: at the start of every cycle pick a new random pattern (1-24),
    // without touching the CHAOS PICKER setting.
    if (! scroll || userPat >= 24) scrollPattern = userPat;
    else if (stepIndex == 0) scrollPattern = rng.nextInt (24);
    const int pat = scrollPattern;
    playingPattern = pat;
    int next = -1;

    if (pat >= 24)
    {
        int cands[kNumLanes]; int n = 0;
        for (int i = 0; i < kNumLanes; ++i) if (playable (i)) cands[n++] = i;
        if (n > 0) next = cands[rng.nextInt (n)];
    }
    else
    {
        for (int tries = 0; tries < 4; ++tries)
        {
            const int lane = kPatterns[pat][stepIndex];
            stepIndex = (stepIndex + 1) % 4;
            if (playable (lane)) { next = lane; break; }   // skip off/empty lanes
        }
    }

    if (next != currentLane)
    {
        if (currentLane >= 0) env[(size_t) currentLane].noteOff();
    }
    if (next >= 0)
    {
        auto& e = env[(size_t) next];
        juce::ADSR::Parameters p;
        p.attack  = apvts.getRawParameterValue (laneId (next, "a"))->load();
        p.decay   = apvts.getRawParameterValue (laneId (next, "d"))->load();
        p.sustain = apvts.getRawParameterValue (laneId (next, "s"))->load();
        p.release = apvts.getRawParameterValue (laneId (next, "r"))->load();
        e.setParameters (p);
        e.noteOn();
    }
    currentLane = next;
    activeLane = next;
    samplesToNextStep += currentStepLengthSamples ((int) (stepCount++ % 2));
}

void ChaosTriggerProcessor::startNote (bool retrigger)
{
    if (retrigger || ! running)
        for (auto& p : lanePos) p = 0.0;
    for (auto& e : env) e.noteOff();
    currentLane = -1;
    stepIndex = 0;
    stepCount = 0;
    samplesToNextStep = 0.0;
    running = true;
}

void ChaosTriggerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    if (laneBuf.getNumSamples() < numSamples) laneBuf.setSize (kNumLanes * 2, numSamples, false, false, true);
    laneBuf.clear();

    // Snapshot samples and per-lane settings for this block
    std::array<std::shared_ptr<const LaneSample>, kNumLanes> smp;
    std::array<bool, kNumLanes> mono {}, rev {};
    for (int i = 0; i < kNumLanes; ++i)
    {
        smp[(size_t) i] = std::atomic_load (&samples[(size_t) i]);
        mono[(size_t) i] = apvts.getRawParameterValue (laneId (i, "mono"))->load() > 0.5f;
        rev[(size_t) i]  = apvts.getRawParameterValue (laneId (i, "rev"))->load() > 0.5f;
        const bool on = apvts.getRawParameterValue (laneId (i, "power"))->load() > 0.5f;
        const float g = on ? juce::Decibels::decibelsToGain (apvts.getRawParameterValue (laneId (i, "vol"))->load(), -59.9f) : 0.0f;
        const float pan = apvts.getRawParameterValue (laneId (i, "pan"))->load();
        const float ang = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        gainL[(size_t) i].setTargetValue (g * std::cos (ang) * juce::MathConstants<float>::sqrt2);
        gainR[(size_t) i].setTargetValue (g * std::sin (ang) * juce::MathConstants<float>::sqrt2);
    }

    std::array<double, kNumLanes> lanePitch {};
    for (int i = 0; i < kNumLanes; ++i)
        lanePitch[(size_t) i] = std::pow (2.0, apvts.getRawParameterValue (laneId (i, "pitch"))->load() / 12.0);
    const bool keyTrack = apvts.getRawParameterValue ("keytrack")->load() > 0.5f;
    const bool legato = apvts.getRawParameterValue ("legato")->load() > 0.5f;
    const double pitchRatio = std::pow (2.0, apvts.getRawParameterValue ("pitch")->load() / 12.0);

    // Destination bus per lane: its own bus if enabled, otherwise the main bus.
    std::array<float*, kNumLanes> outL {}, outR {};
    for (int i = 0; i < kNumLanes; ++i)
    {
        int bus = i;
        if (i >= getBusCount (false) || getBus (false, i) == nullptr || ! getBus (false, i)->isEnabled())
            bus = 0;
        auto b = getBusBuffer (buffer, false, bus);
        outL[(size_t) i] = b.getNumChannels() > 0 ? b.getWritePointer (0) : nullptr;
        outR[(size_t) i] = b.getNumChannels() > 1 ? b.getWritePointer (1) : outL[(size_t) i];
    }

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);   // on-screen / computer keyboard
    auto midiIt = midi.cbegin();
    for (int n = 0; n < numSamples; ++n)
    {
        for (; midiIt != midi.cend() && (*midiIt).samplePosition <= n; ++midiIt)
        {
            const auto m = (*midiIt).getMessage();
            if (m.isNoteOn())
            {
                currentNote = m.getNoteNumber();
                const bool first = heldNotes == 0;
                ++heldNotes;
                if (first || ! legato)
                {
                    startNote (! legato || ! running);
                    juce::ADSR::Parameters mp;
                    mp.attack  = apvts.getRawParameterValue ("m_a")->load();
                    mp.decay   = apvts.getRawParameterValue ("m_d")->load();
                    mp.sustain = apvts.getRawParameterValue ("m_s")->load();
                    mp.release = apvts.getRawParameterValue ("m_r")->load();
                    masterEnv.setParameters (mp);
                    masterEnv.noteOn();
                }
                else
                {   // legato: restart step sequencer only
                    stepIndex = 0; stepCount = 0; samplesToNextStep = 0.0;
                }
            }
            else if (m.isNoteOff())
            {
                heldNotes = juce::jmax (0, heldNotes - 1);
                if (heldNotes == 0)
                    masterEnv.noteOff();   // the sequence keeps running through the master release
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                heldNotes = 0;
                for (auto& e : env) e.reset();
                masterEnv.reset();
                running = false; currentLane = -1; activeLane = -1;
            }
        }

        const double noteRatio = keyTrack ? std::pow (2.0, (currentNote - 60) / 12.0) : 1.0;
        if (! running) continue;

        if (heldNotes > 0 || masterEnv.isActive())
        {
            samplesToNextStep -= 1.0;
            if (samplesToNextStep <= 0.0)
                advanceStep();
        }

        bool anyActive = heldNotes > 0 || masterEnv.isActive();
        for (int i = 0; i < kNumLanes; ++i)
        {
            const auto& s = smp[(size_t) i];
            auto& e = env[(size_t) i];
            if (s == nullptr) continue;

            const int len = s->buffer.getNumSamples();
            // Playhead always moves at original speed: pitch never changes the loop's timing.
            const double inc = s->sampleRate / sr;
            const double ratio = pitchRatio * lanePitch[(size_t) i] * noteRatio;
            double& pos = lanePos[(size_t) i];

            if (e.isActive())
            {
                anyActive = true;
                const float a = e.getNextSample();
                const float* L = s->buffer.getReadPointer (0);
                const float* R = s->buffer.getReadPointer (1);
                const bool rv = rev[(size_t) i];
                auto readAt = [&] (double p, float& outL, float& outR)
                {
                    p = std::fmod (p, (double) len); if (p < 0) p += len;
                    double rp = rv ? (len - 1 - p) : p;
                    rp = juce::jlimit (0.0, (double) (len - 1), rp);
                    const int i0 = (int) rp;
                    const int i1 = (i0 + 1) % len;
                    const float fr = (float) (rp - i0);
                    outL = L[i0] + fr * (L[i1] - L[i0]);
                    outR = R[i0] + fr * (R[i1] - R[i0]);
                };
                float l = 0, r = 0;
                auto& gs = grains[(size_t) i];
                if (std::abs (ratio - 1.0) < 1.0e-4)
                {
                    readAt (pos, l, r);
                }
                else
                {
                    // Granular pitch shift: two overlapping Hann grains read at 'ratio' speed,
                    // each re-anchored to the playhead when it restarts.
                    const double G = grainLenSec * s->sampleRate;
                    for (int k = 0; k < 2; ++k)
                    {
                        double ph = gs.phase + 0.5 * k; if (ph >= 1.0) ph -= 1.0;
                        if (ph < gs.lastPh[k]) gs.anchor[k] = pos;      // grain restarts at the playhead
                        gs.lastPh[k] = ph;
                        float gl, gr;
                        readAt (gs.anchor[k] + ph * G * ratio, gl, gr);  // ...and reads at the shifted speed
                        const float w = (float) (std::sin (juce::MathConstants<double>::pi * ph));
                        l += gl * w * w; r += gr * w * w;
                    }
                }
                if (mono[(size_t) i]) l = r = 0.5f * (l + r);
                laneBuf.setSample (i * 2, n, l * a);
                laneBuf.setSample (i * 2 + 1, n, r * a);
            }
            // All lanes run in parallel so every step lands at the matching spot in its loop.
            pos += inc;
            grains[(size_t) i].phase += inc / (grainLenSec * s->sampleRate);
            if (grains[(size_t) i].phase >= 1.0) grains[(size_t) i].phase -= 1.0;
            if (pos >= len) { pos = std::fmod (pos, (double) len); for (auto& a : grains[(size_t) i].anchor) a -= len; }
        }
        // MASTER ENVELOPE: direct amplitude control from the key press, on top of the lane gates.
        const float mg = masterEnv.isActive() ? masterEnv.getNextSample() : 0.0f;
        for (int ch = 0; ch < kNumLanes * 2; ++ch)
            laneBuf.getWritePointer (ch)[n] *= mg;
        if (heldNotes == 0 && ! masterEnv.isActive())
        {
            for (auto& e : env) e.reset();
            currentLane = -1; activeLane = -1;
            anyActive = false;
        }
        if (! anyActive) running = false;
    }

    // Lane FX -> pan/volume -> lane output bus
    std::array<bool, kNumLanes> busUsed {};
    for (int i = 0; i < kNumLanes; ++i)
    {
        float* L = laneBuf.getWritePointer (i * 2);
        float* R = laneBuf.getWritePointer (i * 2 + 1);
        laneFx[(size_t) i].process (L, R, numSamples, readFx (laneId (i, ""), false));
        for (int n = 0; n < numSamples; ++n)
        {
            const float gl = gainL[(size_t) i].getNextValue(), gr = gainR[(size_t) i].getNextValue();
            if (outL[(size_t) i]) outL[(size_t) i][n] += L[n] * gl;
            if (outR[(size_t) i] && outR[(size_t) i] != outL[(size_t) i]) outR[(size_t) i][n] += R[n] * gr;
        }
    }
    // Master FX on every active output bus (same settings on each)
    const auto mfx = readFx ("m_", true);
    for (int b = 0; b < juce::jmin (kNumLanes, getBusCount (false)); ++b)
    {
        if (getBus (false, b) == nullptr || ! getBus (false, b)->isEnabled()) continue;
        auto bb = getBusBuffer (buffer, false, b);
        if (bb.getNumChannels() >= 2)
            masterFx[(size_t) b].process (bb.getWritePointer (0), bb.getWritePointer (1), numSamples, mfx);
    }
    juce::ignoreUnused (busUsed);

    for (int i = 0; i < kNumLanes; ++i)
    {
        const auto& s = smp[(size_t) i];
        playPos[(size_t) i] = (s != nullptr && s->buffer.getNumSamples() > 0)
            ? (float) ((rev[(size_t) i] ? s->buffer.getNumSamples() - lanePos[(size_t) i] : lanePos[(size_t) i]) / s->buffer.getNumSamples())
            : 0.0f;
        envLevel[(size_t) i] = env[(size_t) i].isActive() ? 1.0f : 0.0f;
    }
}

void ChaosTriggerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    for (int i = 0; i < kNumLanes; ++i)
        if (auto s = getSample (i))
            state.setProperty ("sample" + juce::String (i), s->path, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void ChaosTriggerProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        apvts.replaceState (tree);
        for (int i = 0; i < kNumLanes; ++i)
        {
            const auto path = tree.getProperty ("sample" + juce::String (i)).toString();
            if (path.startsWith ("factory:"))
                loadFactorySample (juce::jlimit (0, kNumLanes - 1, path.substring (8).getIntValue()));
            else if (juce::File (path).existsAsFile())
                loadSample (i, juce::File (path));
        }
    }
}

juce::AudioProcessorEditor* ChaosTriggerProcessor::createEditor() { return new ChaosTriggerEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChaosTriggerProcessor(); }
