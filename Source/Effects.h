#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include "Parameters.h"

// Effects that run on the sample bus (the triggered sounds), in this order:
// Gate > EQ > Compressor > Delay > Reverb. Every stage has its own on/off switch.
namespace BTFx
{
using juce::Decibels;

constexpr float kTwoPi = 6.28318530717958647692f;

// ----------------------------------------------------------------------------- filters
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = {}, z2[2] = {};

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

    float process (int ch, float x)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    static double clampF (double sr, double f) { return juce::jlimit (10.0, sr * 0.45, f); }

    void highpass (double sr, double f, double q = 0.7071)
    {
        const double w = kTwoPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void peak (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = kTwoPi * clampF (sr, f) / sr;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    void lowShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = kTwoPi * clampF (sr, f) / sr;
        const double c = std::cos (w), s = std::sin (w), al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }

    void highShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = kTwoPi * clampF (sr, f) / sr;
        const double c = std::cos (w), s = std::sin (w), al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

// ----------------------------------------------------------------------------- gate
struct Gate
{
    double sr = 44100.0;
    float thr = 0.01f, closeThr = 0.007f, range = 0.001f, atkC = 0.0f, relC = 0.0f, detRel = 0.0f;
    int holdSamples = 0, hold = 0;
    float det = 0.0f, gain = 1.0f;
    bool open = false;

    void prepare (double s) { sr = s; det = 0.0f; gain = 1.0f; hold = 0; open = false; }

    static float coef (double sr, float ms) { return std::exp (-1.0f / (juce::jmax (0.02f, ms) * 0.001f * (float) sr)); }

    void update (float thrDb, float atkMs, float holdMs, float relMs, float rangeDb)
    {
        thr = Decibels::decibelsToGain (thrDb);
        closeThr = Decibels::decibelsToGain (thrDb - 3.0f);       // 3 dB hysteresis stops chattering
        range = Decibels::decibelsToGain (rangeDb, -100.0f);
        atkC = coef (sr, atkMs);
        relC = coef (sr, relMs);
        detRel = coef (sr, 5.0f);
        holdSamples = (int) (holdMs * 0.001 * sr);
    }

    void process (float& l, float& r)
    {
        const float lvl = juce::jmax (std::abs (l), std::abs (r));
        det = lvl > det ? lvl : lvl + detRel * (det - lvl);       // instant attack, 5 ms fall

        if (det > thr)
        {
            open = true;
            hold = holdSamples;
        }
        else if (open)
        {
            if (det < closeThr)
            {
                if (hold > 0)
                    --hold;
                else
                    open = false;
            }
            else
                hold = holdSamples;
        }

        const float target = open ? 1.0f : range;
        const float c = target > gain ? atkC : relC;
        gain = target + c * (gain - target);
        l *= gain;
        r *= gain;
    }
};

// ----------------------------------------------------------------------------- compressor
struct Compressor
{
    double sr = 44100.0;
    float thr = -18.0f, ratio = 4.0f, knee = 6.0f, makeup = 1.0f, mix = 1.0f, atkC = 0.0f, relC = 0.0f;
    float grDb = 0.0f;                                            // smoothed gain reduction (<= 0)

    void prepare (double s) { sr = s; grDb = 0.0f; }

    void update (float thrDb, float ratioIn, float atkMs, float relMs, float kneeDb, float makeupDb, float mixPct)
    {
        thr = thrDb; ratio = juce::jmax (1.0f, ratioIn); knee = kneeDb;
        makeup = Decibels::decibelsToGain (makeupDb);
        mix = mixPct * 0.01f;
        atkC = Gate::coef (sr, atkMs);
        relC = Gate::coef (sr, relMs);
    }

    // static curve: how many dB the level is pushed down
    float reductionDb (float xDb) const
    {
        const float over = xDb - thr;
        float y;
        if (2.0f * over < -knee)
            y = xDb;
        else if (2.0f * std::abs (over) <= knee)
        {
            const float t = over + knee * 0.5f;
            y = xDb + (1.0f / ratio - 1.0f) * t * t / (2.0f * knee);
        }
        else
            y = thr + over / ratio;
        return y - xDb;
    }

    void process (float& l, float& r)
    {
        const float lvl = juce::jmax (std::abs (l), std::abs (r), 1.0e-6f);
        const float gr = reductionDb (Decibels::gainToDecibels (lvl, -120.0f));
        const float c = gr < grDb ? atkC : relC;                  // more reduction = attack
        grDb = gr + c * (grDb - gr);

        const float g = Decibels::decibelsToGain (grDb) * makeup;
        l = l * (1.0f - mix) + l * g * mix;
        r = r * (1.0f - mix) + r * g * mix;
    }
};

// ----------------------------------------------------------------------------- delay
struct Delay
{
    double sr = 44100.0;
    std::vector<float> buf[2];
    int size = 0, wp = 0;
    float timeS = 0.0f, targetS = 0.0f, smoothC = 0.0f, fb = 0.0f, mix = 0.0f, hpA = 0.0f, lpA = 0.0f;
    float hpLow[2] = {}, lpState[2] = {};
    bool ping = false, snap = true;      // snap: jump straight to the set time (first use, or after switching on)

    void prepare (double s)
    {
        sr = s;
        snap = true;
        size = (int) (s * 1.05) + 8;
        for (auto& b : buf)
            b.assign ((size_t) size, 0.0f);
        wp = 0;
        timeS = targetS = 0.25f * (float) s;
        smoothC = 1.0f - std::exp (-1.0f / (0.03f * (float) s));
        hpLow[0] = hpLow[1] = lpState[0] = lpState[1] = 0.0f;
    }

    void update (float timeMs, float fbPct, float lowHz, float highHz, float mixPct, bool pingPong)
    {
        targetS = juce::jlimit (1.0f, (float) (size - 4), timeMs * 0.001f * (float) sr);
        if (snap)
        {
            timeS = targetS;
            snap = false;
        }
        fb = juce::jmin (0.95f, fbPct * 0.01f);
        mix = mixPct * 0.01f;
        hpA = std::exp (-kTwoPi * lowHz / (float) sr);
        lpA = std::exp (-kTwoPi * juce::jmin (highHz, (float) sr * 0.45f) / (float) sr);
        ping = pingPong;
    }

    float read (int ch, float pos) const
    {
        float rp = (float) wp - pos;
        while (rp < 0.0f)
            rp += (float) size;
        const int i0 = (int) rp;
        const float f = rp - (float) i0;
        const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
        return buf[ch][(size_t) i0] * (1.0f - f) + buf[ch][(size_t) i1] * f;
    }

    // feedback path: roll off lows and highs so repeats get darker and thinner
    float filter (int ch, float x)
    {
        lpState[ch] = x + lpA * (lpState[ch] - x);                // low-pass: y = (1-a)x + a y
        const float lp = lpState[ch];
        hpLow[ch] = lp + hpA * (hpLow[ch] - lp);
        return lp - hpLow[ch];                                    // high-pass of the low-passed signal
    }

    void clear()
    {
        snap = true;
        for (auto& b : buf)
            std::fill (b.begin(), b.end(), 0.0f);
        hpLow[0] = hpLow[1] = lpState[0] = lpState[1] = 0.0f;
    }

    void process (float& l, float& r)
    {
        timeS += (targetS - timeS) * smoothC;
        const float yL = read (0, timeS), yR = read (1, timeS);
        const float fL = filter (0, yL), fR = filter (1, yR);

        float wl, wr;
        if (ping)
        {
            wl = (l + r) * 0.5f + fR * fb;
            wr = fL * fb;
        }
        else
        {
            wl = l + fL * fb;
            wr = r + fR * fb;
        }
        buf[0][(size_t) wp] = wl;
        buf[1][(size_t) wp] = wr;
        if (++wp >= size)
            wp = 0;

        l += yL * mix;
        r += yR * mix;
    }
};

// ----------------------------------------------------------------------------- reverb
struct Reverb
{
    double sr = 44100.0;
    juce::Reverb rv;
    std::vector<float> pre[2];
    int size = 0, wp = 0;
    float preS = 0.0f, mix = 0.0f;

    void prepare (double s)
    {
        sr = s;
        rv.setSampleRate (s);
        rv.reset();
        size = (int) (s * 0.11) + 8;
        for (auto& b : pre)
            b.assign ((size_t) size, 0.0f);
        wp = 0;
    }

    void clear()
    {
        rv.reset();
        for (auto& b : pre)
            std::fill (b.begin(), b.end(), 0.0f);
    }

    void update (float sizePct, float dampPct, float widthPct, float preMs, float mixPct)
    {
        juce::Reverb::Parameters p;
        p.roomSize = sizePct * 0.01f;
        p.damping = dampPct * 0.01f;
        p.width = widthPct * 0.01f;
        p.wetLevel = 0.5f;
        p.dryLevel = 0.0f;
        p.freezeMode = 0.0f;
        rv.setParameters (p);
        preS = juce::jlimit (0.0f, (float) (size - 4), preMs * 0.001f * (float) sr);
        mix = mixPct * 0.01f;
    }

    void process (float& l, float& r)
    {
        float il = l, ir = r;
        pre[0][(size_t) wp] = l;
        pre[1][(size_t) wp] = r;
        if (preS >= 1.0f)
        {
            int rp = wp - (int) preS;
            if (rp < 0)
                rp += size;
            il = pre[0][(size_t) rp];
            ir = pre[1][(size_t) rp];
        }
        if (++wp >= size)
            wp = 0;

        rv.processStereo (&il, &ir, 1);
        l += il * mix;
        r += ir * mix;
    }
};

// ----------------------------------------------------------------------------- EQ
struct Eq
{
    double sr = 44100.0;
    Biquad hp, lo, lm, hm, hi;
    bool useHp = false, useLo = false, useLm = false, useHm = false, useHi = false;

    void prepare (double s) { sr = s; hp.reset(); lo.reset(); lm.reset(); hm.reset(); hi.reset(); }

    void update (const float* v)
    {
        using namespace BT::Fx;
        useHp = v[EHp] > 21.0f;
        if (useHp) hp.highpass (sr, v[EHp]);
        useLo = std::abs (v[ELoG]) > 0.05f;
        if (useLo) lo.lowShelf (sr, v[ELoF], v[ELoG]);
        useLm = std::abs (v[ELmG]) > 0.05f;
        if (useLm) lm.peak (sr, v[ELmF], v[ELmQ], v[ELmG]);
        useHm = std::abs (v[EHmG]) > 0.05f;
        if (useHm) hm.peak (sr, v[EHmF], v[EHmQ], v[EHmG]);
        useHi = std::abs (v[EHiG]) > 0.05f;
        if (useHi) hi.highShelf (sr, v[EHiF], v[EHiG]);
    }

    void process (float& l, float& r)
    {
        float x[2] = { l, r };
        for (int c = 0; c < 2; ++c)
        {
            if (useHp) x[c] = hp.process (c, x[c]);
            if (useLo) x[c] = lo.process (c, x[c]);
            if (useLm) x[c] = lm.process (c, x[c]);
            if (useHm) x[c] = hm.process (c, x[c]);
            if (useHi) x[c] = hi.process (c, x[c]);
        }
        l = x[0];
        r = x[1];
    }
};

// ----------------------------------------------------------------------------- the chain
struct Chain
{
    Gate gate;
    Eq eq;
    Compressor comp;
    Delay delay;
    Reverb reverb;
    bool onGate = false, onEq = false, onComp = false, onDelay = false, onReverb = false;

    void prepare (double sr)
    {
        gate.prepare (sr); eq.prepare (sr); comp.prepare (sr); delay.prepare (sr); reverb.prepare (sr);
    }

    // v holds the current value of every BT::Fx parameter, indexed by BT::Fx::Id
    void update (const float* v)
    {
        using namespace BT::Fx;
        const bool wasDelay = onDelay, wasReverb = onReverb;
        onGate = v[GateOn] > 0.5f;   onEq = v[EqOn] > 0.5f;       onComp = v[CompOn] > 0.5f;
        onDelay = v[DelayOn] > 0.5f; onReverb = v[ReverbOn] > 0.5f;
        if (onDelay && ! wasDelay)     delay.clear();        // no old echoes when switching back on
        if (onReverb && ! wasReverb)   reverb.clear();

        if (onGate)   gate.update (v[GThr], v[GAtk], v[GHold], v[GRel], v[GRange]);
        if (onEq)     eq.update (v);
        if (onComp)   comp.update (v[CThr], v[CRatio], v[CAtk], v[CRel], v[CKnee], v[CMakeup], v[CMix]);
        if (onDelay)  delay.update (v[DTime], v[DFb], v[DLow], v[DHigh], v[DMix], v[DPing] > 0.5f);
        if (onReverb) reverb.update (v[RSize], v[RDamp], v[RWidth], v[RPre], v[RMix]);
    }

    void process (float& l, float& r)
    {
        if (onGate)   gate.process (l, r);
        if (onEq)     eq.process (l, r);
        if (onComp)   comp.process (l, r);
        if (onDelay)  delay.process (l, r);
        if (onReverb) reverb.process (l, r);
    }
};
} // namespace BTFx
