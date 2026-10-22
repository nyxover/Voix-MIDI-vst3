#include "PluginEditor.h"

using namespace juce;

static const Colour kBg   (0xff16161a);
static const Colour kCard (0xff1f1f25);
static const Colour kLine (0xff34343c);
static const Colour kAcc  (0xffff5c9a);
static const Colour kOk   (0xff51cf66);
static const Colour kMut  (0xff9a9aa5);

static String noteName (int m)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return String (n[m % 12]) + String (m / 12 - 1);
}

// ---- piano roll ------------------------------------------------------------
void PianoRoll::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (kCard);
    g.fillRoundedRectangle (r, 8.0f);

    const auto notes = proc.getNotes();
    const int L = proc.loopBeats();
    int lo = 48, hi = 84;
    if (! notes.empty())
    {
        lo = 127; hi = 0;
        for (auto& n : notes) { lo = jmin (lo, n.midi); hi = jmax (hi, n.midi); }
        lo -= 2; hi += 3;
        if (hi - lo < 12) { const int m = (hi + lo) / 2; lo = m - 6; hi = m + 6; }
    }
    const float bw = r.getWidth() / (float) L, rh = r.getHeight() / (float) (hi - lo);

    for (int b = 0; b <= L; ++b)
    {
        g.setColour (kLine.withAlpha (b % 4 == 0 ? 1.0f : 0.45f));
        g.drawVerticalLine ((int) (b * bw), 0.0f, r.getHeight());
    }
    g.setFont (11.0f);
    for (int m = lo; m < hi; ++m)
        if (m % 12 == 0)
        {
            g.setColour (kMut.withAlpha (0.6f));
            g.drawText (noteName (m), 3, (int) (r.getHeight() - (m - lo + 1) * rh), 40, 12, Justification::left);
        }

    g.setColour (kAcc);
    for (auto& n : notes)
        g.fillRect (n.start * bw, r.getHeight() - (n.midi - lo + 1) * rh + 1.0f,
                    jmax (2.0f, (float) n.dur * bw - 1.0f), jmax (3.0f, rh - 2.0f));

    const int md = proc.mode.load();
    if (md == VoixMidiProcessor::Recording)
    {
        const float x = (float) jlimit (0.0, (double) L, proc.recBeat.load()) * bw;
        g.setColour (Colours::white);
        g.drawLine (x, 0.0f, x, r.getHeight(), 2.0f);
        const float lm = proc.liveMidi.load();
        if (lm >= (float) lo && lm < (float) hi)
        {
            g.setColour (kOk);
            g.fillEllipse (x - 5.0f, r.getHeight() - (lm - lo) * rh - 5.0f, 10.0f, 10.0f);
        }
    }
    else if (md == VoixMidiProcessor::Armed)
    {
        g.setColour (kMut);
        g.setFont (16.0f);
        g.drawText ("Lance la lecture dans Ableton : l'enregistrement demarre a la prochaine mesure",
                    getLocalBounds(), Justification::centred);
    }
    else if (notes.empty())
    {
        g.setColour (kMut);
        g.setFont (16.0f);
        g.drawText ("Clique sur Enregistrer, puis chante ou siffle", getLocalBounds(), Justification::centred);
    }
}

// ---- drag pad --------------------------------------------------------------
void DragPad::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (kCard);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (kAcc);
    g.drawRoundedRectangle (r, 8.0f, 1.5f);
    g.setColour (Colours::white);
    g.setFont (Font (FontOptions (15.0f, Font::bold)));
    g.drawText ("Glisse ce bloc vers une piste MIDI d'Ableton", getLocalBounds(), Justification::centred);
}

void DragPad::mouseDrag (const MouseEvent& e)
{
    if (dragged || e.getDistanceFromDragStart() < 6) return;
    if (proc.getNotes().empty()) return;
    dragged = true;
    auto f = proc.writeMidi (File::getSpecialLocation (File::tempDirectory).getChildFile ("VoixMidi"),
                             "voix-loop-" + String (Time::currentTimeMillis() % 100000) + ".mid");
    if (f.existsAsFile())
        DragAndDropContainer::performExternalDragDropOfFiles (StringArray { f.getFullPathName() }, false, this);
}

