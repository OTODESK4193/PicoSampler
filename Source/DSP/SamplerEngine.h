// ==========================================
// File: SamplerEngine.h
// マルチボイス・サンプラーエンジン (BrickLimiter完全移植)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "SampleSlot.h"
#include "SamplerVoice.h"
#include "SampleVisualizerData.h"
#include "TptSvfFilter.h"

// Granular準拠 BrickLimiter
class BrickLimiter
{
public:
    void prepare(double sr) noexcept
    {
        sampleRate = sr;
        setReleaseMs(50.0f);
        gain = 1.0f;
    }

    void setReleaseMs(float ms) noexcept
    {
        const double t = juce::jmax(0.5f, ms) * 0.001;
        releaseCoef = 1.0f - std::exp((float)(-1.0 / (t * sampleRate)));
    }

    inline void process(float& l, float& r, float ceilingLin) noexcept
    {
        const float peak = juce::jmax(std::abs(l), std::abs(r), 1.0e-9f);
        const float target = peak > ceilingLin ? ceilingLin / peak : 1.0f;
        if (target < gain) gain = target;                    // 即時アタック
        else gain += releaseCoef * (target - gain);          // スムーズリリース
        l *= gain;
        r *= gain;
    }

private:
    double sampleRate = 44100.0;
    float gain = 1.0f;
    float releaseCoef = 0.0005f;
};

class SamplerEngine
{
public:
    static constexpr int NUM_SLOTS = 8;
    static constexpr int NUM_VOICES = 32;

    enum PlaybackMode
    {
        SingleMode = 0,
        LayerMode,
        RandomMode
    };

    struct Params
    {
        PlaybackMode mode = SingleMode;
        int activeSlot = 0;

        float masterHpfHz = 20.0f;
        float masterLpfHz = 20000.0f;
        bool is24dBFilter = false;
        float outGainDb = 0.0f;
        float ceilingDb = -0.1f;
        float limReleaseMs = 50.0f;
        int polyphonyLimit = 32;

        std::array<SamplerVoiceParams, NUM_SLOTS> slotParams {};
    };

    SamplerEngine() = default;
    ~SamplerEngine() = default;

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        for (auto& slot : slots) slot.prepare(sampleRate);
        for (auto& v : voices) v.prepare(sampleRate);
        filterL_HP.prepare(sampleRate);
        filterR_HP.prepare(sampleRate);
        filterL_LP.prepare(sampleRate);
        filterR_LP.prepare(sampleRate);
        limiter.prepare(sampleRate);
    }

    void reset() noexcept
    {
        for (auto& v : voices) v.reset();
        filterL_HP.reset();
        filterR_HP.reset();
        filterL_LP.reset();
        filterR_LP.reset();
    }

    SampleSlot& getSlot(int index) noexcept { return slots[(size_t)juce::jlimit(0, NUM_SLOTS - 1, index)]; }
    const SampleSlot& getSlot(int index) const noexcept { return slots[(size_t)juce::jlimit(0, NUM_SLOTS - 1, index)]; }

    std::array<bool, 128> getPlayingNotes() const noexcept
    {
        std::array<bool, 128> notes {};
        for (const auto& v : voices)
        {
            if (v.isActive())
            {
                const int n = v.getMidiNote();
                if (n >= 0 && n < 128) notes[(size_t)n] = true;
            }
        }
        return notes;
    }

    void handleMidi(const juce::MidiBuffer& midi, const Params& p) noexcept;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, const Params& p, SampleVisualizerData* visualizerData = nullptr) noexcept;

    std::function<void(int newActiveSlot)> onActiveSlotTriggered;

private:
    void triggerSlotNote(int slotIdx, int midiNote, float velocity, const Params& p) noexcept;
    int findFreeVoice() const noexcept;
    int findOldestVoice() const noexcept;

    double sr = 44100.0;
    int lastRandomSlot = -1;
    std::array<SampleSlot, NUM_SLOTS> slots;
    std::array<PicoVoice, NUM_VOICES> voices;

    TptSvfFilter filterL_HP, filterR_HP;
    TptSvfFilter filterL_LP, filterR_LP;
    BrickLimiter limiter;

    juce::Random rng;
};
