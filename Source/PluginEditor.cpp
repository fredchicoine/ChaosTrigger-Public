#include "PluginEditor.h"

namespace col
{
    const juce::Colour bg     { 0xff0b0505 };
    const juce::Colour blood  { 0xff7a0d0d };
    const juce::Colour red    { 0xffe0202a };
    const juce::Colour dimRed { 0xff3a0c0c };
    const juce::Colour text   { 0xffeeeeee };
    const juce::Colour gold   { 0xffc9a227 };
    const juce::Colour blue   { 0xff2a5bd8 };
}

//==============================================================================
ChaosLookAndFeel::ChaosLookAndFeel()
{
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff160606));
    setColour (juce::ComboBox::outlineColourId, col::red);
    setColour (juce::ComboBox::textColourId, col::red);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff160606));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::blood);
    setColour (juce::ToggleButton::textColourId, col::text);
    setColour (juce::ToggleButton::tickColourId, col::red);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void ChaosLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float start, float end, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4);
    const float rad = juce::jmin (b.getWidth(), b.getHeight()) / 2.0f;
    const auto c = b.getCentre();
    const float ang = start + pos * (end - start);
    const auto base = s.getProperties().getWithDefault ("knobColour", (int) col::red.getARGB());
    const juce::Colour kc ((juce::uint32) (int) base);

    // value arc
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, rad, rad, 0, start, ang, true);
    g.setColour (kc.withAlpha (0.8f));
    g.strokePath (arc, juce::PathStrokeType (2.5f));

    // ring body
    const float body = rad * 0.72f;
    g.setGradientFill (juce::ColourGradient (kc.brighter (0.4f), c.x - body, c.y - body,
                                             kc.darker (1.2f), c.x + body, c.y + body, true));
    g.fillEllipse (c.x - body, c.y - body, body * 2, body * 2);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - body, c.y - body, body * 2, body * 2, 1.5f);

    // blade pointer
    juce::Path blade;
    blade.addTriangle (-rad * 0.18f, rad * 0.35f, rad * 0.18f, rad * 0.35f, 0.0f, -rad * 1.0f);
    g.setColour (juce::Colours::black);
    g.fillPath (blade, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    g.setColour (kc.brighter (0.6f));
    g.strokePath (blade, juce::PathStrokeType (1.0f), juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void ChaosLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const auto accent = juce::Colour ((juce::uint32) (int) b.getProperties().getWithDefault ("accent", (int) col::red.getARGB()));
    const bool on = b.getToggleState();
    g.setColour (on ? accent.withAlpha (0.35f) : juce::Colour (0xff141414));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? accent : (over ? accent.withAlpha (0.6f) : juce::Colour (0xff4a4a4a)));
    g.drawRoundedRectangle (r, 4.0f, on ? 2.0f : 1.2f);
}

void ChaosLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const auto accent = juce::Colour ((juce::uint32) (int) b.getProperties().getWithDefault ("accent", (int) col::red.getARGB()));
    g.setColour (b.getToggleState() ? accent.brighter (0.5f) : col::text);
    g.setFont (juce::FontOptions ((float) juce::jmin (18, b.getHeight() - 8), juce::Font::bold));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}

//==============================================================================
void WaveView::refreshThumb()
{
    auto s = proc.getSample (lane);
    if (s.get() == thumbFor) return;
    thumbFor = s.get();
    peaks.clear();
    if (s == nullptr) { repaint(); return; }
    const int cols = 600;
    const int len = s->buffer.getNumSamples();
    peaks.resize (cols);
    for (int c = 0; c < cols; ++c)
    {
        const int a = (int) ((juce::int64) len * c / cols), b = juce::jmax (a + 1, (int) ((juce::int64) len * (c + 1) / cols));
        float m = 0;
        for (int ch = 0; ch < 2; ++ch)
            m = juce::jmax (m, s->buffer.getMagnitude (ch, a, juce::jmin (b, len) - a));
        peaks[(size_t) c] = m;
    }
    repaint();
}

void WaveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    const bool active = proc.activeLane.load() == lane;
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 6);
    g.setColour (active ? col::red : juce::Colour (0xffbbbbbb));
    g.drawRoundedRectangle (r, 6, active ? 3.0f : 1.5f);

    auto inner = r.reduced (8, 6);
    if (peaks.empty())
    {
        g.setColour (juce::Colours::grey);
        g.setFont (14.0f);
        g.drawText ("Click or drop an audio loop here", inner, juce::Justification::centred);
        return;
    }
    const bool rev = proc.apvts.getRawParameterValue (ChaosTriggerProcessor::laneId (lane, "rev"))->load() > 0.5f;
    g.setColour (active ? juce::Colours::white : juce::Colour (0xffcfcfcf));
    const float mid = inner.getCentreY(), hh = inner.getHeight() / 2;
    const int n = (int) peaks.size();
    for (int x = 0; x < (int) inner.getWidth(); ++x)
    {
        int idx = juce::jlimit (0, n - 1, (int) (x * n / inner.getWidth()));
        if (rev) idx = n - 1 - idx;
        const float v = peaks[(size_t) idx] * hh;
        g.drawVerticalLine ((int) inner.getX() + x, mid - v, mid + v);
    }
    const float p = proc.playPos[(size_t) lane].load();
    const float px = inner.getX() + (rev ? 1.0f - p : p) * inner.getWidth();
    g.setColour (col::red);
    g.drawLine (px, inner.getY(), px, inner.getBottom(), 2.0f);
}

void WaveView::mouseDown (const juce::MouseEvent&)
{
    chooser = std::make_unique<juce::FileChooser> ("Load sample for lane " + juce::String (lane + 1),
                                                   juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile() && proc.loadSample (lane, fc.getResult()) && onLoaded) onLoaded();
        });
}

void WaveView::filesDropped (const juce::StringArray& f, int, int)
{
    if (! f.isEmpty() && proc.loadSample (lane, juce::File (f[0])) && onLoaded) onLoaded();
}

//==============================================================================
static void setupKnob (juce::Slider& s, juce::Colour c, bool textBox = false)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (textBox ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox, false, 60, 16);
    s.setPopupDisplayEnabled (true, true, nullptr);
    s.getProperties().set ("knobColour", (int) c.getARGB());
}

