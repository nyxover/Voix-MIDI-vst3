#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

using namespace juce;

static const int   kBars[]  = { 1, 2, 4, 8 };
static const float kQuant[] = { 0.0f, 0.25f, 0.5f, 1.0f };
static const std::vector<int> kScales[] = {
    {}, { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 3, 5, 7, 10 }, { 0, 2, 4, 7, 9 } };

AudioProcessorValueTreeState::ParameterLayout VoixMidiProcessor::makeLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 }, "Mesures",
           StringArray { "1", "2", "4", "8" }, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "quant", 1 }, "Quantification",
           StringArray { "Aucune", "1/16", "1/8", "1/4" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Gamme",
           StringArray { "Chromatique", "Majeure", "Mineure", "Pentatonique mineure", "Pentatonique majeure" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Tonique",
           StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "oct", 1 }, "Octave", -3, 3, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "sens", 1 }, "Sensibilite",
           NormalisableRange<float> (1.0f, 10.0f, 0.1f), 5.0f));
    return l;
}

VoixMidiProcessor::VoixMidiProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", makeLayout())
{
    raw.reserve (4096);
}

void VoixMidiProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    const double hop = HOP / sr;
    minOn     = jmax (2, (int) std::ceil (0.045 / hop));
    minSwitch = jmax (3, (int) std::ceil (0.055 / hop));
    minSil    = jmax (3, (int) std::ceil (0.080 / hop));
    zeromem (ring, sizeof (ring));
    wp = sinceHop = 0;
}

int VoixMidiProcessor::loopBeats() const
{
    return 4 * kBars[jlimit (0, 3, (int) apvts.getRawParameterValue ("bars")->load())];
}

// ---- pitch detection (YIN) -------------------------------------------------
float VoixMidiProcessor::yin (const float* x, int n) const
{
    const int N = n / 2;
    const int maxT = jmin (N - 1, (int) (sr / 70.0));
    const int minT = jmax (2, (int) (sr / 2000.0));
    float d[WIN / 2 + 2];
    d[0] = 1.0f;
    for (int t = 1; t <= maxT; ++t)
    {
        float s = 0.0f;
        for (int i = 0; i < N; ++i) { const float v = x[i] - x[i + t]; s += v * v; }
        d[t] = s;
    }
    float run = 0.0f;
    for (int t = 1; t <= maxT; ++t) { run += d[t]; d[t] = run > 0.0f ? d[t] * t / run : 1.0f; }
    for (int t = minT; t < maxT; ++t)
    {
        if (d[t] < 0.15f)
        {
            while (t + 1 < maxT && d[t + 1] < d[t]) ++t;
            const float a = d[t - 1], b = d[t], c = d[t + 1], den = a + c - 2.0f * b;
            const float sh = den != 0.0f ? (a - c) / (2.0f * den) : 0.0f;
            return (float) (sr / (t + sh));
        }
    }
    return 0.0f;
}

// ---- note segmentation -----------------------------------------------------
void VoixMidiProcessor::closeNote()
{
    if (! hasCur) return;
    Note n = cur;
    n.dur = jmax (0.05, curLast - cur.start + 0.03 * bpm.load() / 60.0);
    { ScopedLock sl (lock); raw.push_back (n); }
    hasCur = false;
}

void VoixMidiProcessor::feed (double b, float m, float rms)
{
    if (b < 0.0) return;
    if (m < 0.0f)
    {
        ++silent; cand = -999; candN = 0;
        if (silent >= minSil) closeNote();
        return;
    }
    silent = 0;
    const int v = jlimit (30, 127, (int) (40 + rms * 900));
    const int r = (int) std::lround (m);
    if (hasCur)
    {
        if (std::abs (m - cur.midi) < 0.7f) { curLast = b; cur.vel = jmax (cur.vel, v); cand = -999; candN = 0; return; }
        if (cand == r) ++candN; else { cand = r; candN = 1; candB = b; }
        if (candN >= minSwitch) { closeNote(); cur = { r, candB, 0.0, v }; curLast = b; hasCur = true; cand = -999; candN = 0; }
    }
    else
    {
        if (cand == r) ++candN; else { cand = r; candN = 1; candB = b; }
        if (candN >= minOn) { cur = { r, candB, 0.0, v }; curLast = b; hasCur = true; cand = -999; candN = 0; }
    }
}

void VoixMidiProcessor::analyse (double frameBeat, bool playing)
{
    for (int i = 0; i < WIN; ++i) win[i] = ring[(wp + i) % WIN];
    float s = 0.0f;
    for (int i = 0; i < WIN; ++i) s += win[i] * win[i];
    const float rms = std::sqrt (s / WIN);
    const float thr = 0.05f * std::pow (0.7f, apvts.getRawParameterValue ("sens")->load());

    float m = -1.0f;
    if (rms > thr)
    {
        const float f = yin (win, WIN);
        if (f > 0.0f) m = 69.0f + 12.0f * std::log2 (f / 440.0f);
    }
    liveMidi = m;
    level = jmin (1.0f, rms / thr * 0.25f);

    const int md = mode.load();
    if (md == Armed)
    {
        if (! playing) return;
        if (recStart < 0.0) recStart = std::ceil (frameBeat / 4.0 - 1e-6) * 4.0;
        if (frameBeat >= recStart) mode = Recording;
        else { recBeat = frameBeat - recStart; return; }
    }
    if (mode.load() == Recording)
    {
        const double b = frameBeat - recStart, L = (double) loopBeats();
        recBeat = b;
        feed (b, m, rms);
        if (b >= L || ! playing)
        {
            closeNote();
            ScopedLock sl (lock);
            raw.erase (std::remove_if (raw.begin(), raw.end(), [L] (const Note& n) { return n.start >= L; }), raw.end());
            for (auto& n : raw) n.dur = jmin (n.dur, L - n.start);
            mode = raw.empty() ? Idle : Done;
            recStart = -1.0;
        }
    }
}

void VoixMidiProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int n = b.getNumSamples(), nch = b.getNumChannels();
    double ppq0 = 0.0, hostBpm = 120.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { ppq0 = *p; havePpq = true; }
            if (auto t = pos->getBpm()) hostBpm = *t;
        }
    bpm = hostBpm;

    if (pendingArm.exchange (false))
    {
        hasCur = false; cand = -999; candN = 0; silent = 0; recStart = -1.0;
        mode = Armed;
    }
    if (pendingStop.exchange (false))
    {
        closeNote();
        recStart = -1.0;
        ScopedLock sl (lock);
        mode = raw.empty() ? Idle : Done;
    }

    const double bps = hostBpm / 60.0;
    for (int i = 0; i < n; ++i)
    {
        float s = b.getSample (0, i);
        if (nch > 1) s = 0.5f * (s + b.getSample (1, i));
        ring[wp] = s;
        wp = (wp + 1) % WIN;
        if (++sinceHop >= HOP)
        {
            sinceHop = 0;
            analyse (ppq0 + (i - WIN / 2) / sr * bps, playing && havePpq);
        }
    }
}

// ---- UI-side API -----------------------------------------------------------
void VoixMidiProcessor::arm()
{
    { ScopedLock sl (lock); raw.clear(); }
    pendingArm = true;
    mode = Armed;
}
void VoixMidiProcessor::stopRecording() { pendingStop = true; }
void VoixMidiProcessor::clearNotes()
{
    pendingStop = true;
    { ScopedLock sl (lock); raw.clear(); }
    mode = Idle;
}

std::vector<Note> VoixMidiProcessor::getNotes() const
{
    const float g = kQuant[jlimit (0, 3, (int) apvts.getRawParameterValue ("quant")->load())];
    const int sc = jlimit (0, 4, (int) apvts.getRawParameterValue ("scale")->load());
    const int root = (int) apvts.getRawParameterValue ("root")->load();
    const int tr = 12 * (int) apvts.getRawParameterValue ("oct")->load();
    const double L = loopBeats();

    std::vector<Note> out;
    {
        ScopedLock sl (lock);
        out.reserve (raw.size());
        for (auto n : raw)
        {
            if (g > 0.0f)
            {
                n.start = std::round (n.start / g) * g;
                n.dur   = jmax ((double) g, std::round (n.dur / g) * g);
            }
            if (n.start >= L) n.start -= L;
            n.dur = jmax (0.05, jmin (n.dur, L - n.start));
            if (! kScales[sc].empty())
            {
                int best = n.midi, bd = 99;
                for (int x = n.midi - 6; x <= n.midi + 6; ++x)
                {
                    const int deg = ((x - root) % 12 + 12) % 12;
                    if (std::find (kScales[sc].begin(), kScales[sc].end(), deg) != kScales[sc].end()
                        && std::abs (x - n.midi) < bd) { bd = std::abs (x - n.midi); best = x; }
                }
                n.midi = best;
            }
            n.midi = jlimit (0, 127, n.midi + tr);
            out.push_back (n);
        }
    }
    std::sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    return out;
}

File VoixMidiProcessor::writeMidi (const File& folder, const String& name)
{
    const int P = 480;
    MidiMessageSequence seq;
    seq.addEvent (MidiMessage::tempoMetaEvent ((int) (60000000.0 / jmax (20.0, bpm.load()))));
    for (auto& n : getNotes())
    {
        seq.addEvent (MidiMessage::noteOn  (1, n.midi, (uint8) n.vel),
                      n.start * P);
        seq.addEvent (MidiMessage::noteOff (1, n.midi, (uint8) 64),
                      (n.start + n.dur) * P);
    }
    seq.addEvent (MidiMessage (MidiMessage::endOfTrack(), (double) loopBeats() * P));
    seq.updateMatchedPairs();

    MidiFile mf;
    mf.setTicksPerQuarterNote (P);
    mf.addTrack (seq);

    folder.createDirectory();
    File f = folder.getChildFile (name);
    f.deleteFile();
    FileOutputStream os (f);
    if (os.openedOk()) mf.writeTo (os);
    return f;
}

// ---- state -----------------------------------------------------------------
void VoixMidiProcessor::getStateInformation (MemoryBlock& dest)
{
    auto st = apvts.copyState();
    String s;
    { ScopedLock sl (lock); for (auto& n : raw) s << n.midi << "," << n.start << "," << n.dur << "," << n.vel << ";"; }
    st.setProperty ("notes", s, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void VoixMidiProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            std::vector<Note> loaded;
            StringArray recs;
            recs.addTokens (vt.getProperty ("notes").toString(), ";", "");
            for (auto& r : recs)
            {
                StringArray f;
                f.addTokens (r, ",", "");
                if (f.size() == 4) loaded.push_back ({ f[0].getIntValue(), f[1].getDoubleValue(), f[2].getDoubleValue(), f[3].getIntValue() });
            }
            apvts.replaceState (vt);
            { ScopedLock sl (lock); raw = loaded; }
            mode = loaded.empty() ? Idle : Done;
        }
}

AudioProcessorEditor* VoixMidiProcessor::createEditor() { return new VoixMidiEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoixMidiProcessor(); }
