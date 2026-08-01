// ==========================================
// File: SamplerVoice.h
// 単一ボイス定義 (rootKeyOverride & pitchInc メンバー追加)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "SampleSlot.h"

struct SamplerVoiceParams
{
    float attack = 0.01f;
    float decay = 0.3f;
    float sustain = 1.0f;
    float release = 0.3f;

    int octave = 0;
    int semitone = 0;
    float fineTune = 0.0f;
    float speed = 1.0f;

    float pan = 0.0f;
    float slotGainDb = 0.0f;

    float sampleStartRatio = 0.0f;
    float sampleEndRatio = 1.0f;
    float loopStartRatio = 0.2f;
    float loopEndRatio = 0.7f;
    float crossfadeRatio = 0.05f;

    // Start/End マーカー端のプチノイズ対策フェード (ミリ秒)
    float edgeFadeInMs  = 2.0f;
    float edgeFadeOutMs = 3.0f;
    bool isLooping = false;
    bool isStretchMode = false;
    bool isReverse = false;
    bool isFilterBypass = false;
    bool isFxBypass = false;

    int lowNote = 0;
    int highNote = 127;

    int rootKeyOverride = -1;  // -1 = 自動解析値を使用, 0-127 = 手動設定
    
    bool portaEnable = false;
    float portaTime = 0.1f;
    float portaStartMidiNote = -1.0f;
};

class PicoVoice
{
public:
    enum class EnvStage { Idle, Attack, Decay, Sustain, Release };

    PicoVoice() = default;
    ~PicoVoice() = default;

    void prepare(double sr) noexcept
    {
        sampleRate = sr > 1000.0 ? sr : 44100.0;

        // Pan / Slot Gain のジッパーノイズ対策。
        // Pan は MOD のアサイン先でもあるため、ブロック単位のままだと
        // LFO をかけた時に定位が階段状に動いてしまう。
        smGainL.reset(sampleRate, 0.01);
        smGainR.reset(sampleRate, 0.01);
    }

    void startNote(int midiNoteNumber, float noteVelocity, int slotIdx,
                   const SampleSlot& slot, const SamplerVoiceParams& p) noexcept;

    void stopNote(bool allowTailOff) noexcept;

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples,
                         const SampleSlot& slot, const SamplerVoiceParams& p) noexcept;

    void reset() noexcept
    {
        active = false;
        releasing = false;
        envStage = EnvStage::Idle;
        readPosition = 0.0;
        envValue = 0.0f;
        pitchInc = 1.0;
        currentPitchRatio = 1.0;
        hasEnteredLoop = false;
        useAnchor = false;
        activeAnchorSemis = 0;
    }

    bool isActive() const noexcept { return active; }
    int getMidiNote() const noexcept { return midiNote; }
    int getSlotIndex() const noexcept { return slotIndex; }
    uint64_t getAge() const noexcept { return ageCounter; }

private:
    float lagrange3rdInterpolate(float y0, float y1, float y2, float y3, float frac) const noexcept
    {
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    double sampleRate = 44100.0;
    bool active = false;
    bool releasing = false;
    int midiNote = 60;
    float velocity = 0.8f;
    int slotIndex = 0;
    uint64_t ageCounter = 0;

    double readPosition = 0.0;
    double pitchInc = 1.0;     // ★ メンバーに昇格 (SR補正を含む)
    double currentPitchRatio = 1.0;
    int currentAnchorSemis = 0;

    // ------------------------------------------------------------------
    // アンカー遅延生成への対応。
    // Note-On 時点で必要なアンカーがまだ出来ていなければ、原音の
    // リサンプリング再生にフォールバックする (音は途切れない)。
    //   useAnchor         : このノートがアンカーを使っているか
    //   activeAnchorSemis : 実際に使っているアンカーの半音数
    //                       (フォールバック中は 0 = 原音そのもの)
    // ノート途中で音色が切り替わると不自然なため、判定は Note-On で確定させ、
    // 途中で昇格はさせない (スロット再ロード等で無効化された時のみ降格する)。
    // ------------------------------------------------------------------
    bool useAnchor = false;
    int activeAnchorSemis = 0;

    // Pan / Slot Gain / Velocity をまとめた最終ゲイン (L/R)。
    // Note-On では現在値へスナップさせる (立ち上がりを鈍らせないため)。
    juce::LinearSmoothedValue<float> smGainL { 0.0f };
    juce::LinearSmoothedValue<float> smGainR { 0.0f };

    EnvStage envStage = EnvStage::Idle;
    float envValue = 0.0f;

    // Loop ON時、Start->End の初回パスを最後まで再生してから
    // L-Start/L-End 間のループに入ったかどうかのフラグ。
    // (初回パス中に readPosition が L-Start 未満でも強制的に
    //  L-Start へスナップしてしまわないようにするため)
    bool hasEnteredLoop = false;
};
