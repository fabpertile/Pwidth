// Dsp.h - the whole audio engine, with no JUCE dependency (so it can be tested on its own).
// Four bands -> dynamic width stage -> measure -> solo gain -> sum -> bypass crossfade.
// Ported from the Max patch "stereo_phase_adjust_4band_dynamic".
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace pw
{
constexpr int kBands = 4;
constexpr int kGonioSize = 1024;               // points kept per band for the goniometer

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ------------------------------------------------------------------------------------------------
// 2nd-order lowpass, Q = 0.5 (critically damped). Two in series = a 4th-order Linkwitz-Riley lowpass
// (24 dB/oct), the same as two cascaded svf~ objects with resonance 0 in the Max patch.
struct Lp2
{
    float ic1 = 0.f, ic2 = 0.f;
    static void coeffs (double fc, double fs, float& a1, float& a2, float& a3)
    {
        const double g = std::tan (3.14159265358979323846 * fc / fs);
        const double k = 2.0;                              // 1 / Q
        const double A1 = 1.0 / (1.0 + g * (g + k));
        a1 = (float) A1; a2 = (float) (g * A1); a3 = (float) (g * g * A1);
    }
    inline float process (float x, float a1, float a2, float a3)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        return v2;
    }
    void reset() { ic1 = ic2 = 0.f; }
};

// ------------------------------------------------------------------------------------------------
// Hilbert transformer: two chains of four all-pass sections whose outputs stay 90 degrees apart
// (Olli Niemitalo's IIR design). re = in-phase path, im = quadrature path (like hilbert~).
struct Hilbert
{
    struct Ap { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    std::array<Ap, 4> p1, p2;
    float delay = 0.f;
    static constexpr float c1[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
    static constexpr float c2[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };

    static inline float ap (Ap& s, float a2, float x)
    {
        const float y = a2 * (x + s.y2) - s.x2;
        s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y;
        return y;
    }
    inline void process (float x, float& re, float& im)
    {
        float a = x, b = x;
        for (int i = 0; i < 4; ++i) a = ap (p1[(size_t) i], c1[i] * c1[i], a);
        for (int i = 0; i < 4; ++i) b = ap (p2[(size_t) i], c2[i] * c2[i], b);
        re = delay;                // the in-phase path is delayed by one sample relative to the other
        delay = a;
        im = b;
    }
    inline float processReal (float x)
    {
        float a = x;
        for (int i = 0; i < 4; ++i) a = ap (p1[(size_t) i], c1[i] * c1[i], a);
        const float r = delay; delay = a; return r;
    }
    void reset() { *this = Hilbert(); }
};

// ------------------------------------------------------------------------------------------------
struct Ramp                                    // linear ramp to a target (like line~)
{
    float cur = 0.f, target = 0.f, step = 0.f;
    void snap (float v) { cur = target = v; step = 0.f; }
    void set (float t, int samples)
    {
        if (t == target) return;
        target = t;
        step = samples > 0 ? (t - cur) / (float) samples : t - cur;
    }
    inline float next()
    {
        if (step != 0.f)
        {
            cur += step;
            if ((step > 0.f && cur >= target) || (step < 0.f && cur <= target)) { cur = target; step = 0.f; }
        }
        return cur;
    }
};

struct Slide                                   // asymmetric smoother (slide~): times in samples
{
    float y = 0.f;
    inline float process (float x, float up, float down)
    {
        const float s = (x > y) ? up : down;
        y += (x - y) / std::max (1.f, s);
        return y;
    }
};

struct Params
{
    float crossover[3] = { 200.f, 1000.f, 5000.f };
    float target[kBands] = { 0.5f, 0.5f, 0.5f, 0.5f };    // phase target slider
    float amount[kBands] = { 0.f, 0.f, 0.f, 0.f };         // dynamic amount dial
    float attack[kBands] = { 0.f, 0.f, 0.f, 0.f };         // seconds, 0..2
    float decay[kBands]  = { 0.f, 0.f, 0.f, 0.f };         // seconds, 0..2
    bool  solo[kBands]   = { false, false, false, false };
    bool  bypass = false;
    bool  gonioOn = true;
};

// ------------------------------------------------------------------------------------------------
// One band: dynamic width stage (mid/side + Hilbert quadrature) followed by output phase measurement.
struct BandProc
{
    double fs = 44100.0;
    Hilbert hM, hS;
    float pM = 0.f, pS = 0.f;                  // smoothed mid / side energy (5 Hz one-pole)
    float oLL = 0.f, oLR = 0.f, oRR = 0.f;     // output measurement (5 Hz one-pole)
    float measCoef = 0.f;
    Ramp rGM, rGH, rK, rV, rWet;
    Slide sGM, sGH, sGS;
    int rampSamples = 0;
    long sleepCounter = -1;                    // samples until the stage goes back to sleep (-1 = not counting)
    bool active = false;
    float inPhase = 0.5f, outPhase = 0.5f;     // last readings

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        measCoef = 1.f - std::exp (-2.f * 3.14159265f * 5.f / (float) fs);
        rampSamples = (int) (0.03 * fs);
        reset();
    }
    void reset()
    {
        hM.reset(); hS.reset();
        pM = pS = oLL = oLR = oRR = 0.f;
        rGM.snap (1.f); rGH.snap (0.f); rK.snap (1.f); rV.snap (0.f); rWet.snap (0.f);
        sGM.y = 1.f; sGH.y = 0.f; sGS.y = 1.f;
        sleepCounter = -1; active = false;
        inPhase = outPhase = 0.5f;
    }

