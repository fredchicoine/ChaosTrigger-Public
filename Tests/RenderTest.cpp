#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>
// Generates 4 test loops, plays a held note, checks audio + lane switching, snapshots GUI.
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    ChaosTriggerProcessor p;
    for (int l = 0; l < 4; ++l)
    {
        auto s = p.getSample (l);
        std::cout << "factory lane " << l + 1 << ": " << (s ? s->name : juce::String ("MISSING")) << " "
                  << (s ? s->buffer.getNumSamples() : 0) << "\n";
       #if CHAOS_HAS_DEMO_SAMPLES
        if (! s) return 4;
       #else
        if (s) return 4;   // public edition must ship empty
       #endif
    }
    const double sr = 48000; const int bs = 512;
    auto dir = juce::File (argv[1]);
    for (int l = 0; l < 4; ++l)
    {
        juce::AudioBuffer<float> b (2, (int) sr * 2);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float env = std::exp (-(float) (i % 12000) / (1500.0f * (l + 1)));
            float v = env * std::sin (i * 0.02f * (l + 1)) * 0.8f;
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
        auto f = dir.getChildFile ("Amen Break Lane " + juce::String (l + 1) + " 150bpm.wav");
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        w.reset();
        if (! p.loadSample (l, f)) { std::cerr << "load failed\n"; return 1; }
    }
    p.setPlayConfigDetails (0, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    p.apvts.getParameter ("l2_chorus")->setValueNotifyingHost (1.0f);
    p.apvts.getParameter ("m_fmode")->setValueNotifyingHost (p.apvts.getParameter ("m_fmode")->convertTo0to1 (1));
    p.apvts.getParameter ("m_cutoff")->setValueNotifyingHost (0.7f);
    p.apvts.getParameter ("l3_smash")->setValueNotifyingHost (0.8f);
    p.apvts.getParameter ("l1_dist")->setValueNotifyingHost (0.5f);
    p.apvts.getParameter ("l4_haas")->setValueNotifyingHost (0.2f);

    juce::AudioBuffer<float> buf (2, bs);
    juce::AudioBuffer<float> all (2, (int) sr * 3);
    std::set<int> lanesSeen; float peak = 0; bool nan = false;
    for (int blk = 0; blk * bs < all.getNumSamples() - bs; ++blk)
    {
        juce::MidiBuffer m;
        if (blk == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        if (blk == 200) m.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        p.processBlock (buf, m);
        lanesSeen.insert (p.activeLane.load());
        for (int c = 0; c < 2; ++c)
        {
            all.copyFrom (c, blk * bs, buf, c, 0, bs);
            for (int i = 0; i < bs; ++i) { float v = buf.getSample (c, i); nan |= ! std::isfinite (v); peak = std::max (peak, std::abs (v)); }
        }
    }
    std::cout << "peak=" << peak << " nan=" << nan << " lanes seen:";
    for (int l : lanesSeen) std::cout << " " << l;
    std::cout << "\n";
    {
        auto f = dir.getChildFile ("render.wav"); f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    }
    // Automation stress: move every parameter every block while playing.
    {
        auto& params = p.getParameters();
        int automatable = 0;
        for (auto* prm : params) automatable += prm->isAutomatable() ? 1 : 0;
        std::cout << "parameters=" << params.size() << " automatable=" << automatable << "\n";
        juce::Random r (1);
        bool bad = false;
        for (int blk = 0; blk < 400; ++blk)
        {
            juce::MidiBuffer m;
            if (blk % 50 == 0) m.addEvent (juce::MidiMessage::noteOn (1, 48 + (blk / 50) * 3, (juce::uint8) 100), 0);
            if (blk % 50 == 40) m.addEvent (juce::MidiMessage::noteOff (1, 48 + (blk / 50) * 3), 0);
            for (auto* prm : params)
                if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (prm); wp && wp->paramID != "m_ring" && ! wp->paramID.endsWith ("power"))
                    prm->setValueNotifyingHost (r.nextFloat());
            p.processBlock (buf, m);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) bad |= ! std::isfinite (buf.getSample (c, i));
        }
        std::cout << "automation sweep nan=" << bad << "\n";
        if (bad || automatable != params.size()) return 3;
        for (auto* prm : params) prm->setValueNotifyingHost (prm->getDefaultValue());
    }
    // Pitch-shift check: 440 Hz sine on lane 1 only, play C4 (72) -> expect ~880 Hz.
    {
        auto f = dir.getChildFile ("sine440.wav"); f.deleteFile();
        juce::AudioBuffer<float> b (2, (int) sr * 2);
        for (int i = 0; i < b.getNumSamples(); ++i) { float v = 0.5f * std::sin (2 * juce::MathConstants<float>::pi * 440.0f * i / (float) sr); b.setSample (0, i, v); b.setSample (1, i, v); }
        { juce::WavAudioFormat wav; std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
          w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
        p.loadSample (0, f);
        for (int l = 1; l < 4; ++l) p.apvts.getParameter (ChaosTriggerProcessor::laneId (l, "power"))->setValueNotifyingHost (0.0f);
        for (int note : { 60, 72, 55 })
        {
            juce::MidiBuffer off; off.addEvent (juce::MidiMessage::allNotesOff (1), 0); p.processBlock (buf, off);
            int crossings = 0; float prev = 0; int total = 0;
            for (int blk = 0; blk < 100; ++blk)
            {
                juce::MidiBuffer m;
                if (blk == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                p.processBlock (buf, m);
                if (blk < 10) continue;
                for (int i = 0; i < bs; ++i) { float v = buf.getSample (0, i); if (prev <= 0 && v > 0) ++crossings; prev = v; ++total; }
            }
            std::cout << "note " << note << " -> " << crossings * sr / total << " Hz\n";
        }
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (900, 649);
    p.activeLane = 1;
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png; juce::FileOutputStream os (dir.getChildFile ("gui.png"));
    os.setPosition (0); os.truncate(); png.writeImageToStream (img, os);
    // README screenshot: fresh instance with four example loops and a few settings.
    {
        ChaosTriggerProcessor q;
        q.setPlayConfigDetails (0, 2, sr, bs);
        q.prepareToPlay (sr, bs);
        const char* names[] = { "Drums_150bpm.wav", "Hats_150bpm.wav", "Bass_150bpm.wav", "Pad_150bpm.wav" };
        for (int l = 0; l < 4; ++l)
        {
            auto src = dir.getChildFile ("Amen Break Lane " + juce::String (l + 1) + " 150bpm.wav");
            auto dst = dir.getChildFile (names[l]);
            src.copyFileTo (dst);
            q.loadSample (l, dst);
        }
        auto set = [&q] (const char* id, float plain) { auto* pr = q.apvts.getParameter (id); pr->setValueNotifyingHost (pr->convertTo0to1 (plain)); };
        set ("pattern", 1); set ("m_fmode", 1); set ("m_cutoff", 6000); set ("m_res", 0.3f);
        set ("l2_chorus", 1); set ("l3_rev", 1); set ("l4_mono", 1); set ("l1_dist", 0.3f); set ("l3_crush", 12);
        set ("l2_pitch", 7); set ("l4_pitch", -5); set ("m_r", 0.6f); set ("m_a", 0.02f);
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        for (int blk = 0; blk < 30; ++blk) { q.processBlock (buf, m); m.clear(); }
        q.keyboardState.noteOn (1, 60, 1.0f);
        std::unique_ptr<juce::AudioProcessorEditor> qe (q.createEditor());
        qe->setSize (kDesignW, kDesignH);
        if (auto* panel = dynamic_cast<ChaosPanel*> (qe->getChildComponent (0))) panel->refresh();
        auto shot = qe->createComponentSnapshot (qe->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png2; juce::FileOutputStream os2 (dir.getChildFile ("screenshot.png"));
        os2.setPosition (0); os2.truncate(); png2.writeImageToStream (shot, os2);
    }
    return (peak > 0.01f && ! nan && lanesSeen.size() >= 4) ? 0 : 2;
}