ChaosPanel::ChaosPanel (ChaosTriggerProcessor& p)
    : proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    auto label = [this] (juce::Label& l, const juce::String& t, float size, juce::Colour c = col::text)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::FontOptions (size, juce::Font::bold));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    setupKnob (pitch, col::red, true);
    addAndMakeVisible (pitch);
    sAtt.push_back (std::make_unique<SA> (proc.apvts, "pitch", pitch));
    label (pitchLabel, "PITCH", 14);

    setupKnob (freeRate, juce::Colour (0xff888888), true);
    freeRate.setTextValueSuffix (" Hz");
    addAndMakeVisible (freeRate);
    sAtt.push_back (std::make_unique<SA> (proc.apvts, "freeRate", freeRate));
    label (rateLabel, "FREE RATE", 12);

    setupKnob (swing, juce::Colour (0xff888888), false);
    addAndMakeVisible (swing);
    sAtt.push_back (std::make_unique<SA> (proc.apvts, "swing", swing));
    label (swingLabel, "SWING", 12);

    // Mode pairs
    retrig.onClick = [this] { setParam ("legato", 0); };
    legato.onClick = [this] { setParam ("legato", 1); };
    sync.onClick   = [this] { setParam ("free", 0); };
    freeBtn.onClick= [this] { setParam ("free", 1); };
    for (auto* b : { &retrig, &legato, &sync, &freeBtn }) addAndMakeVisible (b);

    label (dividerTitle, "CHAOS DIVIDER", 22);
    const char* divs[] = { "128", "64", "32", "16", "8", "4", "2", "1" };
    for (int i = 0; i < 8; ++i)
    {
        auto* b = divBtns.add (new juce::TextButton (divs[i]));
        b->onClick = [this, i] { setParam ("division", (float) i); };
        addAndMakeVisible (b);
    }
    const char* feels[] = { "STRAIGHT", "TRIPLET", "DOTTED", "SWING" };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = feelBtns.add (new juce::TextButton (feels[i]));
        b->onClick = [this, i] { setParam ("feel", (float) i); };
        addAndMakeVisible (b);
    }

    label (pickerTitle, "CHAOS PICKER", 22);
    pattern.addItemList (proc.apvts.getParameter ("pattern")->getAllValueStrings(), 1);
    addAndMakeVisible (pattern);
    patAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "pattern", pattern);
    randoom.onClick = [this] { setParam ("pattern", 24); };
    addAndMakeVisible (randoom);
    scrollBtn.setClickingTogglesState (true);
    scrollBtn.getProperties().set ("accent", (int) juce::Colour (0xffff8a00).getARGB());
    addAndMakeVisible (scrollBtn);
    scrollAtt = std::make_unique<BA> (proc.apvts, "scroll", scrollBtn);
    label (nowPlaying, "", 15, col::red);

    // Play without MIDI: click the keys, or use the computer keyboard (A W S E D F T G Y H U J K = C3..C4,
    // Z / X = octave down / up). C3 = original pitch.
    keyboard.setOctaveForMiddleC (3);
    keyboard.setKeyPressBaseOctave (5);
    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, col::red);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, col::red.withAlpha (0.3f));
    keyboard.setWantsKeyboardFocus (true);
    addAndMakeVisible (keyboard);
    keyTrackBtn.setClickingTogglesState (true);
    addAndMakeVisible (keyTrackBtn);
    keyTrackAtt = std::make_unique<BA> (proc.apvts, "keytrack", keyTrackBtn);

    loadBtn.onClick = [this] { loadForLane (loadTarget); };
    addAndMakeVisible (loadBtn);
    for (int i = 0; i < kNumLanes; ++i)
    {
        auto* b = loadLaneBtns.add (new juce::TextButton (juce::String (i + 1)));
        b->onClick = [this, i] { loadTarget = i; };
        addAndMakeVisible (b);
    }

    // ---- master FX ----
    label (masterTitle, "MASTER", 18, col::red);
    auto mk = [this] (juce::Slider& s, const juce::String& id, juce::Colour c)
    {
        setupKnob (s, c);
        addAndMakeVisible (s);
        sAtt.push_back (std::make_unique<SA> (proc.apvts, id, s));
    };
    mk (mDist, "m_dist", col::red);  mk (mHaas, "m_haas", col::red);
    mk (mCrush, "m_crush", col::red); mk (mSmash, "m_smash", col::red);
    mk (ringHz, "m_ringHz", juce::Colour (0xff888888));
    mk (cutoff, "m_cutoff", col::red); mk (res, "m_res", col::red);
    label (envTitle, "MASTER ENVELOPE", 13);
    mk (mA, "m_a", col::red); mk (mD, "m_d", col::red); mk (mS, "m_s", col::red); mk (mR, "m_r", col::red);
    ringBtn.setClickingTogglesState (true);
    addAndMakeVisible (ringBtn);
    ringAtt = std::make_unique<BA> (proc.apvts, "m_ring", ringBtn);
    const char* fm[] = { "OFF", "LP", "BP", "HP" };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = fModeBtns.add (new juce::TextButton (fm[i]));
        b->onClick = [this, i] { setParam ("m_fmode", (float) i); };
        addAndMakeVisible (b);
    }

    for (int i = 0; i < kNumLanes; ++i)
    {
        auto& L = lanes[(size_t) i];
        auto id = [i] (const char* s) { return ChaosTriggerProcessor::laneId (i, s); };

        L.name.setJustificationType (juce::Justification::centred);
        L.name.setColour (juce::Label::outlineColourId, col::blood);
        L.name.setMinimumHorizontalScale (0.4f);    // auto-shrinks long names
        addAndMakeVisible (L.name);

        L.mono.setClickingTogglesState (true);
        L.mono.getProperties().set ("accent", (int) col::gold.getARGB());
        L.rev.setClickingTogglesState (true);
        L.rev.getProperties().set ("accent", (int) col::blue.getARGB());
        L.number.setButtonText (juce::String (i + 1));
        L.number.onClick = [this, i] { loadTarget = i; };
        for (auto* b : { &L.mono, &L.rev, &L.number }) addAndMakeVisible (b);
        L.bAtt.push_back (std::make_unique<BA> (proc.apvts, id ("mono"), L.mono));
        L.bAtt.push_back (std::make_unique<BA> (proc.apvts, id ("rev"), L.rev));

        L.power.setColour (juce::ToggleButton::tickColourId, col::red);
        addAndMakeVisible (L.power);
        L.bAtt.push_back (std::make_unique<BA> (proc.apvts, id ("power"), L.power));

        L.wave = std::make_unique<WaveView> (proc, i);
        L.wave->onLoaded = [this] { timerCallback(); };
        addAndMakeVisible (*L.wave);

        const char* adsr[] = { "a", "d", "s", "r" };
        juce::Slider* ks[] = { &L.a, &L.d, &L.s, &L.r };
        for (int k = 0; k < 4; ++k)
        {
            setupKnob (*ks[k], col::red);
            addAndMakeVisible (*ks[k]);
            L.sAtt.push_back (std::make_unique<SA> (proc.apvts, id (adsr[k]), *ks[k]));
        }
        setupKnob (L.pitch, juce::Colour (0xff8a8a8a));
        addAndMakeVisible (L.pitch);
        L.sAtt.push_back (std::make_unique<SA> (proc.apvts, id ("pitch"), L.pitch));
        setupKnob (L.pan, juce::Colour (0xff3a3a3a));
        setupKnob (L.vol, juce::Colour (0xffd8d8d8));
        addAndMakeVisible (L.pan); addAndMakeVisible (L.vol);
        L.sAtt.push_back (std::make_unique<SA> (proc.apvts, id ("pan"), L.pan));
        L.sAtt.push_back (std::make_unique<SA> (proc.apvts, id ("vol"), L.vol));

        const char* fx[] = { "dist", "haas", "crush", "smash" };
        juce::Slider* fs[] = { &L.dist, &L.haas, &L.crush, &L.smash };
        for (int k = 0; k < 4; ++k)
        {
            setupKnob (*fs[k], juce::Colour (0xff9a1a1a));
            addAndMakeVisible (*fs[k]);
            L.sAtt.push_back (std::make_unique<SA> (proc.apvts, id (fx[k]), *fs[k]));
        }
        L.chorus.getProperties().set ("accent", (int) juce::Colour (0xff3fc6c6).getARGB());
        L.chorus.onClick = [this, i]
        {
            const auto cid = ChaosTriggerProcessor::laneId (i, "chorus");
            setParam (cid, (float) (((int) getParam (cid) + 1) % 4));
        };
        addAndMakeVisible (L.chorus);
    }

    setSize (kDesignW, kDesignH);
    startTimerHz (30);
}

