#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PianoRoll.h"
#include "UI.h"

class GraphEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GraphEditor (GraphProcessor&);
    ~GraphEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void syncKnobs();
    void applyKnobs();
    void setTool (int t);
    void choice (Pill& anchor, const juce::String& id, const juce::StringArray& items);
    void toggle (const juce::String& id);
    int  idx (const char* id) const   { return (int) proc.apvts.getRawParameterValue (id)->load(); }

    GraphProcessor& proc;
    PitchlyLookAndFeel laf;
    PianoRoll roll;

    Pill statusPill, correctPill, capturePill, keyPill, scalePill, gridPill, snapPill, wavePill;
    TabButton toolTabs[4];
    Knob transition, vibrato, formant, fine, flatten;

    const juce::Rectangle<int> rollPanel { 28, 92, 820, 466 }, snapPanel { 28, 576, 820, 98 },
                               tabPanel { 868, 92, 304, 42 }, transPanel { 868, 148, 304, 148 },
                               k1 { 868, 310, 145, 175 }, k2 { 1027, 310, 145, 175 },
                               k3 { 868, 499, 145, 175 }, k4 { 1027, 499, 145, 175 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditor)
};
