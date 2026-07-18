// ==========================================
// File: SamplerEngine.h
// 8スロット＆32ポリフォニー サンプラーエンジン (KeyRangeパラメータ対応)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "SampleSlot.h"
#include "SamplerVoice.h"
#include "TptSvfFilter.h"
#include "SampleVisualizerData.h"

class SamplerEngine
{
public:
    static constexpr int NUM_SLOTS = 8;
    static constexpr int NUM_VOICES = 32;

    enum PlaybackMode
    {
        SingleMode = 0,
        LayerMode  = 1,
        RandomMode = 2
    };

    struct Params
    {
        PlaybackMode mode = SingleMode;
        int activeSlot = 0;
        float masterHpfHz = 20.0f;
        float masterLpfHz = 20000.0f;
        bool is24dBFilter = false;
        float outGainDb = 0.0f;
        int polyphonyLimit = 32;

        std::array<SamplerVoiceParams, NUM_SLOTS> slotParams;
    };

    SamplerEngine() = default;
    ~SamplerEngine() = default;

    void prepare(double sampleRate)
    {
        hostSampleRate = sampleRate;
        for (auto& slot : slots) slot.prepare(sampleRate);
        for (auto& voice : voices) voice.prepare(sampleRate);

        filterL_HP.prepare(sampleRate);
        filterR_HP.prepare(sampleRate);
        filterL_LP.prepare(sampleRate);
        filterR_LP.prepare(sampleRate);
    }

    void reset()
    {
        for (auto& voice : voices) voice.reset();
    }

    SampleSlot& getSlot(int idx) noexcept { return slots[(size_t)juce::jlimit(0, NUM_SLOTS - 1, idx)]; }
    const SampleSlot& getSlot(int idx) const noexcept { return slots[(size_t)juce::jlimit(0, NUM_SLOTS - 1, idx)]; }

    void handleMidi(const juce::MidiBuffer& midi, const Params& p) noexcept;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, const Params& p, SampleVisualizerData* visualizerData) noexcept;

private:
    void triggerSlotNote(int slotIdx, int midiNote, float velocity, const Params& p) noexcept;
    int findFreeVoice() const noexcept;
    int findOldestVoice() const noexcept;

    double hostSampleRate = 44100.0;
    std::array<SampleSlot, NUM_SLOTS> slots;
    std::array<PicoVoice, NUM_VOICES> voices;

    TptSvfFilter filterL_HP, filterR_HP;
    TptSvfFilter filterL_LP, filterR_LP;

    juce::Random rng;
};
