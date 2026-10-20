# PitchlyGraph, Graph Mode pitch editor (VST3)

Companion to [Pitchly](https://github.com/oxidizedmozzerella/PitchlyV2). Same look, different workflow:
capture a vocal, see its whole waveform and detected pitch on a piano roll, then **draw the notes you want it to sing**.

## Using it in FL Studio
1. Put PitchlyGraph on the vocal's mixer insert (use the x64 build).
2. Click **CAPTURE** (it arms), then press play. It records while the host plays and stops by itself when playback stops or loops.
3. Wait for "Analysing..." to finish. The waveform and the grey original-pitch line appear.
4. **Draw** notes (Draw tool), move/resize them (Select), cut them (Split) or delete them (Erase / Delete key).
   Each note has its own Transition, Vibrato, Formant, Fine Tune and Flatten.
5. Play the same section again with **CORRECTION** on. Edits re-render in the background (about half a second after you stop editing).

## How it works
- YIN pitch detection on the captured audio, then pitch-synchronous overlap-add (PSOLA) re-pitches the voice toward your notes.
- The corrected audio is rendered offline and played back aligned to the host's timeline, so there is no added latency.
- Outside your notes the original audio is untouched.
- Transition = glide time into the note. Vibrato = how much of the original vibrato is kept. Flatten = how much slow pitch drift is removed. Fine Tune in cents. Formant in semitones.

## Notes / limits
- The captured audio is not saved with the project; re-capture after reopening. Your drawn notes and settings are saved.
- The corrected playback is mono (the capture is a mono mix).
- Capture holds up to 3 minutes.

Build: push to GitHub and use Actions (artifacts `PitchlyGraph-VST3-x64` / `-ARM64`), or run `build_windows.bat x64`.
