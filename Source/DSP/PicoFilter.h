// ==========================================
// File: PicoFilter.h
// 単系統フィルター DSP (Wavetableプロジェクト DualFilterEngine より移植)
//   種類: LPF / HPF / BPF / Notch / Comb / LadderLPF(Moog) / Vowel / CombPlus / Phaser
//   12dB/24dB スロープ切替 (LPF/HPF/BPF のみ)
//   getMagnitudeForFrequency() で GUI カーブ描画と DSP が同一式を共有する
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "LadderFilter.h"

inline float snapToZero(float val) noexcept
{
    return std::abs(val) < 1.0e-15f ? 0.0f : val;
}

class PicoFilter
{
public:
    enum FilterType
    {
        LPF = 0,
        HPF,
        BPF,
        Notch,
        Comb,
        LadderLPF,
        Vowel,
        CombPlus,
        Phaser,
        NumTypes
    };

    // 状態変数の異常検出しきい値（これを超えたらリセット）
    static constexpr float kStateLimit = 1000.0f;

    struct Params
    {
        bool  enable  = false;
        int   type    = LPF;    // FilterType
        bool  slope24 = false;  // LPF/HPF/BPF のみ有効
        float cutoff  = 2000.0f; // 20..20000 Hz
        float res     = 0.707f;  // 0.1..10.0 (Q相当)
        float formant = 0.0f;    // 0.0..1.0 (Vowel: A/E/I/O/U モーフ)
        float combMix = 0.5f;    // 0.0..1.0 (CombPlus / Phaser の Wet Mix)
    };

    PicoFilter() = default;
    ~PicoFilter() = default;

    void prepare(double sampleRate, int samplesPerBlock) noexcept;
    void reset() noexcept;

    void process(juce::AudioBuffer<float>& buffer, const Params& p) noexcept;

    // GUI 用: 周波数特性（振幅）。オーディオスレッドの状態には一切触れない純関数。
    static float getMagnitudeForFrequency(float freqHz, const Params& p, double sampleRate) noexcept;

private:
    float processSample(int ch, float input, const Params& p) noexcept;

    static float resToQ(float res) noexcept { return juce::jlimit(0.1f, 10.0f, res); }
    static float res01(float res) noexcept { return juce::jlimit(0.0f, 1.0f, (res - 0.1f) / 9.9f); }
    static float vowelQ(float res) noexcept { return juce::jlimit(0.5f, 8.0f, 0.5f + res01(res) * 7.5f); }

    static void getVowelFreqs(float formant01, float cutoffHz, float sr,
                               float& f1, float& f2, float& f3) noexcept;

    double sr = 44100.0;

    // ------------------------------------------------------------------
    // パラメータスムージング (ジッパーノイズ対策)
    //
    // 旧実装は PluginProcessor 側の juce::LinearSmoothedValue を
    // 「1ブロックにつき getNextValue() 1回」しか進めていなかったため、
    // 実効スムージング時間が 20ms × バッファサイズ
    // (512サンプル設定なら約10秒) になり、Cutoff ノブがほとんど追従しなかった。
    // ここでサンプル単位に平滑化する形へ移し、ノブ操作にも MOD にも
    // 正しい速度で追従させる。
    // ------------------------------------------------------------------
    float smCoef    = 0.0015f;   // prepare() でサンプルレートから算出
    float smCutoff  = 2000.0f;
    float smRes     = 0.707f;
    float smFormant = 0.0f;
    float smCombMix = 0.5f;
    bool  smInitialised = false;

    // --- SVF 状態変数 (LPF/HPF/BPF/Notch, 2段まで) ---
    float svf_s1[2][2] = {};
    float svf_s2[2][2] = {};

    // --- Vowel Formant 状態変数 (3バンドフォルマント) ---
    float form_s1[3][2] = {};
    float form_s2[3][2] = {};

    // --- Comb / CombPlus ディレイバッファ ---
    static constexpr int kCombBufSize = 16384;
    float combBuffer[2][kCombBufSize] = {};
    int combWriteIdx[2] = {};

    // --- LadderLPF ---
    LadderFilter ladderL, ladderR;

    // --- Phaser (All-Pass カスケード) ---
    float apState[8][2] = {};
    float apPrev[2] = { 0.0f, 0.0f };
};
