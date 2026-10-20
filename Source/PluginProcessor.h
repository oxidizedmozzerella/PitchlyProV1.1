#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"

class GraphProcessor : public juce::AudioProcessor
{
public:
    GraphProcessor();
    ~GraphProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PitchlyGraph"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray& keyNames();
    static const juce::StringArray& scaleNames();
    static const juce::StringArray& gridNames();

    // ---- capture: while the host plays, records what flows through the plugin ----
    void startCapture();                    // arms: recording starts when the host starts playing
    void stopCapture()                      { capturing = false; }
    bool isCapturing() const                { return capturing.load(); }
    bool isArmed() const                    { return capturing.load() && needStamp.load(); }
    int  capturedSamples() const            { return capLen.load (std::memory_order_acquire); }
    double captureRate() const              { return capRate; }
    const float* captureData() const        { return capBuf.data(); }
    juce::CriticalSection captureLock;      // held by prepareToPlay (resize) and by readers

    // ---- analysis / engine state for the UI ----
    int  workerState() const                { return wstate.load(); }            // 0 idle, 1 analysing, 2 rendering
    int  analysisVersion() const            { return trackVersion.load(); }
    std::shared_ptr<const PitchTrack> getPitchTrack() const { const juce::ScopedLock sl (trackLock); return track; }
    double getBpm() const                   { return bpm.load(); }
    double getCaptureStartPpq() const       { return capStartPpq.load(); }
    void notesChanged();                    // call after any edit of `notes`: schedules a re-render

    juce::AudioProcessorValueTreeState apvts;
    std::vector<GraphNote> notes;           // message thread only (the worker gets a snapshot)

private:
    struct RenderBuf { std::vector<float> data; long long startSample = 0; };
    class Worker;

    void publish (RenderBuf*);
    void retire (RenderBuf*);
    void collectGarbage (bool all);

    static constexpr int kMaxCaptureSeconds = 180;
    std::vector<float> capBuf;
    std::atomic<int> capLen { 0 };
    std::atomic<bool> capturing { false }, needStamp { false };
    std::atomic<long long> capStartSample { -1 };
    std::atomic<double> capStartPpq { 0.0 }, bpm { 120.0 };
    double capRate = 44100.0;

    std::atomic<RenderBuf*> current { nullptr };
    juce::CriticalSection retireLock, noteLock;
    mutable juce::CriticalSection trackLock;
    std::vector<std::pair<RenderBuf*, juce::uint32>> retired;
    std::vector<GraphNote> noteSnapshot;
    std::shared_ptr<const PitchTrack> track;
    std::atomic<int> generation { 0 }, trackVersion { 0 }, wstate { 0 };
    std::atomic<bool> renderDirty { false };
    std::atomic<juce::int64> lastEditMs { 0 };
    std::unique_ptr<Worker> worker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphProcessor)
};
