// ==========================================
// File: SamplerEngine.cpp
// サンプラーエンジン ノートハンドリング＆レンダリング実装
// ==========================================
#include "SamplerEngine.h"

void SamplerEngine::handleMidi(const juce::MidiBuffer& midi, const Params& p) noexcept
{
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const float vel = msg.getFloatVelocity();

            switch (p.mode)
            {
            case SingleMode:
            {
                triggerSlotNote(p.activeSlot, note, vel, p);
                break;
            }
            case LayerMode:
            {
                for (int slotIdx = 0; slotIdx < NUM_SLOTS; ++slotIdx)
                {
                    const auto& meta = slots[(size_t)slotIdx].getMetadata();
                    if (slots[(size_t)slotIdx].isReady() && note >= meta.lowNote && note <= meta.highNote)
                    {
                        triggerSlotNote(slotIdx, note, vel, p);
                    }
                }
                break;
            }
            case RandomMode:
            {
                std::vector<int> readySlots;
                for (int i = 0; i < NUM_SLOTS; ++i)
                {
                    if (slots[(size_t)i].isReady()) readySlots.push_back(i);
                }
                if (!readySlots.empty())
                {
                    const int chosen = readySlots[(size_t)rng.nextInt((int)readySlots.size())];
                    triggerSlotNote(chosen, note, vel, p);
                }
                break;
            }
            }
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            for (auto& v : voices)
            {
                if (v.isActive() && v.getMidiNote() == note)
                {
                    v.stopNote(true);
                }
            }
        }
        else if (msg.isAllNotesOff())
        {
            for (auto& v : voices) v.stopNote(false);
        }
    }
}

void SamplerEngine::triggerSlotNote(int slotIdx, int midiNote, float velocity, const Params& p) noexcept
{
    if (slotIdx < 0 || slotIdx >= NUM_SLOTS) return;
    const auto& slot = slots[(size_t)slotIdx];
    if (!slot.isReady()) return;

    int voiceIdx = findFreeVoice();
    if (voiceIdx < 0) voiceIdx = findOldestVoice();
    if (voiceIdx < 0) return;

    const auto& voiceP = p.slotParams[(size_t)slotIdx];
    voices[(size_t)voiceIdx].startNote(midiNote, velocity, slotIdx, slot, voiceP);
}

int SamplerEngine::findFreeVoice() const noexcept
{
    for (int i = 0; i < NUM_VOICES; ++i)
    {
        if (!voices[(size_t)i].isActive()) return i;
    }
    return -1;
}

int SamplerEngine::findOldestVoice() const noexcept
{
    int oldestIdx = -1;
    uint64_t maxAge = 0;

    for (int i = 0; i < NUM_VOICES; ++i)
    {
        if (voices[(size_t)i].isActive())
        {
            const uint64_t age = voices[(size_t)i].getAge();
            if (oldestIdx == -1 || age > maxAge)
            {
                maxAge = age;
                oldestIdx = i;
            }
        }
    }
    return oldestIdx;
}

void SamplerEngine::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, const Params& p, SampleVisualizerData* visualizerData) noexcept
{
    const int numSamples = outputBuffer.getNumSamples();
    const int numCh = outputBuffer.getNumChannels();

    for (size_t i = 0; i < NUM_VOICES; ++i)
    {
        auto& v = voices[i];
        if (v.isActive())
        {
            const int slotIdx = v.getSlotIndex();
            if (slotIdx >= 0 && slotIdx < NUM_SLOTS)
            {
                v.renderNextBlock(outputBuffer, 0, numSamples, slots[(size_t)slotIdx], p.slotParams[(size_t)slotIdx]);

                if (visualizerData && v.isActive())
                {
                    visualizerData->push(slotIdx, 0.5f, p.slotParams[(size_t)slotIdx].pan, 0.0f, 0.1f);
                }
            }
        }
    }

    filterL_HP.setCutoffAndType(p.masterHpfHz, TptSvfFilter::HighPass, p.is24dBFilter);
    filterR_HP.setCutoffAndType(p.masterHpfHz, TptSvfFilter::HighPass, p.is24dBFilter);
    filterL_LP.setCutoffAndType(p.masterLpfHz, TptSvfFilter::LowPass, p.is24dBFilter);
    filterR_LP.setCutoffAndType(p.masterLpfHz, TptSvfFilter::LowPass, p.is24dBFilter);

    float* outL = outputBuffer.getWritePointer(0);
    float* outR = numCh > 1 ? outputBuffer.getWritePointer(1) : outL;
    const float masterGain = juce::Decibels::decibelsToGain(p.outGainDb);

    for (int s = 0; s < numSamples; ++s)
    {
        float l = filterL_HP.processSample(outL[s]);
        l = filterL_LP.processSample(l);

        float r = filterR_HP.processSample(outR[s]);
        r = filterR_LP.processSample(r);

        outL[s] = l * masterGain;
        outR[s] = r * masterGain;
    }
}
