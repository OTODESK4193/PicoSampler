// ==========================================
// File: SampleVisualizerData.h
// ボイス/サンプル再生イベント可視化用ロックフリーリングバッファ
// DSP → GUI リアルタイム同期
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <atomic>

struct SampleVoiceEvent
{
    int slotIndex = 0;
    float normPosition = 0.0f;
    float pan = 0.0f;
    float semitones = 0.0f;
    float durationSec = 0.1f;
    bool active = true;
};

class SampleVisualizerData
{
public:
    static constexpr size_t kCapacity = 256;

    SampleVisualizerData() { writePos.store(0); }

    void push(int slotIdx, float pos, float pan, float semi, float dur, bool active = true) noexcept
    {
        const size_t w = writePos.load(std::memory_order_relaxed);
        buffer[w] = { slotIdx, pos, pan, semi, dur, active };
        writePos.store((w + 1) % kCapacity, std::memory_order_release);
    }

    size_t getWritePos() const noexcept
    {
        return writePos.load(std::memory_order_acquire);
    }

    const SampleVoiceEvent& getEvent(size_t index) const noexcept
    {
        return buffer[index % kCapacity];
    }

private:
    std::array<SampleVoiceEvent, kCapacity> buffer {};
    std::atomic<size_t> writePos { 0 };
};
