#pragma once
#include <vector>
#include <memory>
#include <functional>

// One drawn note. Times are seconds from the start of the captured audio.
struct GraphNote
{
    double start = 0.0, length = 0.5;
    int pitch = 60;                       // MIDI note the vocal should sing
    float transition = 35.0f;             // ms glide into the note
    float vibrato = 40.0f;                // % of the original fast pitch movement (vibrato) that is kept
    float formant = 0.0f;                 // semitones
    float fine = 0.0f;                    // cents
    float flatten = 100.0f;               // % of the slow pitch drift that is removed inside the note
};

// Detected pitch of the captured audio: f0[i] in Hz at time firstSec + i * hopSec (0 = unvoiced).
struct PitchTrack
{
    double hopSec = 0.0, firstSec = 0.0;
    std::vector<float> f0;
};

namespace dsp
{
    using CancelFn = std::function<bool()>;

    // YIN pitch detection on a mono signal. Returns nullptr if cancelled.
    std::shared_ptr<PitchTrack> analysePitch (const std::vector<float>& x, double sampleRate, const CancelFn& cancelled);

    // Pitch-synchronous overlap-add: makes the voice follow the drawn notes. Returns false if cancelled.
    bool renderCorrected (const std::vector<float>& x, double sampleRate, const PitchTrack& track,
                          const std::vector<GraphNote>& notes, std::vector<float>& out, const CancelFn& cancelled);
}
