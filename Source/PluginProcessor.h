#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>

struct Note { int midi; double start, dur; int vel; };

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

    juce::CriticalSection lock;
    std::vector<Note> raw;
    std::atomic<bool> pendingArm { false }, pendingStop { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoixMidiProcessor)
};
