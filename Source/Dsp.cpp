#include "Dsp.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    inline double smooth01 (double t) { t = std::min (1.0, std::max (0.0, t)); return t * t * (3.0 - 2.0 * t); }
    inline double centsOf (double hz) { return 6900.0 + 1200.0 * std::log2 (hz / 440.0); }
}

std::shared_ptr<PitchTrack> dsp::analysePitch (const std::vector<float>& x, double sr, const CancelFn& cancelled)
{
    auto pt = std::make_shared<PitchTrack>();
    const int n = (int) x.size();
    const int factor = std::max (1, (int) std::lround (sr / 11025.0));
    const double sr2 = sr / factor;
    const int m = n / factor;
    const int tauMin = std::max (2, (int) std::floor (sr2 / 800.0));
    const int tauMax = (int) std::ceil (sr2 / 70.0);
    const int W = tauMax + 96, hop2 = 64;
    pt->hopSec = hop2 * (double) factor / sr;
    pt->firstSec = (W / 2.0) * factor / sr;
    if (m < W + tauMax + hop2) return pt;

    std::vector<float> ds ((size_t) m);
    for (int i = 0; i < m; ++i)
    {
        float s = 0.0f;
        for (int j = 0; j < factor; ++j) s += x[(size_t) (i * factor + j)];
        ds[(size_t) i] = s / (float) factor;
    }

    const int frames = (m - W - tauMax) / hop2 + 1;
    pt->f0.assign ((size_t) frames, 0.0f);
    std::vector<double> d ((size_t) tauMax + 2), cm ((size_t) tauMax + 2);

    for (int f = 0; f < frames; ++f)
    {
        if ((f & 63) == 0 && cancelled()) return nullptr;
        const float* a = ds.data() + (size_t) f * hop2;
        double e = 0.0;
        for (int j = 0; j < W; ++j) e += (double) a[j] * a[j];
        if (std::sqrt (e / W) < 0.004) continue;                       // too quiet

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double acc = 0.0;
            for (int j = 0; j < W; ++j) { const double df = (double) a[j] - a[j + tau]; acc += df * df; }
            d[(size_t) tau] = acc;
        }
        cm[0] = 1.0;
        double run = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau) { run += d[(size_t) tau]; cm[(size_t) tau] = run > 0.0 ? d[(size_t) tau] * tau / run : 1.0; }

        int found = 0;
        for (int t = tauMin; t < tauMax; ++t)
            if (cm[(size_t) t] < 0.15) { while (t + 1 < tauMax && cm[(size_t) t + 1] < cm[(size_t) t]) ++t; found = t; break; }
        if (found == 0)
        {
            int best = tauMin;
            for (int t = tauMin; t < tauMax; ++t) if (cm[(size_t) t] < cm[(size_t) best]) best = t;
            if (cm[(size_t) best] < 0.3) found = best;
        }
        if (found == 0) continue;

        double tauf = found;
        if (found > 1 && found < tauMax)
        {
            const double p = cm[(size_t) found - 1], q = cm[(size_t) found], r = cm[(size_t) found + 1], den = p - 2.0 * q + r;
            if (den != 0.0) tauf += std::max (-1.0, std::min (1.0, 0.5 * (p - r) / den));
        }
        pt->f0[(size_t) f] = (float) (sr2 / tauf);
    }

    // 3-point median clean-up of isolated glitches
    auto g = pt->f0;
    for (int i = 1; i + 1 < frames; ++i)
    {
        const float p = pt->f0[(size_t) i - 1], q = pt->f0[(size_t) i], r = pt->f0[(size_t) i + 1];
        if (p > 0 && q > 0 && r > 0) g[(size_t) i] = std::max (std::min (p, q), std::min (std::max (p, q), r));
    }
    pt->f0 = g;
    return pt;
}

