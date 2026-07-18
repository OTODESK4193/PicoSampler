// ==========================================
// File: SamplerEngine.h
// 32ボイス マルチモードサンプラーエンジン
// Single / Layer / Random モード ＆ TPT SVF フィルター
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include "SampleSlot.h"
#include "SamplerVoice.h"
#include "SampleVisualizerData.h"

class TptSvfFilter
{
public:
    enum Type { LowPass, HighPass };

    void prepare(double sr) noexcept
    {
        sampleRate = sr;
        reset();
    }

    void reset() noexcept
    {
        s1 = s2 = 0.0f;
        s1_2 = s2_2 = 0.0f;
    }

    void setCutoffAndType(float cutoffHz, Type type, bool is24dB) noexcept
    {
        const float wd = juce::MathConstants<float>::twoPi * juce::jlimit(10.0f, (float)(sampleRate * 0.49), cutoffHz);
        const float T = 1.0f / (float)sampleRate;
        const float wa = (2.0f / T) * std::tan(wd * T * 0.5f);
        g = wa * T * 0.5f;
        R = 0.7071f;
        filterType = type;
        twoPole = is24dB;
    }

    inline float processSample(float in) noexcept
    {
        const float hp = (in - (2.0f * R + g) * s1 - s2) / (1.0f + 2.0f * R * g + g * g);
        const float bp = g * hp + s1;
        s1 = g * hp + bp;
        const float lp = g * bp + s2;
        s2 = g * bp + lp;

        float out = (filterType == HighPass) ? hp : lp;
        if (twoPole)
        {
            const float hp2 = (out - (2.0f * R + g) * s1_2 - s2_2) / (1.0f + 2.0f * R * g + g * g);
            const float bp2 = g * hp2 + s1_2;
            s1_2 = g * hp2 + bp2;
            const float lp2 = g * bp2 + s2_2;
            s2_2 = g * bp2 + lp2;
            out = (filterType == HighPass) ? hp2 : lp2;
        }

        return out;
    }

private:
    double sampleRate = 44100.0;
    float g = 0.0f, R = 0.7071f;
    float s1 = 0.0f, s2 = 0.0f;
    float s1_2 = 0.0f, s2_2 = 0.0f;
    Type filterType = LowPass;
    bool twoPole = false;
};

class SamplerEngine
{
public:
    static constexpr int NUM_VOICES = 32;
    static constexpr int NUM_SLOTS = 8;

    enum PlaybackMode { SingleMode = 0, LayerMode, RandomMode };

    struct Params
    {
        PlaybackMode mode = SingleMode;
        int activeSlot = 0;
        std::array<SamplerVoiceParams, NUM_SLOTS> slotParams;

        float masterHpfHz = 20.0f;
        float masterLpfHz = 20000.0f;
        bool is24dBFilter = false;
        float outGainDb = 0.0f;
        int polyphonyLimit = 32;
    };

    SamplerEngine() = default;

    void prepare(double sr) noexcept
    {
        sampleRate = sr;
        for (auto& v : voices) v.prepare(sr);
        filterL_HP.prepare(sr); filterR_HP.prepare(sr);
        filterL_LP.prepare(sr); filterR_LP.prepare(sr);
        for (auto& s : slots) s.prepare(sr);
    }

    void reset() noexcept
    {
        for (auto& v : voices) v.reset();
        filterL_HP.reset(); filterR_HP.reset();
        filterL_LP.reset(); filterR_LP.reset();
    }

    void handleMidi(const juce::MidiBuffer& midi, const Params& p) noexcept;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, const Params& p, SampleVisualizerData* visualizerData = nullptr) noexcept;

    SampleSlot& getSlot(int index) noexcept { return slots[(size_t)juce::jlimit(0, NUM_SLOTS - 1, index)]; }
    const SampleSlot& getSlot(int index) const noexcept { return slots[(size_t)juce::jlimit(0, NUM_SLOTS - 1, index)]; }

private:
    int findFreeVoice() const noexcept;
    int findOldestVoice() const noexcept;
    void triggerSlotNote(int slotIdx, int midiNote, float velocity, const Params& p) noexcept;

    double sampleRate = 44100.0;
    std::array<SampleSlot, NUM_SLOTS> slots;
    std::array<PicoVoice, NUM_VOICES> voices;

    TptSvfFilter filterL_HP, filterR_HP;
    TptSvfFilter filterL_LP, filterR_LP;
    juce::Random rng;
};
