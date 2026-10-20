#include "UI.h"
using namespace juce;
using namespace ui;

Font F (float size, bool bold, float kerning)
{
    Font f (FontOptions (size, bold ? Font::bold : Font::plain));
    return kerning > 0.0f ? f.withExtraKerningFactor (kerning) : f;
}
float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga; ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}
void drawPanel (Graphics& g, Rectangle<float> r, float radius)
{
    g.setGradientFill (ColourGradient (panelTop, 0, r.getY(), panelBot, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (line); g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (Colours::white.withAlpha (0.035f));                       // faint top highlight
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
}

// ------------------------------------------------------------------------------------------------
void PitchlyLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const bool hot = s.isMouseOverOrDragging();
    const float r = size * 0.44f - 2.0f, thick = jmax (2.5f, size * 0.055f);
    const PathStrokeType round (thick, PathStrokeType::curved, PathStrokeType::rounded);

    Path track; track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    g.setColour (Colour (0xff252931)); g.strokePath (track, round);

    const float a = start + pos * (end - start);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;          // centred controls fill outward from 12 o'clock
    if (std::abs (a - from) > 0.01f)
    {
        Path val; val.addCentredArc (c.x, c.y, r, r, 0.0f, jmin (from, a), jmax (from, a), true);
        const float glow[2][2] = { { 3.6f, 0.07f }, { 2.2f, 0.15f } };
        for (auto& gl : glow)
        {
            g.setColour (accent.withAlpha (gl[1] * (hot ? 1.7f : 1.0f)));
            g.strokePath (val, PathStrokeType (thick * gl[0], PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (accent); g.strokePath (val, round);
    }

    // metal cap
    const float rc = size * 0.30f;
    const auto cap = Rectangle<float> (rc * 2, rc * 2).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.45f)); g.fillEllipse (cap.translated (0.0f, 2.0f).expanded (1.0f));
    g.setGradientFill (ColourGradient (Colour (hot ? 0xff373b45 : 0xff30343d), c.x, c.y - rc, Colour (0xff16181d), c.x, c.y + rc, false));
    g.fillEllipse (cap);
    g.setColour (Colour (0xff3b404a)); g.drawEllipse (cap.reduced (0.5f), 1.0f);

    // indicator
    const Point<float> dir (std::sin (a), -std::cos (a));
    Path ind; ind.startNewSubPath (c + dir * (rc * 0.38f)); ind.lineTo (c + dir * (rc * 0.88f));
    g.setColour (accent); g.strokePath (ind, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

// ------------------------------------------------------------------------------------------------
Knob::Knob (const String& label, bool wide, const String& subtitle, std::function<String (double)> format)
    : labelText (label), subtitleText (subtitle), isWide (wide), fmt (std::move (format))
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}
void Knob::resized()
{
    if (isWide) { slider.setBounds (10, (getHeight() - 116) / 2, 116, 116); return; }
    const int k = jlimit (56, 104, jmin (getWidth() - 20, getHeight() - 52));
    slider.setBounds ((getWidth() - k) / 2, (getHeight() - (k + 40)) / 2, k, k);
}
void Knob::paint (Graphics& g)
{
    const String v = fmt (slider.getValue());
    if (isWide)
    {
        const int tx = 132, mid = getHeight() / 2;
        g.setColour (mute); g.setFont (F (10.0f, false, 0.16f)); g.drawText (labelText.toUpperCase(), tx, mid - 34, 112, 14, Justification::left);
        g.setColour (ui::text); g.setFont (F (24.0f, true));      g.drawText (v, tx, mid - 19, 112, 34, Justification::left);
        g.setColour (mute); g.setFont (F (11.0f));                g.drawText (subtitleText, tx, mid + 18, 112, 16, Justification::left);
    }
    else
    {
        const int vy = slider.getBottom() + 3;
        g.setColour (ui::text); g.setFont (F (13.0f, true)); g.drawText (v, 0, vy, getWidth(), 16, Justification::centred);
        g.setColour (mute); g.setFont (F (10.0f, false, 0.16f)); g.drawText (labelText.toUpperCase(), 0, vy + 19, getWidth(), 14, Justification::centred);
    }
}

// ------------------------------------------------------------------------------------------------
void Pill::paint (Graphics& g)
{
    const bool hi = highlighted && highlighted();
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (hi ? accent : panelTop); g.fillRoundedRectangle (b, b.getHeight() / 2);
    g.setColour (hi ? accent : line);     g.drawRoundedRectangle (b, b.getHeight() / 2, 1.0f);

    float x = 16.0f;
    const auto pf = F (12.0f, hi), vf = F (12.0f, true);
    g.setColour (hi ? bg : soft); g.setFont (pf);
    if (centred) { g.drawText (prefix, getLocalBounds(), Justification::centred); return; }
    const float pw = textWidth (pf, prefix);
    g.drawText (prefix, Rectangle<float> (x, 0, pw + 4, (float) getHeight()), Justification::centredLeft);
    x += pw + 8.0f;

    if (value)
    {
        const String v = value();
        g.setColour (text); g.setFont (vf);
        g.drawText (v, Rectangle<float> (x, 0, textWidth (vf, v) + 4, (float) getHeight()), Justification::centredLeft);
    }
    if (meter)
    {
        const float mw = (float) meterWidth;
        const Rectangle<float> bar (x, getHeight() * 0.5f - 2.0f, mw, 4.0f);
        g.setColour (line); g.fillRoundedRectangle (bar, 2.0f);
        const float lv = jlimit (0.0f, 1.0f, meter());
        if (lv > 0.01f)
        {
            g.setColour (accent.withAlpha (0.25f)); g.fillRoundedRectangle (bar.withWidth (mw * lv).expanded (0.0f, 2.0f), 3.0f);
            g.setColour (accent);                   g.fillRoundedRectangle (bar.withWidth (mw * lv), 2.0f);
        }
    }
}

void TabButton::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (3.0f, 3.0f);
    if (selected) { g.setColour (accent.withAlpha (0.16f)); g.fillRoundedRectangle (b, 7.0f); }
    else if (isMouseOver()) { g.setColour (Colours::white.withAlpha (0.04f)); g.fillRoundedRectangle (b, 7.0f); }
    g.setColour (selected ? accent : mute); g.setFont (F (10.0f, selected, 0.1f));
    g.drawText (label, getLocalBounds(), Justification::centred);
}
