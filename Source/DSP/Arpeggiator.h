// ==========================================
// File: Arpeggiator.h
// 13パターン アルペジエイター (Swingノリ制御 & Sync動的追従レスポンス)
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

    static constexpr int kNumSyncRates = 13;

    // ------------------------------------------------------------------
    // Sync 音価インデックス ⇔ 「遅い→速い」順位 の相互変換。
    //
    // getSyncRateNames() の並びは「ストレート / 付点 / 3連」でグループ化
    // されており、実際の音価の長さ順ではない
    // (例: index 3 = 1/4 は 1拍、index 4 = 1/4. は 1.5拍で index 3 より遅い)。
    // そのため MOD で「速く / 遅く」を直感的に動かすには、いったん
    // 音価順の順位へ変換してから増減させる必要がある。
    // ------------------------------------------------------------------
    static int syncRateToRank(int idx) noexcept;
    static int rankToSyncRate(int rank) noexcept;

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
        int repeat = 1;             // 1..4 (ステップリピート)
        float accent = 0.0f;        // -1.0..+1.0 (ベロシティグラデーション)
        float swing = 0.0f;         // 0.0..0.75 (スイング)
        int durMode = 0;
        float gatePct = 0.8f;       // 0.1..1.0
        int key = 0;
        int scale = 0;
        double bpm = 120.0;
    };

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // MIDIブロック処理
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
    void rebuildSequence(int pattern, int octaves, int offset, int key, int scale, int repeat, float accent) noexcept;
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

    bool prevSyncState = true;
    float prevRateFree = 4.0f;
    int prevRateSync = 6;

    juce::Random rng;
};
