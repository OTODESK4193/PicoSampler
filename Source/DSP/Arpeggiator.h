// ==========================================
// File: Arpeggiator.h
// 13パターン アルペジエイター (Offset ＆ スケール量子化完全連携)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <algorithm>
#include "ScaleQuantizer.h"

class Arpeggiator
{
public:
    enum Pattern
    {
        Up = 0, Down, UpDown, DownUp, UpDownIncl,
        Converge, Diverge, PedalLow, PedalHigh,
        AsPlayed, RandomPattern, RandomWalk, Chord,
        NumPatterns
    };

    enum DurMode
    {
        GateLength = 0, Note1_32, Note1_16, Note1_8, Note1_4
    };

    static juce::StringArray getPatternNames()
    {
        return { "Up", "Down", "Up-Down", "Down-Up", "Up-Down (Incl)",
                 "Converge", "Diverge", "Pedal Low", "Pedal High",
                 "As Played", "Random", "Random Walk", "Chord" };
    }

    static juce::StringArray getSyncRateNames()
    {
        return { "1/1", "1/2", "1/2T", "1/4", "1/4.", "1/4T",
                 "1/8", "1/8.", "1/8T", "1/16", "1/16.", "1/16T", "1/32" };
    }

    static juce::StringArray getDurationNames()
    {
        return { "Gate %", "1/32", "1/16", "1/8", "1/4" };
    }

    struct Params
    {
        bool enable = false;
        bool latch = false;
        bool sync = true;
        int pattern = 0;
        int rateSync = 6;           // 1/8
        float rateFreeHz = 4.0f;
        int octaves = 1;            // 1..4
        int offset = 0;             // -12..+12 半音
        int durMode = 0;
        float gatePct = 0.8f;       // 0.1..1.0
        int key = 0;
        int scale = 0;
        double bpm = 120.0;
    };

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // MIDIブロック処理 (入力MIDIをアルペジオノートに書き換え)
    void process(juce::MidiBuffer& midi, int numSamples, const Params& p) noexcept;

private:
    struct NoteInfo
    {
        int noteNumber = 60;
        float velocity = 0.8f;
        uint32_t orderId = 0;
    };

    struct ActiveOutputNote
    {
        int noteNumber = 60;
        int samplesRemaining = 0;
    };

    void handleIncomingMidi(const juce::MidiBuffer& midi, bool latch) noexcept;
    void rebuildSequence(int pattern, int octaves, int offset, int key, int scale) noexcept;
    int calculateStepSamples(const Params& p, int numSamples) const noexcept;
    int calculateGateSamples(const Params& p, int stepSamples) const noexcept;
    void stopAllActiveNotes(juce::MidiBuffer& outMidi, int sampleOffset) noexcept;

    double sr = 44100.0;
    std::vector<NoteInfo> heldNotes;
    std::vector<NoteInfo> latchedNotes;
    std::vector<NoteInfo> playSequence;

    std::vector<ActiveOutputNote> activeOutputs;

    uint32_t nextOrderId = 0;
    int stepSampleCounter = 0;
    size_t sequenceIndex = 0;
    bool directionUp = true;
    int walkIndex = 0;

    juce::Random rng;
};
