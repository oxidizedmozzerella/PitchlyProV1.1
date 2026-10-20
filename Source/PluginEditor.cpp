#include "PluginEditor.h"
using namespace juce;
using namespace ui;

static String fmt1 (double v, const char* unit) { return String (v, 1) + " " + unit; }

GraphEditor::GraphEditor (GraphProcessor& p)
    : AudioProcessorEditor (&p), proc (p), roll (p),
      transition ("Transition", true, "Smooth \xe2\x86\x92 Instant", [] (double v) { return String ((int) std::round (v)) + " ms"; }),
      vibrato    ("Vibrato", false, {}, [] (double v) { return String ((int) std::round (v)) + "%"; }),
      formant    ("Formant", false, {}, [] (double v) { return fmt1 (v, "st"); }),
      fine       ("Fine Tune", false, {}, [] (double v) { return String ((int) std::round (v)) + " ct"; }),
      flatten    ("Flatten", false, {}, [] (double v) { return String ((int) std::round (v)) + "%"; })
{
    setLookAndFeel (&laf);
    setSize (1200, 700);

    addAndMakeVisible (roll);
    roll.onSelectionChanged = [this] { syncKnobs(); };

    // header
    statusPill.prefix = "STATUS";
    statusPill.value = [this]
    {
        const double secs = proc.capturedSamples() / jmax (1.0, proc.captureRate());
        if (proc.isArmed()) return String ("Armed \xc2\xb7 press play");
        if (proc.workerState() == 1) return String ("Analysing\xe2\x80\xa6");
        if (proc.workerState() == 2) return String ("Rendering\xe2\x80\xa6");
        if (proc.isCapturing()) return "Recording \xc2\xb7 " + String (secs, 1) + " s";
        return proc.capturedSamples() > 0 ? "Captured \xc2\xb7 " + String (secs, 1) + " s" : String ("Empty");
    };
    correctPill.prefix = "CORRECTION"; correctPill.value = [this] { return String (idx ("correct") ? "On" : "Off"); };
    correctPill.highlighted = [this] { return idx ("correct") != 0; };
    correctPill.onClick = [this] { toggle ("correct"); };
    capturePill.prefix = "CAPTURE"; capturePill.centred = true;
    capturePill.highlighted = [this] { return proc.isCapturing(); };
    capturePill.onClick = [this] { if (proc.isCapturing()) proc.stopCapture(); else proc.startCapture(); repaint(); };

    // snapping bar
    keyPill.prefix = "KEY";     keyPill.value   = [this] { return GraphProcessor::keyNames()[idx ("key")]; };
    scalePill.prefix = "SCALE"; scalePill.value = [this] { return GraphProcessor::scaleNames()[idx ("scale")]; };
    gridPill.prefix = "GRID";   gridPill.value  = [this] { return GraphProcessor::gridNames()[idx ("grid")]; };
    snapPill.prefix = "SNAP";   snapPill.value  = [this] { return String (idx ("snap") ? "On" : "Off"); };
    wavePill.prefix = "WAVEFORM"; wavePill.value = [this] { return String (idx ("wave") ? "Show" : "Hide"); };
    keyPill.onClick   = [this] { choice (keyPill, "key", GraphProcessor::keyNames()); };
    scalePill.onClick = [this] { choice (scalePill, "scale", GraphProcessor::scaleNames()); };
    gridPill.onClick  = [this] { choice (gridPill, "grid", GraphProcessor::gridNames()); };
    snapPill.onClick  = [this] { toggle ("snap"); };
    wavePill.onClick  = [this] { toggle ("wave"); };
    snapPill.highlighted = [this] { return idx ("snap") != 0; };
    for (auto* c : { &statusPill, &correctPill, &capturePill, &keyPill, &scalePill, &gridPill, &snapPill, &wavePill }) addAndMakeVisible (*c);

    // tools
    const char* names[4] = { "DRAW", "SELECT", "ERASE", "SPLIT" };
    for (int i = 0; i < 4; ++i)
    {
        toolTabs[i].label = names[i]; toolTabs[i].onClick = [this, i] { setTool (i); };
        addAndMakeVisible (toolTabs[i]);
    }
    setTool (PianoRoll::Draw);

    // per-note knobs
    struct K { Knob* k; double lo, hi, step, def; bool bipolar; double skew; };
    for (auto& d : { K { &transition, 0, 200, 1, 35, false, 0.4 }, K { &vibrato, 0, 100, 1, 40, false, 1 },
                     K { &formant, -6, 6, 0.1, 0, true, 1 },       K { &fine, -50, 50, 1, 0, true, 1 },
                     K { &flatten, 0, 100, 1, 100, false, 1 } })
    {
        d.k->slider.setRange (d.lo, d.hi, d.step);
        if (d.skew != 1) d.k->slider.setSkewFactor (d.skew);
        d.k->slider.setDoubleClickReturnValue (true, d.def);
        d.k->slider.setValue (d.def, dontSendNotification);
        if (d.bipolar) d.k->slider.getProperties().set ("bipolar", true);
        auto* kp = d.k;
        kp->slider.onValueChange = [this, kp] { kp->repaint(); applyKnobs(); };
        addAndMakeVisible (*kp);
    }
    syncKnobs();
    startTimerHz (10);
}

GraphEditor::~GraphEditor() { setLookAndFeel (nullptr); }

void GraphEditor::setTool (int t)
{
    for (int i = 0; i < 4; ++i) { toolTabs[i].selected = (i == t); toolTabs[i].repaint(); }
    roll.setTool ((PianoRoll::Tool) t);
}

