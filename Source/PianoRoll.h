#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "UI.h"

// Piano roll: full captured waveform behind it, notes you DRAW (not move-the-detected-notes), Pitchly styling.
class PianoRoll : public juce::Component, private juce::Timer
{
public:
    enum Tool { Draw = 0, Select, Erase, Split };

    explicit PianoRoll (GraphProcessor&);
    ~PianoRoll() override { stopTimer(); }

    void setTool (Tool t)               { tool = t; }
    int  getSelected() const            { return selected; }
    GraphNote* selectedNote()           { return juce::isPositiveAndBelow (selected, (int) proc.notes.size()) ? &proc.notes[(size_t) selected] : nullptr; }
    void deleteSelected();

    std::function<void()> onSelectionChanged;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    static constexpr int keyW = 52, rulerH = 22, topMidi = 69, rows = 20;

    void timerCallback() override;
    float rowH() const                  { return (getHeight() - rulerH) / (float) rows; }
    float yOfMidi (int m) const         { return rulerH + (topMidi - m) * rowH(); }
    int   midiAtY (float y) const       { return juce::jlimit (topMidi - rows + 1, topMidi, topMidi - (int) std::floor ((y - rulerH) / rowH())); }
    float xOfTime (double t) const      { return keyW + (float) ((t - viewStart) * pps); }
    double timeAtX (float x) const      { return viewStart + (x - keyW) / pps; }
    double beatSeconds() const          { return 60.0 / juce::jmax (20.0, proc.getBpm()); }
    double gridBeats() const;
    double gridSeconds() const          { return gridBeats() * beatSeconds(); }
    double beatOf (double t) const      { return proc.getCaptureStartPpq() + t / beatSeconds(); }
    double timeOfBeat (double b) const  { return (b - proc.getCaptureStartPpq()) * beatSeconds(); }
    double snapTime (double t) const;
    bool inScale (int pitchClass) const;
    int snapPitch (int midi) const;
    int hitNote (juce::Point<float> p) const;
    juce::Rectangle<float> noteRect (const GraphNote&) const;
    void select (int i)                 { if (i != selected) { selected = i; if (onSelectionChanged) onSelectionChanged(); } repaint(); }
    void drawWaveform (juce::Graphics&);

    GraphProcessor& proc;
    Tool tool = Draw;
    int selected = -1;
    enum Drag { None, Move, Resize } drag = None;
    double dragOffT = 0.0; int dragOffPitch = 0;

    double viewStart = 0.0, pps = 90.0;       // seconds, pixels per second
    int lastLen = 0, lastVersion = -1; float normPeak = 0.05f;
};
