// ==========================================
// File: SampleSlot.h
// スロット管理 (fileSampleRate 保持 & rootKey 手動オーバーライド対応)
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
        double fileSampleRate = 44100.0;  // ★ ファイル元のサンプルレートを保持
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

    void prepare(double sr) noexcept { engineSampleRate = sr > 1000.0 ? sr : 44100.0; }
    bool loadFromFile(const juce::File& file);
    void clear();

    bool isReady() const noexcept { return ready.load(std::memory_order_relaxed); }
    bool isAnalyzing() const noexcept { return analyzing.load(std::memory_order_relaxed); }

    const juce::AudioBuffer<float>& getOriginalBuffer() const noexcept { return originalBuffer; }
    const juce::AudioBuffer<float>* getAnchorBuffer(int stOffset) const noexcept;
    const Metadata& getMetadata() const noexcept { return metadata; }
    Metadata& getMetadata() noexcept { return metadata; }

    double getFileSampleRate() const noexcept { return metadata.fileSampleRate; }

private:
    void renderAnchors();

    std::atomic<bool> ready { false };
    std::atomic<bool> analyzing { false };

    double engineSampleRate = 44100.0;
    juce::AudioBuffer<float> originalBuffer;
    std::array<juce::AudioBuffer<float>, kNumAnchors> anchorBuffers;
    Metadata metadata;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleSlot)
};