    // d = effective dynamic control 0..1 (0.5 = no effect)
    void setControl (float d, float attackS, float decayS)
    {
        d = clampf (d, 0.f, 1.f);
        const float u = std::max (0.f, 2.f * d - 1.f);
        const float v = std::max (0.f, 1.f - 2.f * d);
        rGM.set (std::sqrt (1.f - u), rampSamples);
        rGH.set (std::sqrt (u), rampSamples);
        rK.set (1.f - v, rampSamples);
        rV.set (v, rampSamples);

        const bool nowActive = std::abs (d - 0.5f) > 0.0001f;
        if (nowActive)
        {
            sleepCounter = -1;
            if (! active) rWet.set (1.f, rampSamples);
            else rWet.set (1.f, rampSamples);
        }
        else if (active)
        {
            // wait for the attack / decay tails to die away (5 x the longer time + 50 ms), then fade to dry
            sleepCounter = (long) ((5.0 * std::max (attackS, decayS) + 0.05) * fs);
        }
        active = nowActive;
        attackSamples = std::max (1.f, attackS * (float) fs);
        decaySamples = std::max (1.f, decayS * (float) fs);
    }

    inline void process (float& l, float& r)
    {
        const float dryL = l, dryR = r;
        const float M = 0.5f * (l + r), S = 0.5f * (l - r);
        float mr, mi;
        hM.process (M, mr, mi);
        const float sr = hS.processReal (S);

        pM += measCoef * (M * M - pM);
        pS += measCoef * (S * S - pS);

        const float gMt = rGM.next(), gHt = rGH.next(), k = rK.next(), v = rV.next();
        const float gSt = std::sqrt (std::max (0.f, k * (pM + 1e-9f) / (pM + v * pS + 1e-9f)));
        const float gS = sGS.process (gSt, decaySamples, attackSamples);   // neutral = 1: engaging = falling
        const float gM = sGM.process (gMt, decaySamples, attackSamples);
        const float gH = sGH.process (gHt, attackSamples, decaySamples);   // neutral = 0: engaging = rising

        const float Mg = mr * gM, Hg = mi * gH, Sg = sr * gS;
        const float sp = Sg + Hg;
        const float wl = Mg + sp, wr = Mg - sp;

        if (sleepCounter > 0 && --sleepCounter == 0) { rWet.set (0.f, rampSamples); sleepCounter = -1; }
        const float wet = rWet.next();
        l = wl * wet + dryL * (1.f - wet);
        r = wr * wet + dryR * (1.f - wet);

        // measurements
        inPhase = (pS + 5e-10f) / (pM + pS + 1e-9f);
        oLL += measCoef * (l * l - oLL);
        oLR += measCoef * (l * r - oLR);
        oRR += measCoef * (r * r - oRR);
        const float rr = oLR / (std::sqrt (oLL * oRR) + 1e-9f);
        outPhase = clampf (0.5f * (1.f - rr), 0.f, 1.f);
    }

    float attackSamples = 1.f, decaySamples = 1.f;
};

// ------------------------------------------------------------------------------------------------
class Engine
{
public:
    // readings for the user interface (written by the audio thread, read by the UI thread)
    std::atomic<float> inPhase[kBands], outPhase[kBands];
    std::atomic<float> peakIn[2], peakOut[2];
    std::atomic<int> gonioPos[kBands];
    std::array<std::array<float, kGonioSize * 2>, kBands> gonio {};   // x, y pairs

    Engine()
    {
        for (int b = 0; b < kBands; ++b) { inPhase[b] = 0.5f; outPhase[b] = 0.5f; gonioPos[b] = 0; }
        for (int c = 0; c < 2; ++c) { peakIn[c] = 0.f; peakOut[c] = 0.f; }
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& b : bands) b.prepare (fs);
        for (int c = 0; c < 2; ++c) for (int f = 0; f < 3; ++f) for (int s = 0; s < 2; ++s) lp[c][f][s].reset();
        for (int f = 0; f < 3; ++f) { smoothF[f] = 0.f; }
        soloGain.fill (Ramp{});
        for (auto& r : soloGain) r.snap (1.f);
        bypassRamp.snap (0.f);
        firstBlock = true;
    }

