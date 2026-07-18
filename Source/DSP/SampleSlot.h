// ==========================================
// File: SampleSlot.h
// 8スロット個別のサンプル＆24アンカー管理
// SignalStretch によるオフラインピッチシフトアンカー生成
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include "signalsmith-stretch/signalsmith-stretch.h"

class SampleSlot
{
public:
    static constexpr int kNumAnchors = 24; // ルート音の -12 ~ +11 半音

    enum class Status
    {
        Empty,
        Loading,
        Analyzing,
        Stretching,
        Ready,
        Failed
    };

    struct Metadata
    {
        juce::String filePath;
        juce::String fileName;
        int rootKey = 60;              // 検出/指定ルートキー (0-127)
        float centsOffset = 0.0f;
        int lowNote = 0;               // Layer モード音域下限
        int highNote = 127;            // Layer モード音域上限
        double originalSampleRate = 44100.0;
        int numChannels = 2;
        int lengthInSamples = 0;
        bool isLooping = false;
        int sampleStart = 0;
        int sampleEnd = 0;
        int loopStart = 0;
        int loopLength = 0;
        float crossfadeSec = 0.05f;

        // 比率ベースフィールド (WaveformDisplay / SamplerEngine 用)
        float sampleStartRatio = 0.0f;
        float sampleEndRatio = 1.0f;
        float loopStartRatio = 0.2f;
        float loopLengthRatio = 0.5f;
        float crossfadeRatio = 0.05f;
    };

    SampleSlot() = default;
    ~SampleSlot() = default;

    void prepare(double targetSampleRate)
    {
        hostSampleRate = targetSampleRate > 1000.0 ? targetSampleRate : 44100.0;
    }

    bool loadFromFile(const juce::File& file, int rootKeyOverride = -1);

    void clear()
    {
        status.store(Status::Empty);
        originalBuffer.setSize(0, 0);
        for (auto& b : anchorBuffers) b.setSize(0, 0);
        meta = {};
    }

    Status getStatus() const noexcept { return status.load(std::memory_order_relaxed); }
    bool isReady() const noexcept { return getStatus() == Status::Ready; }

    const Metadata& getMetadata() const noexcept { return meta; }
    Metadata& getMetadata() noexcept { return meta; }

    const juce::AudioBuffer<float>* getAnchorBuffer(int stOffsetFromRoot) const noexcept
    {
        const int idx = juce::jlimit(0, kNumAnchors - 1, stOffsetFromRoot + 12);
        if (anchorBuffers[(size_t)idx].getNumSamples() > 0)
            return &anchorBuffers[(size_t)idx];
        return originalBuffer.getNumSamples() > 0 ? &originalBuffer : nullptr;
    }

    const juce::AudioBuffer<float>& getOriginalBuffer() const noexcept { return originalBuffer; }

private:
    void generateAnchorsSignalStretch();

    double hostSampleRate = 44100.0;
    std::atomic<Status> status { Status::Empty };
    Metadata meta;

    juce::AudioBuffer<float> originalBuffer;
    std::array<juce::AudioBuffer<float>, kNumAnchors> anchorBuffers;
};
