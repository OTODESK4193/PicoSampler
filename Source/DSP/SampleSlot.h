// ==========================================
// File: SampleSlot.h
// スロット管理 (prepare メソッド追加)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "signalsmith-stretch/signalsmith-stretch.h"

class SampleSlot
{
public:
    static constexpr int kNumAnchors = 24;

    struct Metadata
    {
        juce::String filePath;
        juce::String fileName;
        int rootKey = 60;
        float centsOffset = 0.0f;
        float sampleStartRatio = 0.0f;
        float sampleEndRatio = 1.0f;
        float loopStartRatio = 0.2f;
        float loopLengthRatio = 0.5f;
        float crossfadeRatio = 0.05f;
        bool isLooping = false;
        bool isReverse = false;
    };

    SampleSlot() = default;
    ~SampleSlot() = default;

    void prepare(double sr) noexcept { juce::ignoreUnused(sr); }
    bool loadFromFile(const juce::File& file);
    void clear();

    bool isReady() const noexcept { return ready.load(std::memory_order_relaxed); }
    bool isAnalyzing() const noexcept { return analyzing.load(std::memory_order_relaxed); }

    const juce::AudioBuffer<float>& getOriginalBuffer() const noexcept { return originalBuffer; }
    const juce::AudioBuffer<float>* getAnchorBuffer(int stOffset) const noexcept;
    const Metadata& getMetadata() const noexcept { return metadata; }
    Metadata& getMetadata() noexcept { return metadata; }

private:
    void renderAnchors();

    std::atomic<bool> ready { false };
    std::atomic<bool> analyzing { false };

    juce::AudioBuffer<float> originalBuffer;
    std::array<juce::AudioBuffer<float>, kNumAnchors> anchorBuffers;
    Metadata metadata;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleSlot)
};
