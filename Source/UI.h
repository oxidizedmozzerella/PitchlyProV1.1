#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// Pitchly look: same palette, knobs, pills and tabs as the Pitchly autotune plugin.
namespace ui
{
    inline const juce::Colour bg { 0xff0b0c0f }, panelTop { 0xff161920 }, panelBot { 0xff111317 }, line { 0xff23262d },
                              text { 0xffeceef2 }, mute { 0xff7d828d }, soft { 0xffb8bcc5 }, dim { 0xff595e69 },
                              accent { 0xff60a5fa };
}

juce::Font F (float size, bool bold = false, float kerning = 0.0f);
float textWidth (const juce::Font& f, const juce::String& s);
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 14.0f);

class PitchlyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
};

class Knob : public juce::Component
{
public:
    Knob (const juce::String& label, bool wide, const juce::String& subtitle, std::function<juce::String (double)> format);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String labelText, subtitleText; bool isWide; std::function<juce::String (double)> fmt;
};

class Pill : public juce::Component
{
public:
    juce::String prefix;
    std::function<juce::String()> value;
    std::function<float()> meter;
    std::function<bool()> highlighted;
    std::function<void()> onClick;
    int meterWidth = 90;
    bool centred = false;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick && contains (getMouseXYRelative())) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { if (onClick) setMouseCursor (juce::MouseCursor::PointingHandCursor); }
};

class TabButton : public juce::Component
{
public:
    juce::String label; bool selected = false; std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick && contains (getMouseXYRelative())) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
};