ChaosPanel::~ChaosPanel() { setLookAndFeel (nullptr); }

void ChaosPanel::setParam (const juce::String& id, float plain)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void ChaosPanel::loadForLane (int lane)
{
    chooser = std::make_unique<juce::FileChooser> ("Load sample for lane " + juce::String (lane + 1),
                                                   juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, lane] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) proc.loadSample (lane, fc.getResult());
            timerCallback();
        });
}

void ChaosPanel::timerCallback()
{
    // Keep computer-keyboard playing available unless the user is typing somewhere else.
    if (isShowing() && juce::Component::getCurrentlyFocusedComponent() == nullptr)
        keyboard.grabKeyboardFocus();
    const bool leg = getParam ("legato") > 0.5f, fr = getParam ("free") > 0.5f;
    retrig.setToggleState (! leg, juce::dontSendNotification);
    legato.setToggleState (leg, juce::dontSendNotification);
    sync.setToggleState (! fr, juce::dontSendNotification);
    freeBtn.setToggleState (fr, juce::dontSendNotification);
    freeRate.setEnabled (fr);
    for (auto* b : divBtns) b->setEnabled (! fr);
    const int div = (int) getParam ("division"), feel = (int) getParam ("feel");
    for (int i = 0; i < divBtns.size(); ++i) divBtns[i]->setToggleState (i == div, juce::dontSendNotification);
    for (int i = 0; i < feelBtns.size(); ++i) feelBtns[i]->setToggleState (i == feel, juce::dontSendNotification);
    swing.setEnabled (feel == 3);
    const int fmode = (int) getParam ("m_fmode");
    for (int i = 0; i < 4; ++i) fModeBtns[i]->setToggleState (i == fmode, juce::dontSendNotification);
    cutoff.setEnabled (fmode > 0); res.setEnabled (fmode > 0);
    ringHz.setEnabled (ringBtn.getToggleState());
    randoom.setToggleState ((int) getParam ("pattern") == 24, juce::dontSendNotification);
    {
        const int pp = proc.playingPattern.load();
        juce::String s = "NOW: ";
        if (pp >= 24) s << "66 RANDOOM";
        else { s << (pp + 1) << " - "; for (int k = 0; k < 4; ++k) s << (kPatterns[pp][k] + 1); }
        nowPlaying.setText (s, juce::dontSendNotification);
    }
    for (int i = 0; i < kNumLanes; ++i) loadLaneBtns[i]->setToggleState (i == loadTarget, juce::dontSendNotification);

    for (int i = 0; i < kNumLanes; ++i)
    {
        auto& L = lanes[(size_t) i];
        auto s = proc.getSample (i);
        L.name.setText (s ? s->name.toUpperCase() : "NO SAMPLE", juce::dontSendNotification);
        L.name.setColour (juce::Label::textColourId, s ? col::text : juce::Colours::grey);
        const int ch = (int) getParam (ChaosTriggerProcessor::laneId (i, "chorus"));
        const char* cn[] = { "CHORUS OFF", "CHORUS I", "CHORUS II", "CHORUS I+II" };
        L.chorus.setButtonText (cn[ch]);
        L.chorus.setToggleState (ch > 0, juce::dontSendNotification);
        L.number.setToggleState (proc.activeLane.load() == i, juce::dontSendNotification);
        L.wave->refreshThumb();
        L.wave->repaint();
    }
}