// ---- editor ----------------------------------------------------------------
VoixMidiEditor::VoixMidiEditor (VoixMidiProcessor& p)
    : AudioProcessorEditor (&p), proc (p), roll (p), pad (p)
{
    setSize (760, 460);

    for (auto* b : { &recBtn, &stopBtn, &clearBtn, &saveBtn }) addAndMakeVisible (b);
    recBtn.setColour (TextButton::buttonColourId, kAcc);
    recBtn.onClick   = [this] { proc.arm(); };
    stopBtn.onClick  = [this] { proc.stopRecording(); };
    clearBtn.onClick = [this] { proc.clearNotes(); };
    saveBtn.onClick  = [this]
    {
        auto dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("VoixMidi");
        auto f = proc.writeMidi (dir, "voix-loop-" + Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".mid");
        if (f.existsAsFile()) f.revealToUser();
    };

    auto setup = [this] (ComboBox& c, const char* id, std::unique_ptr<CA>& a)
    {
        if (auto* prm = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
            c.addItemList (prm->choices, 1);
        addAndMakeVisible (c);
        a = std::make_unique<CA> (proc.apvts, id, c);
    };
    setup (bars, "bars", aBars);   setup (quant, "quant", aQuant);
    setup (scale, "scale", aScale); setup (root, "root", aRoot);

    oct.setSliderStyle (Slider::IncDecButtons);
    oct.setTextBoxStyle (Slider::TextBoxLeft, false, 40, 24);
    sens.setSliderStyle (Slider::LinearHorizontal);
    sens.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (oct);  aOct  = std::make_unique<SA> (proc.apvts, "oct", oct);
    addAndMakeVisible (sens); aSens = std::make_unique<SA> (proc.apvts, "sens", sens);

    for (auto* l : { &lBars, &lQuant, &lScale, &lRoot, &lOct, &lSens })
    {
        l->setColour (Label::textColourId, kMut);
        l->setFont (Font (FontOptions (12.0f)));
        addAndMakeVisible (l);
    }
    liveLbl.setFont (Font (FontOptions (26.0f, Font::bold)));
    liveLbl.setColour (Label::textColourId, Colours::white);
    statusLbl.setColour (Label::textColourId, kMut);
    statusLbl.setJustificationType (Justification::centredRight);
    addAndMakeVisible (liveLbl);
    addAndMakeVisible (statusLbl);
    addAndMakeVisible (roll);
    addAndMakeVisible (pad);
    startTimerHz (30);
}

void VoixMidiEditor::paint (Graphics& g)
{
    g.fillAll (kBg);
    const float lv = proc.level.load();
    g.setColour (kLine);
    g.fillRoundedRectangle (200.0f, 22.0f, 200.0f, 8.0f, 4.0f);
    g.setColour (kOk);
    g.fillRoundedRectangle (200.0f, 22.0f, 200.0f * lv, 8.0f, 4.0f);
}

void VoixMidiEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto top = r.removeFromTop (44);
    liveLbl.setBounds (top.removeFromLeft (180));
    top.removeFromLeft (230);
    statusLbl.setBounds (top);
    r.removeFromTop (6);

    auto rowBtn = r.removeFromBottom (36);
    r.removeFromBottom (8);
    pad.setBounds (r.removeFromBottom (44));
    r.removeFromBottom (8);

    auto ctl = r.removeFromBottom (48);
    r.removeFromBottom (8);
    roll.setBounds (r);

    const int w = ctl.getWidth() / 6;
    auto col = [&] (Label& l, Component& c)
    {
        auto a = ctl.removeFromLeft (w).reduced (4, 0);
        l.setBounds (a.removeFromTop (16));
        c.setBounds (a);
    };
    col (lBars, bars); col (lQuant, quant); col (lScale, scale);
    col (lRoot, root); col (lOct, oct);     col (lSens, sens);

    const int bw = rowBtn.getWidth() / 4;
    for (auto* b : { &recBtn, &stopBtn, &clearBtn, &saveBtn })
        b->setBounds (rowBtn.removeFromLeft (bw).reduced (4, 0));
}

void VoixMidiEditor::timerCallback()
{
    const float lm = proc.liveMidi.load();
    liveLbl.setText (lm >= 0.0f ? noteName ((int) std::lround (lm)) : String ("-"), dontSendNotification);
    static const char* st[] = { "Pret", "En attente de la prochaine mesure...", "Enregistrement", "Boucle enregistree" };
    statusLbl.setText (st[jlimit (0, 3, proc.mode.load())], dontSendNotification);
    repaint();
}
