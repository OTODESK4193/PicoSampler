// ==========================================
// File: SamplerVoice.h
// 32音ポリフォニックボイス (Lagrange3次補間 + 等パワーXFade)
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
    float pan = 0.0f;
    float slotGainDb = 0.0f;
    float speed = 1.0f;
    bool isLooping = false;

    float sampleStartRatio = 0.0f;
    float sampleEndRatio = 1.0f;
    float loopStartRatio = 0.0f;
    float loopLengthRatio = 1.0f;
    float crossfadeRatio = 0.05f;
};

class PicoVoice
{
public:
    PicoVoice() = default;

    void prepare(double sr) noexcept
    {
        sampleRate = sr > 1000.0 ? sr : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        active = false;
        releasing = false;
        slotIndex = -1;
        midiNote = 60;
        velocity = 0.8f;
        readPosition = 0.0;
        envValue = 0.0f;
        envStage = EnvStage::Idle;
        ageCounter = 0;
    }

    void startNote(int midiNoteNumber, float noteVelocity, int slotIdx,
                   const SampleSlot& slot, const SamplerVoiceParams& p) noexcept;

    void stopNote(bool allowTailOff) noexcept;

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples,
                         const SampleSlot& slot, const SamplerVoiceParams& p) noexcept;

    bool isActive() const noexcept { return active; }
    bool isReleasing() const noexcept { return releasing; }
    int getMidiNote() const noexcept { return midiNote; }
    int getSlotIndex() const noexcept { return slotIndex; }
    uint64_t getAge() const noexcept { return ageCounter; }

private:
    enum class EnvStage { Idle, Attack, Decay, Sustain, Release };

    static inline float lagrange3rdInterpolate(float y0, float y1, float y2, float y3, float frac) noexcept
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
    int slotIndex = -1;
    int midiNote = 60;
    float velocity = 0.8f;
    double readPosition = 0.0;

    EnvStage envStage = EnvStage::Idle;
    float envValue = 0.0f;
    uint64_t ageCounter = 0;
};