void ChaosPanel::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::blood.withAlpha (0.55f), b.getCentreX(), b.getHeight() * 0.4f,
                                             col::bg, 0, 0, true));
    g.fillRect (b);
    // grungy speckle
    juce::Random r (666);
    for (int i = 0; i < 1600; ++i)
    {
        g.setColour (juce::Colour (0xff000000).withAlpha (r.nextFloat() * 0.35f));
        g.fillEllipse (r.nextFloat() * b.getWidth(), r.nextFloat() * b.getHeight(), 1 + r.nextFloat() * 5, 1 + r.nextFloat() * 5);
    }

    // ADSR / PAN / VOL captions per lane
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    for (auto& L : lanes)
    {
        const char* t[] = { "A", "D", "S", "R" };
        juce::Slider* ks[] = { &L.a, &L.d, &L.s, &L.r };
        g.setColour (col::text);
        for (int k = 0; k < 4; ++k)
            g.drawText (t[k], ks[k]->getBounds().withY (ks[k]->getBottom()).withHeight (16), juce::Justification::centred);
        g.drawText ("PITCH " + juce::String ((int) L.pitch.getValue()), L.pitch.getBounds().withY (L.pitch.getBottom()).withHeight (16).expanded (14, 0), juce::Justification::centred);
        g.drawText ("PAN", L.pan.getBounds().withY (L.pan.getBottom()).withHeight (16), juce::Justification::centred);
        g.drawText ("VOL", L.vol.getBounds().withY (L.vol.getBottom()).withHeight (16), juce::Justification::centred);
        const char* ft[] = { "DIST", "HAAS", "CRUSH", "SMASH" };
        juce::Slider* fs[] = { &L.dist, &L.haas, &L.crush, &L.smash };
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        for (int k = 0; k < 4; ++k)
            g.drawText (ft[k], fs[k]->getBounds().withY (fs[k]->getBottom()).withHeight (14).expanded (10, 0), juce::Justification::centred);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.setColour (col::text.withAlpha (0.8f));
        g.fillRect (L.a.getX(), L.a.getBottom() + 18, L.r.getRight() - L.a.getX(), 2);
    }
    {
        const char* mt[] = { "DIST", "HAAS", "CRUSH", "SMASH", "RING HZ", "CUTOFF", "RES" };
        juce::Slider* ms[] = { &mDist, &mHaas, &mCrush, &mSmash, &ringHz, &cutoff, &res };
        g.setColour (col::text);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        for (int k = 0; k < 7; ++k)
            g.drawText (mt[k], ms[k]->getBounds().withY (ms[k]->getBottom()).withHeight (14).expanded (12, 0), juce::Justification::centred);
        auto band = masterTitle.getBounds().getUnion (res.getBounds()).expanded (6, 4).withBottom (res.getBottom() + 16);
        g.setColour (col::red.withAlpha (0.5f));
        g.drawRoundedRectangle (band.toFloat(), 6, 1.2f);
    }
    {
        const char* et[] = { "A", "D", "S", "R" };
        juce::Slider* es[] = { &mA, &mD, &mS, &mR };
        g.setColour (col::text);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        for (int k = 0; k < 4; ++k)
            g.drawText (et[k], es[k]->getBounds().withY (es[k]->getBottom()).withHeight (14), juce::Justification::centred);
    }
    g.setColour (col::text.withAlpha (0.4f));
    g.drawVerticalLine (pickerTitle.getX() - 12, 10.0f, (float) loadBtn.getBottom());
}