    void process (float* L, float* R, int n, const Params& p)
    {
        // crossover frequencies, kept in order and below Nyquist
        float f[3] = { p.crossover[0], p.crossover[1], p.crossover[2] };
        const float fmax = (float) (0.45 * fs);
        f[0] = clampf (f[0], 20.f, fmax);
        f[1] = clampf (std::max (f[1], f[0] * 1.01f), 20.f, fmax);
        f[2] = clampf (std::max (f[2], f[1] * 1.01f), 20.f, fmax);
        if (firstBlock) { for (int i = 0; i < 3; ++i) smoothF[i] = f[i]; firstBlock = false; }

        // control per band
        for (int b = 0; b < kBands; ++b)
        {
            const float d = 0.5f + (p.target[b] - 0.5f) * p.amount[b];
            bands[(size_t) b].setControl (d, p.attack[b], p.decay[b]);
        }
        bool anySolo = false;
        for (bool s : p.solo) anySolo = anySolo || s;
        const int rs = (int) (0.02 * fs);
        for (int b = 0; b < kBands; ++b) soloGain[(size_t) b].set ((p.solo[(size_t) b] || ! anySolo) ? 1.f : 0.f, rs);
        bypassRamp.set (p.bypass ? 1.f : 0.f, rs);

        float pkIn[2] = { 0, 0 }, pkOut[2] = { 0, 0 };
        float a1[3], a2[3], a3[3];
        const float smoothCoef = 1.f - std::exp (-32.f / (0.02f * (float) fs));

        for (int i = 0; i < n; ++i)
        {
            if ((i & 31) == 0)
            {
                for (int k = 0; k < 3; ++k)
                {
                    smoothF[k] += smoothCoef * (f[k] - smoothF[k]);
                    Lp2::coeffs (smoothF[k], fs, a1[k], a2[k], a3[k]);
                }
            }
            const float in[2] = { L[i], R[i] };
            pkIn[0] = std::max (pkIn[0], std::abs (in[0]));
            pkIn[1] = std::max (pkIn[1], std::abs (in[1]));

            float lpv[2][3];
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < 3; ++k)
                {
                    float y = lp[c][k][0].process (in[c], a1[k], a2[k], a3[k]);
                    lpv[c][k] = lp[c][k][1].process (y, a1[k], a2[k], a3[k]);
                }
            float bl[kBands] = { lpv[0][0], lpv[0][1] - lpv[0][0], lpv[0][2] - lpv[0][1], in[0] - lpv[0][2] };
            float br[kBands] = { lpv[1][0], lpv[1][1] - lpv[1][0], lpv[1][2] - lpv[1][1], in[1] - lpv[1][2] };

            float sumL = 0.f, sumR = 0.f;
            for (int b = 0; b < kBands; ++b)
            {
                float l = bl[b], r = br[b];
                bands[(size_t) b].process (l, r);
                if (p.gonioOn && (i & 1) == 0)                      // goniometer feed: every 2nd sample
                {
                    int pos = gonioPos[b].load (std::memory_order_relaxed);
                    gonio[(size_t) b][(size_t) pos * 2] = (l - r) * 0.70710678f;
                    gonio[(size_t) b][(size_t) pos * 2 + 1] = (l + r) * 0.70710678f;
                    gonioPos[b].store ((pos + 1) % kGonioSize, std::memory_order_release);
                }
                const float g = soloGain[(size_t) b].next();
                sumL += l * g; sumR += r * g;
            }
            const float by = bypassRamp.next();
            const float oL = sumL * (1.f - by) + in[0] * by;
            const float oR = sumR * (1.f - by) + in[1] * by;
            L[i] = oL; R[i] = oR;
            pkOut[0] = std::max (pkOut[0], std::abs (oL));
            pkOut[1] = std::max (pkOut[1], std::abs (oR));
        }

        for (int b = 0; b < kBands; ++b)
        {
            inPhase[b].store (bands[(size_t) b].inPhase);
            outPhase[b].store (bands[(size_t) b].outPhase);
        }
        for (int c = 0; c < 2; ++c)
        {
            peakIn[c].store (std::max (peakIn[c].load(), pkIn[c]));      // the UI reads and resets these
            peakOut[c].store (std::max (peakOut[c].load(), pkOut[c]));
        }
    }

private:
    double fs = 44100.0;
    std::array<BandProc, kBands> bands;
    Lp2 lp[2][3][2];
    float smoothF[3] = { 200.f, 1000.f, 5000.f };
    std::array<Ramp, kBands> soloGain;
    Ramp bypassRamp;
    bool firstBlock = true;
};
} // namespace pw
