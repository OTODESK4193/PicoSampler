// ==========================================
// File: PicoFilter.h
// CleanSVF, Vowel (Formant), Comb フィルター DSP (QuadMorphFilterより完全移植)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

class PicoFilter
{
public:
    enum FilterModel
    {
        CleanSVF = 0,
        VowelFormant,
        CombFilter,
        NumModels
    };

    enum FilterType
    {
        LowPass = 0,
        BandPass,
        HighPass,
        Notch,
        NumTypes
    };

    struct Params
    {
        bool enable = false;
        int model = 0;          // 0: Clean SVF, 1: Vowel, 2: Comb
        int type = 0;           // 0: LP, 1: BP, 2: HP, 3: Notch
        int slope = 0;          // 0: 12dB, 1: 24dB
        float cutoff = 2000.0f; // 20..20000 Hz
        float res = 0.707f;     // 0.1..10.0
        float formant = 0.0f;   // 0.0..1.0 (A, E, I, O, U モーフ)
        float combMix = 0.5f;   // 0.0..1.0 (Comb Wet Mix)
    };

    PicoFilter() = default;
    ~PicoFilter() = default;

    void prepare(double sampleRate, int samplesPerBlock) noexcept;
    void reset() noexcept;

    void process(juce::AudioBuffer<float>& buffer, const Params& p) noexcept;
    float getMagnitudeForFrequency(float freqHz, const Params& p) const noexcept;

private:
    float processSample(int ch, float input, const Params& p) noexcept;

    double sr = 44100.0;

    // --- Clean SVF 状態変数 (2段/4段) ---
    float svf_s1[4][2] = {};
    float svf_s2[4][2] = {};

    // --- Vowel Formant 状態変数 (3バンドフォルマント) ---
    float form_s1[3][2] = {};
    float form_s2[3][2] = {};

    // --- Comb Filter ディレイバッファ ---
    static constexpr int kCombBufSize = 16384;
    float combBuffer[2][kCombBufSize] = {};
    int combWriteIdx[2] = {};
};