void GraphEditor::choice (Pill& anchor, const String& id, const StringArray& items)
{
    PopupMenu m;
    for (int i = 0; i < items.size(); ++i) m.addItem (i + 1, items[i], true, i == idx (id.toRawUTF8()));
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&anchor), [this, id, n = items.size()] (int r)
    {
        if (r <= 0) return;
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 ((float) (r - 1)));
        repaint(); roll.repaint();
    });
}

void GraphEditor::toggle (const String& id)
{
    if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (idx (id.toRawUTF8()) ? 0.0f : 1.0f);
    repaint(); roll.repaint();
}

void GraphEditor::syncKnobs()
{
    auto* n = roll.selectedNote();
    const bool on = n != nullptr;
    for (auto* k : { &transition, &vibrato, &formant, &fine, &flatten }) { k->setEnabled (on); k->setAlpha (on ? 1.0f : 0.4f); }
    if (on)
    {
        transition.slider.setValue (n->transition, dontSendNotification); vibrato.slider.setValue (n->vibrato, dontSendNotification);
        formant.slider.setValue (n->formant, dontSendNotification);       fine.slider.setValue (n->fine, dontSendNotification);
        flatten.slider.setValue (n->flatten, dontSendNotification);
    }
    for (auto* k : { &transition, &vibrato, &formant, &fine, &flatten }) k->repaint();
}

void GraphEditor::applyKnobs()
{
    if (auto* n = roll.selectedNote())
    {
        n->transition = (float) transition.slider.getValue(); n->vibrato = (float) vibrato.slider.getValue();
        n->formant = (float) formant.slider.getValue();       n->fine = (float) fine.slider.getValue();
        n->flatten = (float) flatten.slider.getValue();
        proc.notesChanged();
    }
}

void GraphEditor::timerCallback()
{
    capturePill.prefix = proc.isCapturing() ? "STOP" : "CAPTURE";
    correctPill.repaint();
    statusPill.repaint(); capturePill.repaint();
}

void GraphEditor::resized()
{
    roll.setBounds (rollPanel.getX() + 20, rollPanel.getY() + 46, 780, 400);
    statusPill.setBounds (getWidth() - 28 - 120 - 14 - 240, 26, 240, 40);
    capturePill.setBounds (getWidth() - 28 - 120, 26, 120, 40);
    correctPill.setBounds (getWidth() - 28 - 120 - 14 - 240 - 14 - 170, 26, 170, 40);

    int x = snapPanel.getRight() - 20;
    for (auto [pill, w] : { std::pair<Pill*, int> { &wavePill, 150 }, { &snapPill, 104 }, { &gridPill, 104 }, { &scalePill, 150 }, { &keyPill, 96 } })
    { pill->setBounds (x - w, snapPanel.getCentreY() - 20, w, 40); x -= w + 12; }

    for (int i = 0; i < 4; ++i) toolTabs[i].setBounds (tabPanel.getX() + 5 + i * (tabPanel.getWidth() - 10) / 4, tabPanel.getY() + 4, (tabPanel.getWidth() - 10) / 4, tabPanel.getHeight() - 8);
    transition.setBounds (transPanel); vibrato.setBounds (k1); formant.setBounds (k2); fine.setBounds (k3); flatten.setBounds (k4);
}

void GraphEditor::paint (Graphics& g)
{
    g.fillAll (bg);
    // logo + title
    const Rectangle<float> logo (28.0f, 26.0f, 38.0f, 38.0f);
    g.setColour (accent); g.fillRoundedRectangle (logo, 10.0f);
    Path step; step.startNewSubPath (37, 55); step.lineTo (43, 55); step.lineTo (43, 49); step.lineTo (49, 49); step.lineTo (49, 43); step.lineTo (57, 43);
    g.setColour (bg); g.strokePath (step, PathStrokeType (3.0f, PathStrokeType::mitered));
    g.setColour (text); g.setFont (F (18.0f, true, 0.06f)); g.drawText ("PITCHLY", 80, 24, 300, 24, Justification::left);
    g.setColour (mute); g.setFont (F (9.0f, false, 0.16f)); g.drawText ("GRAPH MODE PITCH EDITOR", 80, 46, 300, 14, Justification::left);

    for (auto r : { rollPanel, snapPanel, tabPanel, transPanel, k1, k2, k3, k4 }) drawPanel (g, r.toFloat(), 16.0f);

    g.setColour (mute); g.setFont (F (10.0f, false, 0.16f));
    g.drawText ("PITCH EDITOR", rollPanel.getX() + 20, rollPanel.getY() + 14, 200, 16, Justification::left);
    g.drawText ("NOTE SNAPPING", snapPanel.getX() + 20, snapPanel.getCentreY() - 8, 200, 16, Justification::left);

    // legend
    struct L { const char* t; Colour c; };
    int lx = rollPanel.getRight() - 20;
    for (auto l : { L { "DRAWN NOTES", accent }, L { "ORIGINAL PITCH", Colour (0xffaab4c6) }, L { "WAVEFORM", Colour (0xff8d97a8) } })
    {
        const int w = (int) textWidth (F (10.0f, false, 0.16f), l.t) + 6;
        g.setColour (mute); g.drawText (l.t, lx - w, rollPanel.getY() + 14, w, 16, Justification::right);
        g.setColour (l.c); g.fillEllipse ((float) (lx - w - 12), (float) rollPanel.getY() + 18.5f, 7.0f, 7.0f);
        lx -= w + 30;
    }
}