bool dsp::renderCorrected (const std::vector<float>& x, double sr, const PitchTrack& pt,
                           const std::vector<GraphNote>& notes, std::vector<float>& out, const CancelFn& cancelled)
{
    const int n = (int) x.size(), nF = (int) pt.f0.size();
    out = x;
    if (n == 0 || nF == 0 || notes.empty() || pt.hopSec <= 0.0) return true;

    auto timeOf = [&] (int i) { return pt.firstSec + i * pt.hopSec; };
    auto frameAt = [&] (double sec) { return std::max (0, std::min (nF - 1, (int) std::lround ((sec - pt.firstSec) / pt.hopSec))); };

    // ---- 1. per-frame pitch ratio and formant ratio from the drawn notes ----
    std::vector<float> ratio ((size_t) nF, 1.0f), fratio ((size_t) nF, 1.0f);
    std::vector<double> cs ((size_t) nF, -1.0);                      // median-smoothed cents, -1 = unvoiced
    for (int i = 0; i < nF; ++i)
    {
        if (pt.f0[(size_t) i] <= 0.0f) continue;
        double v[5]; int k = 0;
        for (int j = std::max (0, i - 2); j <= std::min (nF - 1, i + 2); ++j) if (pt.f0[(size_t) j] > 0.0f) v[k++] = centsOf (pt.f0[(size_t) j]);
        std::sort (v, v + k);
        cs[(size_t) i] = v[k / 2];
    }

    for (const auto& note : notes)
    {
        const double t0 = note.start, t1 = note.start + note.length;
        const int i0 = std::max (0, (int) std::ceil ((t0 - pt.firstSec) / pt.hopSec)), i1 = std::min (nF - 1, (int) std::floor ((t1 - pt.firstSec) / pt.hopSec));
        std::vector<int> vi;
        for (int i = i0; i <= i1; ++i) if (cs[(size_t) i] >= 0.0) vi.push_back (i);
        if (vi.empty()) continue;

        const double target = note.pitch * 100.0 + note.fine;
        std::vector<double> dev (vi.size()), slow (vi.size());
        for (size_t a = 0; a < vi.size(); ++a) dev[a] = cs[(size_t) vi[a]] - target;
        const double win = 0.075;
        double meanSlow = 0.0;
        for (size_t a = 0; a < vi.size(); ++a)
        {
            double s = 0.0; int c = 0;
            for (size_t b = 0; b < vi.size(); ++b) if (std::fabs (timeOf (vi[b]) - timeOf (vi[a])) <= win) { s += dev[b]; ++c; }
            slow[a] = s / std::max (1, c);
            meanSlow += slow[a];
        }
        meanSlow /= (double) vi.size();

        const double vib = note.vibrato / 100.0, flat = note.flatten / 100.0;
        const double trIn = std::max (0.0, note.transition / 1000.0), trOut = std::min (trIn, 0.03);
        for (size_t a = 0; a < vi.size(); ++a)
        {
            const int i = vi[a];
            const double t = timeOf (i);
            const double wIn = trIn > 0.0 ? smooth01 ((t - t0) / trIn) : 1.0, wOut = trOut > 0.0 ? smooth01 ((t1 - t) / trOut) : 1.0;
            const double w = std::min (wIn, wOut);
            const double outDev = vib * (dev[a] - slow[a]) + (1.0 - flat) * (slow[a] - meanSlow);
            const double T = target + outDev;                              // where the pitch should be
            const double shiftCents = (T - cs[(size_t) i]) * w;
            ratio[(size_t) i] = (float) std::min (4.0, std::max (0.25, std::pow (2.0, shiftCents / 1200.0)));
            fratio[(size_t) i] = (float) std::pow (2.0, note.formant / 12.0 * w);
        }
        // unvoiced frames inside the note: keep the formant shift so consonants don't flip timbre
        for (int i = i0; i <= i1; ++i) if (cs[(size_t) i] < 0.0) fratio[(size_t) i] = (float) std::pow (2.0, note.formant / 12.0);
    }

    // ---- 2. pitch marks (one per period, snapped to waveform peaks) ----
    std::vector<int> marks; std::vector<float> periods;
    const double unvoicedHop = 0.008 * sr;
    {
        double p = 0.0;
        while (p < n)
        {
            if (cancelled()) return false;
            const float f0 = pt.f0[(size_t) frameAt (p / sr)];
            const double P = f0 > 0.0f ? sr / f0 : unvoicedHop;
            int pos = (int) p;
            if (f0 > 0.0f)
            {
                const int r = (int) (P * 0.2), lo = std::max (0, pos - r), hi = std::min (n - 1, pos + r);
                float best = -1e9f;
                for (int i = lo; i <= hi; ++i) if (x[(size_t) i] > best) { best = x[(size_t) i]; pos = i; }
                if (! marks.empty() && pos <= marks.back() + (int) (P * 0.5)) pos = marks.back() + (int) (P * 0.9);
            }
            if (pos >= n) break;
            marks.push_back (pos); periods.push_back ((float) P);
            p = pos + P;
        }
    }
    if (marks.empty()) return true;

    // ---- 3. overlap-add synthesis ----
    std::vector<float> acc ((size_t) n, 0.0f), wsum ((size_t) n, 0.0f);
    double ts = marks[0];
    size_t k = 0; int counter = 0;
    while (ts < n)
    {
        if ((++counter & 255) == 0 && cancelled()) return false;
        while (k + 1 < marks.size() && std::fabs (marks[k + 1] - ts) <= std::fabs (marks[k] - ts)) ++k;
        const double P = periods[k];
        const int half = std::max (8, (int) P);
        const int fi = frameAt (ts / sr);
        const double fr = fratio[(size_t) fi], r = ratio[(size_t) fi];
        const int centre = (int) std::lround (ts);
        for (int j = -half; j <= half; ++j)
        {
            const int dst = centre + j;
            if (dst < 0 || dst >= n) continue;
            const double src = marks[k] + j * fr;
            const int s0 = (int) std::floor (src);
            if (s0 < 0 || s0 + 1 >= n) continue;
            const double fracp = src - s0;
            const float s = (float) (x[(size_t) s0] * (1.0 - fracp) + x[(size_t) s0 + 1] * fracp);
            const float w = (float) (0.5 + 0.5 * std::cos (kPi * j / half));
            acc[(size_t) dst] += w * s; wsum[(size_t) dst] += w;
        }
        ts += P / r;
    }

    // ---- 4. mix: corrected audio only around notes, original everywhere else, with short fades ----
    std::vector<float> mask ((size_t) n, 0.0f);
    const int fade = (int) (0.02 * sr);
    for (const auto& note : notes)
    {
        const int a = std::max (0, (int) (note.start * sr)), b = std::min (n, (int) ((note.start + note.length) * sr));
        for (int i = a; i < b; ++i) mask[(size_t) i] = 1.0f;
        for (int j = 1; j <= fade; ++j)
        {
            const float w = (float) j / (float) fade;
            if (a - j >= 0) mask[(size_t) (a - j)] = std::max (mask[(size_t) (a - j)], 1.0f - w);
            if (b + j - 1 < n && b + j - 1 >= 0) mask[(size_t) (b + j - 1)] = std::max (mask[(size_t) (b + j - 1)], 1.0f - w);
        }
    }
    for (int i = 0; i < n; ++i)
    {
        const float c = wsum[(size_t) i] > 0.1f ? acc[(size_t) i] / wsum[(size_t) i] : x[(size_t) i];
        out[(size_t) i] = x[(size_t) i] * (1.0f - mask[(size_t) i]) + c * mask[(size_t) i];
    }
    return true;
}
