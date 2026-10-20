#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

const StringArray& GraphProcessor::keyNames()   { static const StringArray k { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return k; }
const StringArray& GraphProcessor::scaleNames() { static const StringArray s { "Major", "Minor", "Chromatic" }; return s; }
const StringArray& GraphProcessor::gridNames()  { static const StringArray s { "1/4", "1/8", "1/16", "Off" }; return s; }

AudioProcessorValueTreeState::ParameterLayout GraphProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 },   "Key",   keyNames(),   0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames(), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "grid", 1 },  "Grid",  gridNames(),  1));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "snap", 1 },  "Snap to Scale", true));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "wave", 1 },  "Show Waveform", true));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "correct", 1 }, "Correction", true));
    return { p.begin(), p.end() };
}

// Background thread: pitch analysis after a capture, and re-rendering the corrected audio after note edits.
class GraphProcessor::Worker : public Thread
{
public:
    explicit Worker (GraphProcessor& o) : Thread ("PitchlyGraph worker"), p (o) {}

    void run() override
    {
        std::vector<float> audio;
        double sr = 44100.0;
        int analysedGen = -1;

        while (! threadShouldExit())
        {
            wait (50);
            p.collectGarbage (false);
            const int gen = p.generation.load();

            if (! p.capturing.load() && p.capLen.load() > 0 && gen != analysedGen)
            {
                {
                    const ScopedLock sl (p.captureLock);
                    audio.assign (p.capBuf.begin(), p.capBuf.begin() + p.capLen.load());
                    sr = p.capRate;
                }
                p.wstate = 1;
                auto cancel = [&] { return threadShouldExit() || p.capturing.load() || p.generation.load() != gen; };
                if (auto tr = dsp::analysePitch (audio, sr, cancel))
                {
                    { const ScopedLock sl (p.trackLock); p.track = tr; }
                    p.trackVersion++;
                    analysedGen = gen;
                    p.renderDirty = true;
                    p.lastEditMs = 0;
                }
                p.wstate = 0;
            }

            if (p.renderDirty.load() && analysedGen == p.generation.load() && ! p.capturing.load()
                && (juce::int64) Time::getMillisecondCounter() - p.lastEditMs.load() >= 300)
            {
                p.renderDirty = false;
                std::vector<GraphNote> nn;
                { const ScopedLock sl (p.noteLock); nn = p.noteSnapshot; }
                std::shared_ptr<const PitchTrack> tr;
                { const ScopedLock sl (p.trackLock); tr = p.track; }

                if (nn.empty() || tr == nullptr) { p.publish (nullptr); continue; }

                p.wstate = 2;
                std::vector<float> out;
                auto cancel = [&] { return threadShouldExit() || p.renderDirty.load() || p.generation.load() != gen || p.capturing.load(); };
                if (dsp::renderCorrected (audio, sr, *tr, nn, out, cancel))
                {
                    auto* rb = new RenderBuf();
                    rb->data = std::move (out);
                    rb->startSample = p.capStartSample.load();
                    p.publish (rb);
                }
                p.wstate = 0;
            }
        }
    }

private:
    GraphProcessor& p;
};

GraphProcessor::GraphProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    worker = std::make_unique<Worker> (*this);
    worker->startThread();
}

GraphProcessor::~GraphProcessor()
{
    worker->stopThread (4000);
    worker.reset();
    delete current.exchange (nullptr);
    collectGarbage (true);
}

bool GraphProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

void GraphProcessor::prepareToPlay (double sr, int)
{
    const ScopedLock sl (captureLock);
    capRate = sr;
    const size_t need = (size_t) (sr * kMaxCaptureSeconds);
    if (capBuf.size() < need) { capBuf.assign (need, 0.0f); capLen = 0; }
}

void GraphProcessor::retire (RenderBuf* b)
{
    if (b == nullptr) return;
    const ScopedLock sl (retireLock);
    retired.push_back ({ b, Time::getMillisecondCounter() });
}

void GraphProcessor::collectGarbage (bool all)
{
    const ScopedLock sl (retireLock);
    const auto now = Time::getMillisecondCounter();
    for (size_t i = retired.size(); i-- > 0;)
        if (all || now - retired[i].second > 3000) { delete retired[i].first; retired.erase (retired.begin() + (long) i); }
}