void ChaosPanel::resized()
{
    auto area = getLocalBounds().reduced (14);
    {
        auto kb = area.removeFromBottom (78);
        keyTrackBtn.setBounds (kb.removeFromLeft (110).withSizeKeepingCentre (104, 34));
        kb.removeFromLeft (6);
        keyboard.setBounds (kb);
        keyboard.setKeyWidth (juce::jmax (12.0f, kb.getWidth() / 50.0f));
        area.removeFromBottom (8);
    }
    const int W = area.getWidth();

    // ---- header -------------------------------------------------------------
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.25f));
    auto left = top.removeFromLeft (W * 40 / 100);
    auto mid  = top.removeFromLeft (W * 30 / 100);
    auto right = top.reduced (16, 0);

    auto pitchArea = left.removeFromLeft (left.getWidth() / 4);
    pitchLabel.setBounds (pitchArea.removeFromTop (18));
    pitch.setBounds (pitchArea.removeFromTop (juce::jmin (pitchArea.getHeight() - 40, 100)));

    auto modes = left.removeFromTop (left.getHeight() * 2 / 3).reduced (4);
    auto row1 = modes.removeFromTop (modes.getHeight() / 2).reduced (0, 3);
    retrig.setBounds (row1.removeFromLeft (row1.getWidth() / 3).reduced (2));
    sync.setBounds (row1.removeFromLeft (row1.getWidth() / 2).reduced (2));
    rateLabel.setBounds (row1.removeFromTop (14));
    freeRate.setBounds (row1);
    auto row2 = modes.reduced (0, 3);
    legato.setBounds (row2.removeFromLeft (row2.getWidth() / 3).reduced (2));
    freeBtn.setBounds (row2.removeFromLeft (row2.getWidth() / 2).reduced (2));

    auto loadRow = left.reduced (4);
    loadBtn.setBounds (loadRow.removeFromLeft (loadRow.getWidth() / 2).reduced (2));
    const int lw = loadRow.getWidth() / 4;
    for (auto* b : loadLaneBtns) b->setBounds (loadRow.removeFromLeft (lw).reduced (3));

    dividerTitle.setBounds (mid.removeFromTop (32));
    auto divRow1 = mid.removeFromTop (mid.getHeight() / 3);
    auto divRow2 = mid.removeFromTop (mid.getHeight() / 2);
    for (int i = 0; i < 4; ++i) divBtns[i]->setBounds (divRow1.removeFromLeft (divRow1.getWidth() / (4 - i)).reduced (3));
    for (int i = 4; i < 8; ++i) divBtns[i]->setBounds (divRow2.removeFromLeft (divRow2.getWidth() / (8 - i)).reduced (3));
    auto swingArea = mid.removeFromRight (56);
    swingLabel.setBounds (swingArea.removeFromBottom (14));
    swing.setBounds (swingArea);
    for (int i = 0; i < 4; ++i)
    {
        auto cell = mid.withWidth (mid.getWidth() / 2).withHeight (mid.getHeight() / 2)
                       .translated ((i % 2) * mid.getWidth() / 2, (i / 2) * mid.getHeight() / 2);
        feelBtns[i]->setBounds (cell.reduced (3));
    }

    pickerTitle.setBounds (right.removeFromTop (32));
    right.removeFromTop (10);
    pattern.setBounds (right.removeFromTop (34).reduced (4, 0));
    right.removeFromTop (10);
    {
        auto rr = right.removeFromTop (34);
        randoom.setBounds (rr.removeFromLeft (rr.getWidth() / 2).reduced (4, 0));
        scrollBtn.setBounds (rr.reduced (4, 0));
    }
    right.removeFromTop (6);
    nowPlaying.setBounds (right.removeFromTop (22));
    right.removeFromTop (4);
    envTitle.setBounds (right.removeFromTop (16));
    {
        auto er = right.removeFromTop (right.getHeight() - 14);
        const int kw = er.getWidth() / 4, kh = juce::jmin (er.getHeight(), kw);
        juce::Slider* es[] = { &mA, &mD, &mS, &mR };
        for (auto* s : es) s->setBounds (er.removeFromLeft (kw).withSizeKeepingCentre (kh, kh));
    }

    // ---- master FX band -----------------------------------------------------
    area.removeFromTop (6);
    auto mb = area.removeFromTop (juce::roundToInt (kDesignH * 0.10f)).reduced (6, 2);
    masterTitle.setBounds (mb.removeFromLeft (90));
    const int mh = mb.getHeight() - 16;
    juce::Slider* fx4[] = { &mDist, &mHaas, &mCrush, &mSmash };
    for (auto* s : fx4) s->setBounds (mb.removeFromLeft (mh + 20).withHeight (mh).withSizeKeepingCentre (mh, mh));
    mb.removeFromLeft (20);
    ringBtn.setBounds (mb.removeFromLeft (110).withSizeKeepingCentre (110, 32));
    ringHz.setBounds (mb.removeFromLeft (mh + 20).withHeight (mh).withSizeKeepingCentre (mh, mh));
    mb.removeFromLeft (20);
    auto fbtn = mb.removeFromLeft (240).withSizeKeepingCentre (240, 32);
    for (int i = 0; i < 4; ++i) fModeBtns[i]->setBounds (fbtn.removeFromLeft (60).reduced (2, 0));
    cutoff.setBounds (mb.removeFromLeft (mh + 20).withHeight (mh).withSizeKeepingCentre (mh, mh));
    res.setBounds (mb.removeFromLeft (mh + 20).withHeight (mh).withSizeKeepingCentre (mh, mh));

    // ---- lanes --------------------------------------------------------------
    area.removeFromTop (8);
    const int laneH = area.getHeight() / kNumLanes;
    for (auto& L : lanes)
    {
        auto row = area.removeFromTop (laneH).reduced (0, 4);
        auto wavePart = row.removeFromLeft (W * 42 / 100);
        auto head = wavePart.removeFromTop (26);
        head.removeFromLeft (44);
        L.name.setBounds (head.removeFromLeft (head.getWidth() * 45 / 100).reduced (2, 0));
        L.mono.setBounds (head.removeFromLeft (head.getWidth() / 2).reduced (3, 0));
        L.rev.setBounds (head.reduced (3, 0));
        L.number.setBounds (wavePart.removeFromLeft (40).withSizeKeepingCentre (34, 40));
        wavePart.removeFromLeft (4);
        L.wave->setBounds (wavePart.reduced (0, 3));

        row.removeFromLeft (16);
        auto powerArea = row.removeFromRight (80);
        L.power.setBounds (powerArea.withSizeKeepingCentre (80, 26));
        auto fxArea = row.removeFromRight (row.getWidth() * 38 / 100);
        L.chorus.setBounds (fxArea.removeFromBottom (26).reduced (8, 0));
        const int fk = juce::jmin (fxArea.getHeight() - 16, fxArea.getWidth() / 4 - 6);
        juce::Slider* fs[] = { &L.dist, &L.haas, &L.crush, &L.smash };
        const int fw = fxArea.getWidth() / 4;
        for (int k = 0; k < 4; ++k)
            fs[k]->setBounds (juce::Rectangle<int> (fxArea.getX() + k * fw, fxArea.getY(), fw, fk).withSizeKeepingCentre (fk, fk));
        const int knobRowH = row.getHeight() - 24;
        const int ks = juce::jmin (knobRowH, row.getWidth() / 7);
        juce::Slider* ks6[] = { &L.a, &L.d, &L.s, &L.r, &L.pitch, &L.pan, &L.vol };
        const int colW = row.getWidth() / 7;
        for (int k = 0; k < 7; ++k)
            ks6[k]->setBounds (juce::Rectangle<int> (row.getX() + k * colW + k / 4 * 8, row.getY(), colW, ks + 0).withSizeKeepingCentre (ks, ks));
    }
}

//==============================================================================
ChaosTriggerEditor::ChaosTriggerEditor (ChaosTriggerProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    // The panel keeps its proportions and scales to any window size, so nothing gets cut off
    // when the host's plugin window is smaller than the design size (e.g. Reaper's FX window).
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) kDesignW / kDesignH);
        c->setSizeLimits (kDesignW / 3, kDesignH / 3, kDesignW * 2, kDesignH * 2);
    }
    // Open at a size that fits the screen (max 85% of the usable area).
    double scale = 1.0;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jmin (1.0, d->userArea.getWidth() * 0.85 / kDesignW, d->userArea.getHeight() * 0.85 / kDesignH);
    setSize (juce::roundToInt (kDesignW * scale), juce::roundToInt (kDesignH * scale));
}

void ChaosTriggerEditor::resized()
{
    const float s = juce::jmin (getWidth() / (float) kDesignW, getHeight() / (float) kDesignH);
    panel.setTransform (juce::AffineTransform::scale (s));
    panel.setTopLeftPosition (0, 0);
}
