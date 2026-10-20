#include "Dsp.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
int main()
{
    const double sr = 44100; const int n = (int) (4 * sr);
    std::vector<float> x (n);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double f = 220.0 * std::pow (2.0, (30.0 * std::sin (2 * M_PI * 5.5 * t) - 60.0) / 1200.0);   // ~220 Hz, slightly flat, +-30 ct vibrato
        ph += f / sr; double s = 0;
        for (int h = 1; h <= 12; ++h) s += std::sin (2 * M_PI * ph * h) / h;
        x[(size_t) i] = (float) (0.3 * s * (t > 0.2 && t < 3.8 ? 1.0 : 0.0));
    }
    auto never = [] { return false; };
    auto tr = dsp::analysePitch (x, sr, never);
    auto med = [&] (const PitchTrack& p, double a, double b)
    {
        std::vector<double> v; for (size_t i = 0; i < p.f0.size(); ++i) { double t = p.firstSec + i * p.hopSec; if (t > a && t < b && p.f0[i] > 0) v.push_back (p.f0[i]); }
        if (v.empty()) return std::make_pair (0.0, 0.0);
        std::sort (v.begin(), v.end()); double m = v[v.size() / 2], s = 0;
        for (double q : v) s += std::pow (1200 * std::log2 (q / m), 2);
        return std::make_pair (m, std::sqrt (s / v.size()));
    };
    auto a = med (*tr, 0.5, 3.5);
    std::printf ("input  median %.1f Hz (expect ~%.1f), spread %.1f ct\n", a.first, 220 * std::pow (2.0, -60 / 1200.0), a.second);

    std::vector<GraphNote> notes (2);
    notes[0].start = 0.8; notes[0].length = 1.2; notes[0].pitch = 62; notes[0].vibrato = 0; notes[0].flatten = 100; notes[0].transition = 30;   // D4 = 293.66
    notes[1].start = 2.2; notes[1].length = 1.2; notes[1].pitch = 57; notes[1].vibrato = 100; notes[1].flatten = 0; notes[1].transition = 30;   // A3 = 220.00 keep vibrato
    std::vector<float> y;
    bool ok = dsp::renderCorrected (x, sr, *tr, notes, y, never);
    auto t2 = dsp::analysePitch (y, sr, never);
    auto b = med (*t2, 1.0, 1.9), c = med (*t2, 2.4, 3.3), o = med (*t2, 0.3, 0.7);
    std::printf ("render ok=%d\nnote1 D4: median %.1f Hz (expect 293.7), spread %.1f ct (expect small)\n", ok, b.first, b.second);
    std::printf ("note2 A3: median %.1f Hz (expect 220.0), spread %.1f ct (vibrato kept ~ +-30ct -> ~21)\n", c.first, c.second);
    std::printf ("outside note: median %.1f Hz (expect unchanged ~%.1f)\n", o.first, a.first);
    double mx = 0; for (int i = 0; i < (int) (0.7 * sr); ++i) mx = std::max (mx, (double) std::fabs (y[(size_t) i] - x[(size_t) i]));
    std::printf ("max diff before first note: %g (expect 0)\n", mx);
    double rx = 0, ry = 0; for (int i = (int) (1.0 * sr); i < (int) (1.9 * sr); ++i) { rx += x[(size_t) i] * x[(size_t) i]; ry += y[(size_t) i] * y[(size_t) i]; }
    std::printf ("loudness ratio in note1: %.2f (expect ~1)\n", std::sqrt (ry / rx));
}
