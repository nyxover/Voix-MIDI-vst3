#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>

struct Note { int midi; double start, dur; int vel; };

// Envoie les notes vers un port MIDI depuis un thread dedie (pas de MIDI sur le thread audio)
class MidiSender : public juce::Thread
{
public:
    MidiSender() : juce::Thread ("VoixMidi out") { startThread(); }
    ~MidiSender() override { stopThread (500); }

    void push (bool on, int note, int vel)
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) { buf[s1] = { on, note, vel }; fifo.finishedWrite (1); evt.signal(); }
    }
    void setDevice (const juce::String& id)
    {
        std::unique_ptr<juce::MidiOutput> o;
        if (id.isNotEmpty()) o = juce::MidiOutput::openDevice (id);
        const juce::ScopedLock sl (lock);
        if (out) out->sendMessageNow (juce::MidiMessage::allNotesOff (1));
        out = std::move (o);
        open = out != nullptr;
    }
    bool isOpen() const { return open.load(); }

    void run() override
    {
        while (! threadShouldExit())
        {
            evt.wait (100);
            while (fifo.getNumReady() > 0)
            {
                int s1, n1, s2, n2;
                fifo.prepareToRead (1, s1, n1, s2, n2);
                if (n1 <= 0) break;
                const Ev e = buf[s1];
                fifo.finishedRead (1);
                const juce::ScopedLock sl (lock);
                if (out)
                    out->sendMessageNow (e.on ? juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel)
                                              : juce::MidiMessage::noteOff (1, e.note, (juce::uint8) 0));
            }
        }
    }
private:
    struct Ev { bool on; int note, vel; };
    juce::AbstractFifo fifo { 256 };
    Ev buf[256] {};
    juce::WaitableEvent evt;
    juce::CriticalSection lock;
    std::unique_ptr<juce::MidiOutput> out;
    std::atomic<bool> open { false };
};

class VoixMidiProcessor : public juce::AudioProcessor
{
public:
    enum Mode { Idle = 0, Armed, Recording, Done };

    VoixMidiProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == l.getMainOutputChannelSet()
            && ! l.getMainInputChannelSet().isDisabled();
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Voix MIDI"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // UI API
    void arm();
    void stopRecording();
    void clearNotes();
    std::vector<Note> getNotes() const;      // quantised / scaled / transposed
    int loopBeats() const;
    int mapNote (int midi) const;            // gamme + octave
    void setMidiDevice (const juce::String& id);
    juce::String getMidiDevice() const { const juce::ScopedLock sl (lock); return midiId; }
    juce::File writeMidi (const juce::File& folder, const juce::String& name);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>  liveMidi { -1.0f }, level { 0.0f };
    std::atomic<int>    mode { Idle };
    std::atomic<double> recBeat { 0.0 }, bpm { 120.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    float yin (const float* x, int n) const;
    void analyse (double frameBeat, bool playing);
    void feed (double beat, float m, float rms);
    void closeNote();
    void emitOn();
    void emitOff();

    static constexpr int WIN = 2048, HOP = 512;
    float ring[WIN] {}, win[WIN] {};
    int wp = 0, sinceHop = 0;
    double sr = 44100.0;
    int minOn = 4, minSwitch = 5, minSil = 7;

    // tracker state (audio thread)
    bool hasCur = false;
    Note cur {};
    double curLast = 0, candB = 0, recStart = -1.0;
    int cand = -999, candN = 0, silent = 0;

    MidiSender sender;
    juce::String midiId;
    int heldNote = -1;
    bool liveActive = false;
    double liveT = 0.0;

    mutable juce::CriticalSection lock;
    std::vector<Note> raw;
    std::atomic<bool> pendingArm { false }, pendingStop { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoixMidiProcessor)
};
