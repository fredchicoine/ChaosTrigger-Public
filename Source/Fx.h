#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

// Settings for one effect chain (read from parameters each block).
struct FxSettings
{
    float dist = 0, haasMs = 0, bits = 24, smash = 0;
    int chorus = 0;            // 0 off, 1 = I, 2 = II, 3 = I+II   (lanes only)
    bool ring = false; float ringHz = 440;              // master only
    int filterMode = 0; float cutoff = 20000, res = 0;  // master only: 0 off, 1 LP24, 2 BP6, 3 HP12
};

class FxChain
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        haasBuf.assign ((size_t) (sr * 0.11) + 4, 0.0f);
        chorusBuf[0].assign ((size_t) (sr * 0.02) + 4, 0.0f);
        chorusBuf[1] = chorusBuf[0];
        haasW = chorusW = 0;
        env = 0; lfoPhase = 0; ringPhase = 0;

        juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlock), 2 };
        ladder.prepare (spec);
        ladder.setMode (juce::dsp::LadderFilterMode::LPF24);
        svf.prepare (spec);
        ladder.reset(); svf.reset();
    }

    void process (float* L, float* R, int n, const FxSettings& s)
    {
        if (L == nullptr || R == nullptr || n <= 0) return;

        // ---- Distortion (warm tanh saturation) --------------------------------
        if (s.dist > 0.001f)
        {
            const float drive = 1.0f + s.dist * 24.0f, norm = 1.0f / std::tanh (drive);
            for (int i = 0; i < n; ++i)
            {
                // slight asymmetry for even harmonics = warmth
                L[i] = std::tanh (drive * L[i] + 0.1f * s.dist) * norm - std::tanh (0.1f * s.dist) * norm;
                R[i] = std::tanh (drive * R[i] + 0.1f * s.dist) * norm - std::tanh (0.1f * s.dist) * norm;
            }
        }

        // ---- Bitcrush 24 -> 1 bit ---------------------------------------------
        if (s.bits < 23.5f)
        {
            const float q = std::pow (2.0f, s.bits - 1.0f);
            for (int i = 0; i < n; ++i)
            {
                L[i] = std::round (L[i] * q) / q;
                R[i] = std::round (R[i] * q) / q;
            }
        }

        // ---- Smash compressor: -6 dB 2:1  ->  -40 dB 20:1 ---------------------
        if (s.smash > 0.001f)
        {
            const float thr = -6.0f - 34.0f * s.smash, ratio = 2.0f + 18.0f * s.smash;
            const float makeup = -thr * (1.0f - 1.0f / ratio) * 0.6f;
            const float att = std::exp (-1.0f / (0.002f * (float) sr)), rel = std::exp (-1.0f / (0.12f * (float) sr));
            for (int i = 0; i < n; ++i)
            {
                const float x = juce::jmax (std::abs (L[i]), std::abs (R[i]));
                env = x > env ? att * env + (1 - att) * x : rel * env + (1 - rel) * x;
                const float db = juce::Decibels::gainToDecibels (env, -120.0f);
                const float over = db - thr;
                const float gr = over > 0 ? over * (1.0f - 1.0f / ratio) : 0.0f;
                const float g = juce::Decibels::decibelsToGain (makeup - gr);
                L[i] *= g; R[i] *= g;
            }
        }

        // ---- Juno-60 style chorus ---------------------------------------------
        if (s.chorus > 0)
        {
            // I: 0.4 Hz triangle, II: 0.6 Hz triangle (deep); I+II: 8 Hz sine (shallow vibrato)
            const float rate  = s.chorus == 1 ? 0.4f : s.chorus == 2 ? 0.6f : 8.0f;
            const float depth = (s.chorus == 3 ? 0.25f : 1.85f) * 0.001f * (float) sr;
            const float centre = 3.5f * 0.001f * (float) sr;
            const int size = (int) chorusBuf[0].size();
            for (int i = 0; i < n; ++i)
            {
                float m;
                if (s.chorus == 3) m = std::sin (lfoPhase * juce::MathConstants<float>::twoPi);
                else m = 4.0f * std::abs (lfoPhase - 0.5f) - 1.0f;           // triangle
                lfoPhase += rate / (float) sr; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

                const float mono = 0.5f * (L[i] + R[i]);
                chorusBuf[0][(size_t) chorusW] = mono;
                const float wl = readFrac (chorusBuf[0], chorusW, centre + depth * m, size);
                const float wr = readFrac (chorusBuf[0], chorusW, centre - depth * m, size);  // inverted LFO on R
                chorusW = (chorusW + 1) % size;
                L[i] = 0.7f * L[i] + 0.7f * wl;
                R[i] = 0.7f * R[i] + 0.7f * wr;
            }
        }

        // ---- Ring mod (master) ------------------------------------------------
        if (s.ring)
        {
            for (int i = 0; i < n; ++i)
            {
                const float m = std::sin (ringPhase * juce::MathConstants<float>::twoPi);
                ringPhase += s.ringHz / (float) sr; if (ringPhase >= 1.0f) ringPhase -= 1.0f;
                L[i] *= m; R[i] *= m;
            }
        }

        // ---- Filter (master) --------------------------------------------------
        if (s.filterMode > 0)
        {
            float* ch[] = { L, R };
            juce::dsp::AudioBlock<float> block (ch, 2, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            if (s.filterMode == 1)
            {
                ladder.setCutoffFrequencyHz (s.cutoff);
                ladder.setResonance (juce::jlimit (0.0f, 0.95f, s.res));
                ladder.process (ctx);
            }
            else
            {
                svf.setType (s.filterMode == 2 ? juce::dsp::StateVariableTPTFilterType::bandpass
                                               : juce::dsp::StateVariableTPTFilterType::highpass);
                svf.setCutoffFrequency (s.cutoff);
                svf.setResonance (0.5f + s.res * 9.5f);   // Q 0.5 .. 10
                svf.process (ctx);
            }
        }

        // ---- Haas: delay the right side 0..100 ms ------------------------------
        if (s.haasMs > 0.05f)
        {
            const int size = (int) haasBuf.size();
            const float d = s.haasMs * 0.001f * (float) sr;
            for (int i = 0; i < n; ++i)
            {
                haasBuf[(size_t) haasW] = R[i];
                R[i] = readFrac (haasBuf, haasW, d, size);
                haasW = (haasW + 1) % size;
            }
        }
        else
        {
            for (int i = 0; i < n; ++i) { haasBuf[(size_t) haasW] = R[i]; haasW = (haasW + 1) % (int) haasBuf.size(); }
        }
    }

private:
    static float readFrac (const std::vector<float>& b, int w, float delay, int size)
    {
        float rp = (float) w - delay;
        while (rp < 0) rp += (float) size;
        const int i0 = (int) rp % size, i1 = (i0 + 1) % size;
        const float f = rp - std::floor (rp);
        return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
    }

    double sr = 44100;
    std::vector<float> haasBuf, chorusBuf[2];
    int haasW = 0, chorusW = 0;
    float env = 0, lfoPhase = 0, ringPhase = 0;
    juce::dsp::LadderFilter<float> ladder;
    juce::dsp::StateVariableTPTFilter<float> svf;
};