void GraphProcessor::publish (RenderBuf* b) { retire (current.exchange (b, std::memory_order_acq_rel)); }

void GraphProcessor::notesChanged()
{
    { const ScopedLock sl (noteLock); noteSnapshot = notes; }
    lastEditMs = (juce::int64) Time::getMillisecondCounter();
    renderDirty = true;
}

void GraphProcessor::startCapture()
{
    publish (nullptr);                                   // the old render no longer matches
    { const ScopedLock sl (trackLock); track.reset(); }
    trackVersion++;
    capLen.store (0, std::memory_order_release);
    generation++;
    needStamp = true;
    capturing = true;
}

void GraphProcessor::processBlock (AudioBuffer<float>& buf, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    bool playing = false, haveTime = false;
    long long hostSample = 0;
    double ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm.store (*b);
            playing = pos->getIsPlaying();
            if (auto t = pos->getTimeInSamples()) { hostSample = (long long) *t; haveTime = true; }
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }

    const int n = buf.getNumSamples(), ch = buf.getNumChannels();

    // ---- capture (audio passes through untouched) ----
    if (capturing.load())
    {
        int len = capLen.load (std::memory_order_relaxed);
        if (needStamp.load())
        {
            if (! (playing && haveTime)) return;                       // armed: wait for the host to play
            capStartSample = hostSample; capStartPpq = ppq; needStamp = false; len = 0;
        }
        else if (! playing || ! haveTime || std::llabs (hostSample - (capStartSample.load() + len)) > 2LL * n)
            capturing = false;                                          // host stopped or looped: capture is complete

        if (capturing.load())
        {
            const int todo = jmin (n, (int) capBuf.size() - len);
            for (int i = 0; i < todo; ++i)
            {
                float s = 0.0f;
                for (int c = 0; c < ch; ++c) s += buf.getReadPointer (c)[i];
                capBuf[(size_t) (len + i)] = s / (float) jmax (1, ch);
            }
            capLen.store (len + todo, std::memory_order_release);
            if (todo < n) capturing = false;                            // buffer full
        }
        return;
    }

    // ---- playback: swap in the corrected audio, aligned to the host timeline ----
    if (playing && haveTime && apvts.getRawParameterValue ("correct")->load() > 0.5f)
        if (auto* rb = current.load (std::memory_order_acquire))
        {
            const long long rel = hostSample - rb->startSample;
            const long long total = (long long) rb->data.size();
            for (int i = 0; i < n; ++i)
            {
                const long long idx = rel + i;
                if (idx < 0 || idx >= total) continue;
                const float s = rb->data[(size_t) idx];
                for (int c = 0; c < ch; ++c) buf.getWritePointer (c)[i] = s;
            }
        }
}

AudioProcessorEditor* GraphProcessor::createEditor() { return new GraphEditor (*this); }

void GraphProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    ValueTree nodes ("NOTES");
    for (auto& n : notes)
    {
        ValueTree v ("N");
        v.setProperty ("start", n.start, nullptr).setProperty ("len", n.length, nullptr).setProperty ("pitch", n.pitch, nullptr)
         .setProperty ("tr", n.transition, nullptr).setProperty ("vib", n.vibrato, nullptr).setProperty ("fmt", n.formant, nullptr)
         .setProperty ("fine", n.fine, nullptr).setProperty ("flat", n.flatten, nullptr);
        nodes.appendChild (v, nullptr);
    }
    state.appendChild (nodes, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void GraphProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        notes.clear();
        if (auto nodes = state.getChildWithName ("NOTES"); nodes.isValid())
        {
            for (auto v : nodes)
            {
                GraphNote n;
                n.start = v["start"]; n.length = v["len"]; n.pitch = v["pitch"]; n.transition = v["tr"]; n.vibrato = v["vib"];
                n.formant = v["fmt"]; n.fine = v["fine"]; n.flatten = v["flat"];
                notes.push_back (n);
            }
            state.removeChild (nodes, nullptr);
        }
        apvts.replaceState (state);
        notesChanged();
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GraphProcessor(); }
