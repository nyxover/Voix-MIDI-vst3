#pragma once
#include "PluginProcessor.h"

class PianoRoll : public juce::Component
{
public:
    explicit PianoRoll (VoixMidiProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    VoixMidiProcessor& proc;
};

class DragPad : public juce::Component
{
public:
    explicit DragPad (VoixMidiProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { dragged = false; }
    void mouseDrag (const juce::MouseEvent&) override;
private:
    VoixMidiProcessor& proc;
    bool dragged = false;
};

class VoixMidiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoixMidiEditor (VoixMidiProcessor&);
    ~VoixMidiEditor() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    VoixMidiProcessor& proc;

    juce::TextButton recBtn { "Enregistrer" }, stopBtn { "Stop" }, clearBtn { "Effacer" }, saveBtn { "Exporter .mid" };
    juce::ComboBox bars, quant, scale, root, midiOut;
    juce::ToggleButton liveBtn { "Mode live" };
    juce::TextButton refreshBtn { "Actualiser" };
    juce::Label lPort { {}, "Sortie MIDI" };
    juce::Array<juce::MidiDeviceInfo> devs;
    void fillDevices();
    juce::Slider oct, sens;
    juce::Label liveLbl, statusLbl;
    juce::Label lBars { {}, "Mesures" }, lQuant { {}, "Quantif." }, lScale { {}, "Gamme" },
                lRoot { {}, "Tonique" }, lOct { {}, "Octave" }, lSens { {}, "Sensibilite" };
    PianoRoll roll;
    DragPad pad;

    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<BA> aLive;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<CA> aBars, aQuant, aScale, aRoot;
    std::unique_ptr<SA> aOct, aSens;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoixMidiEditor)
};
