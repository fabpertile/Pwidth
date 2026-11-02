// Stand-alone tests of the DSP engine (no JUCE needed):  g++ -O2 -std=c++17 -I../Source test_dsp.cpp -o test_dsp && ./test_dsp
#include "Dsp.h"
#include <cstdio>
#include <random>
#include <vector>

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } else { std::printf("ok:   "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void run (pw::Engine& e, std::vector<float>& L, std::vector<float>& R, const pw::Params& p)
{
    const int block = 256;
    for (size_t i = 0; i < L.size(); i += block)
        e.process (L.data() + i, R.data() + i, (int) std::min<size_t> (block, L.size() - i), p);
}

int main()
{
    const double fs = 48000.0;
    const int N = (int) (fs * 3);
    std::mt19937 rng (1);
    std::normal_distribution<float> nd (0.f, 0.2f);

    // 1. Hilbert: the two paths stay 90 degrees apart
    for (double f : { 100.0, 500.0, 2000.0, 8000.0 })
    {
        pw::Hilbert h; double sre = 0, sim = 0, cre = 0, cim = 0; // correlate with sin/cos of f
        float re, im;
        for (int n = 0; n < 48000; ++n)
        {
            const float x = (float) std::sin (2 * M_PI * f * n / fs);
            h.process (x, re, im);
            if (n > 4000) { const double s = std::sin (2 * M_PI * f * n / fs), c = std::cos (2 * M_PI * f * n / fs);
                            sre += re * s; cre += re * c; sim += im * s; cim += im * c; }
        }
        const double pr = std::atan2 (cre, sre), pi_ = std::atan2 (cim, sim);
        double diff = std::fmod (std::abs (pr - pi_) * 180 / M_PI, 360); if (diff > 180) diff = 360 - diff;
        CHECK (std::abs (diff - 90.0) < 3.0, "hilbert paths %.0f Hz differ by %.2f deg", f, diff);
    }

    // 2. neutral settings: output equals input (the four bands sum flat)
    {
        pw::Engine e; e.prepare (fs); pw::Params p;
        std::vector<float> L (N), R (N), L0, R0;
        for (int i = 0; i < N; ++i) { L[i] = nd (rng); R[i] = nd (rng); }
        L0 = L; R0 = R; run (e, L, R, p);
        double err = 0, ref = 0;
        for (int i = 4800; i < N; ++i) { err += (L[i] - L0[i]) * (L[i] - L0[i]) + (R[i] - R0[i]) * (R[i] - R0[i]); ref += L0[i] * L0[i] + R0[i] * R0[i]; }
        CHECK (10 * std::log10 (err / ref + 1e-30) < -90.0, "neutral = unity (error %.1f dB)", 10 * std::log10 (err / ref + 1e-30));
    }

    // 3. uncorrelated noise, target 0 amount 1 -> collapses to mono; output phase ~ 0
    {
        pw::Engine e; e.prepare (fs); pw::Params p;
        for (int b = 0; b < 4; ++b) { p.target[b] = 0.f; p.amount[b] = 1.f; }
        std::vector<float> L (N), R (N);
        for (int i = 0; i < N; ++i) { L[i] = nd (rng); R[i] = nd (rng); }
        run (e, L, R, p);
        for (int b = 0; b < 4; ++b)
            CHECK (e.outPhase[b].load() < 0.08f && e.inPhase[b].load() > 0.3f, "band %d: in %.2f -> out %.2f (collapse to mono)", b + 1, e.inPhase[b].load(), e.outPhase[b].load());
    }

    // 4. mono noise, target 1 amount 1 -> fully out of phase; output phase ~ 1
    {
        pw::Engine e; e.prepare (fs); pw::Params p;
        for (int b = 0; b < 4; ++b) { p.target[b] = 1.f; p.amount[b] = 1.f; }
        std::vector<float> L (N), R;
        for (int i = 0; i < N; ++i) L[i] = nd (rng);
        R = L; run (e, L, R, p);
        for (int b = 0; b < 4; ++b)
            CHECK (e.outPhase[b].load() > 0.92f && e.inPhase[b].load() < 0.08f, "band %d: in %.2f -> out %.2f (mono pushed out of phase)", b + 1, e.inPhase[b].load(), e.outPhase[b].load());
    }

    // 5. mono noise, target 0.75 amount 1 -> somewhere in between (wider than the input)
    {
        pw::Engine e; e.prepare (fs); pw::Params p;
        for (int b = 0; b < 4; ++b) { p.target[b] = 0.75f; p.amount[b] = 1.f; }
        std::vector<float> L (N), R;
        for (int i = 0; i < N; ++i) L[i] = nd (rng);
        R = L; run (e, L, R, p);
        for (int b = 0; b < 4; ++b)
            CHECK (e.outPhase[b].load() > 0.3f && e.outPhase[b].load() < 0.8f, "band %d: mono -> %.2f at target 0.75", b + 1, e.outPhase[b].load());
    }

    // 6. anti-phase noise, amount 0 or target 0.5 -> unchanged
    {
        pw::Engine e; e.prepare (fs); pw::Params p; p.target[0] = 0.5f; p.amount[0] = 1.f;
        std::vector<float> L (N), R (N);
        for (int i = 0; i < N; ++i) { L[i] = nd (rng); R[i] = -L[i]; }
        auto L0 = L; run (e, L, R, p);
        double err = 0, ref = 0; for (int i = 4800; i < N; ++i) { err += (L[i] - L0[i]) * (L[i] - L0[i]); ref += L0[i] * L0[i]; }
        CHECK (10 * std::log10 (err / ref + 1e-30) < -90.0, "anti-phase input untouched at slider 0.5");
    }

    // 7. solo: only the soloed band is heard; bypass returns the dry signal
    {
        pw::Engine e; e.prepare (fs); pw::Params p; p.solo[3] = true;
        std::vector<float> L (N), R (N);
        for (int i = 0; i < N; ++i) { L[i] = std::sin (2 * M_PI * 100 * i / fs) * 0.5f; R[i] = L[i]; }  // 100 Hz sits in band 1
        run (e, L, R, p);
        double rms = 0; for (int i = N / 2; i < N; ++i) rms += L[i] * L[i];
        CHECK (std::sqrt (rms / (N / 2)) < 0.04, "soloing band 4 attenuates a 100 Hz tone by >20 dB (rms %.4f)", std::sqrt (rms / (N / 2)));
        p.solo[3] = false; p.bypass = true;
        for (int i = 0; i < N; ++i) { L[i] = nd (rng); R[i] = nd (rng); }
        auto L0 = L; run (e, L, R, p);
        double err = 0; for (int i = 4800; i < N; ++i) err += (L[i] - L0[i]) * (L[i] - L0[i]);
        CHECK (err < 1e-9, "bypass = dry signal");
    }

    std::printf ("%s (%d failures)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails);
    return fails ? 1 : 0;
}
