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

        // Start / End / L-Start / L-End / X-Fade も MOD のアサイン先。
        // ブロック単位で動かすとループ端が readPosition を一気に追い越し、
        // クロスフェードを通らずに波形がワープしてプチッと鳴る。
        smStartRatio.reset(sampleRate, 0.01);
        smEndRatio.reset(sampleRate, 0.01);
        smLoopStart.reset(sampleRate, 0.01);
        smLoopEnd.reset(sampleRate, 0.01);
        smXFade.reset(sampleRate, 0.01);
    }

    // startStamp: SamplerEngine が発行する単調増加の通し番号。
    // 「一番古いボイス」を正しく選ぶために使う (getAge() 参照)。
    void startNote(int midiNoteNumber, float noteVelocity, int slotIdx,
                   const SampleSlot& slot, const SamplerVoiceParams& p,
                   uint64_t startStamp) noexcept;

    void stopNote(bool allowTailOff) noexcept;

    bool isReleasing() const noexcept { return releasing || envStage == EnvStage::Release; }

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
        releaseRateOverride = 0.0f;
        declickPos = 0;
        declickLen = 0;
        declickL = declickR = 0.0f;
        lastOutL = lastOutR = 0.0f;
    }

    bool isActive() const noexcept { return active; }
    int getMidiNote() const noexcept { return midiNote; }
    int getSlotIndex() const noexcept { return slotIndex; }

    // 発音開始時に割り当てられた通し番号。小さいほど古い。
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

    // デクリックテールのゲイン (1 → 0 の raised cosine)。呼ぶたびに1サンプル進む。
    inline float nextDeclickGain() noexcept
    {
        if (declickPos >= declickLen) return 0.0f;
        const float t = 1.0f - (float)declickPos / (float)declickLen;
        ++declickPos;
        return 0.5f - 0.5f * std::cos(t * juce::MathConstants<float>::pi);
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

    // サンプル位置マーカー (比率 0..1)。Note-On で現在値へスナップする。
    juce::LinearSmoothedValue<float> smStartRatio { 0.0f };
    juce::LinearSmoothedValue<float> smEndRatio   { 1.0f };
    juce::LinearSmoothedValue<float> smLoopStart  { 0.2f };
    juce::LinearSmoothedValue<float> smLoopEnd    { 0.7f };
    juce::LinearSmoothedValue<float> smXFade      { 0.05f };

    EnvStage envStage = EnvStage::Idle;
    float envValue = 0.0f;

    // ------------------------------------------------------------------
    // デクリッカー
    //
    // ボイスが奪われて (voice stealing) 別のノートで再スタートすると、
    // 直前まで出ていた振幅から一気に 0 (= 新しいノートの Attack 開始点) へ
    // 飛ぶため、1サンプルの段差ができてプチッと鳴る。
    // ARP を高速に回すとボイス数が足りず毎ステップ発生し、
    // その段差が Delay / Reverb のフィードバックに入って延々と繰り返される。
    //
    // 対策として、切り替え直前の出力値を数ミリ秒かけて 0 まで
    // raised cosine で落とす短いテールを加算し、段差を殺す。
    // ------------------------------------------------------------------
    float lastOutL = 0.0f, lastOutR = 0.0f;
    float declickL = 0.0f, declickR = 0.0f;
    int   declickPos = 0;
    int   declickLen = 0;

    // 0 より大きいとき、p.release ではなくこのレート (1サンプルあたりの
    // 減衰量) でリリースする。ボイス強制停止時の高速フェード用。
    float releaseRateOverride = 0.0f;

    // Loop ON時、Start->End の初回パスを最後まで再生してから
    // L-Start/L-End 間のループに入ったかどうかのフラグ。
    // (初回パス中に readPosition が L-Start 未満でも強制的に
    //  L-Start へスナップしてしまわないようにするため)
    bool hasEnteredLoop = false;
};
