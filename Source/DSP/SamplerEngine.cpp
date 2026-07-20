// ==========================================
// File: SamplerEngine.cpp
// サンプラーエンジン (BrickLimiter適用)
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
                const auto& sp = p.slotParams[(size_t)p.activeSlot];
                if (slots[(size_t)p.activeSlot].isReady() && note >= sp.lowNote && note <= sp.highNote)
                {
                    triggerSlotNote(p.activeSlot, note, vel, p);
                }
                break;
            }
            case LayerMode:
            {
                for (int slotIdx = 0; slotIdx < NUM_SLOTS; ++slotIdx)
                {
                    const auto& sp = p.slotParams[(size_t)slotIdx];
                    if (slots[(size_t)slotIdx].isReady() && note >= sp.lowNote && note <= sp.highNote)
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
                    const auto& sp = p.slotParams[(size_t)i];
                    if (slots[(size_t)i].isReady() && note >= sp.lowNote && note <= sp.highNote)
                    {
                        readySlots.push_back(i);
                    }
                }
                if (!readySlots.empty())
                {
                    int chosen = readySlots[0];
                    if (readySlots.size() > 1)
                    {
                        std::vector<int> filtered;
                        for (int sIdx : readySlots)
                        {
                            if (sIdx != lastRandomSlot) filtered.push_back(sIdx);
                        }
                        if (!filtered.empty())
                            chosen = filtered[(size_t)rng.nextInt((int)filtered.size())];
                        else
                            chosen = readySlots[(size_t)rng.nextInt((int)readySlots.size())];
                    }
                    lastRandomSlot = chosen;
                    triggerSlotNote(chosen, note, vel, p);
                    if (onActiveSlotTriggered) onActiveSlotTriggered(chosen);
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

    auto voiceP = p.slotParams[(size_t)slotIdx];
    voiceP.portaEnable = p.portaEnable;
    voiceP.portaTime = p.portaTime;
    voiceP.portaStartMidiNote = lastPlayedNote;

    voices[(size_t)voiceIdx].startNote(midiNote, velocity, slotIdx, slot, voiceP);
    
    lastPlayedNote = (float)midiNote;
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

void SamplerEngine::renderNextBlock(juce::AudioBuffer<float>& normalBuffer,
                                     juce::AudioBuffer<float>& fltBypassBuffer,
                                     juce::AudioBuffer<float>& fxBypassBuffer,
                                     juce::AudioBuffer<float>& bothBypassBuffer,
                                     const Params& p,
                                     SampleVisualizerData* visualizerData) noexcept
{
    const int numSamples = normalBuffer.getNumSamples();

    for (size_t i = 0; i < NUM_VOICES; ++i)
    {
        auto& v = voices[i];
        if (v.isActive())
        {
            const int slotIdx = v.getSlotIndex();
            if (slotIdx >= 0 && slotIdx < NUM_SLOTS)
            {
                const auto& sp = p.slotParams[(size_t)slotIdx];
                
                juce::AudioBuffer<float>& targetBuf = 
                    (sp.isFilterBypass && sp.isFxBypass)  ? bothBypassBuffer :
                    (sp.isFilterBypass && !sp.isFxBypass) ? fltBypassBuffer :
                    (!sp.isFilterBypass && sp.isFxBypass) ? fxBypassBuffer :
                                                            normalBuffer;

                v.renderNextBlock(targetBuf, 0, numSamples, slots[(size_t)slotIdx], sp);

                if (visualizerData && v.isActive())
                {
                    visualizerData->push(slotIdx, 0.5f, sp.pan, 0.0f, 0.1f);
                }
            }
        }
    }

    filterL_HP.setCutoffAndType(p.masterHpfHz, TptSvfFilter::HighPass, p.is24dBFilter);
    filterR_HP.setCutoffAndType(p.masterHpfHz, TptSvfFilter::HighPass, p.is24dBFilter);
    filterL_LP.setCutoffAndType(p.masterLpfHz, TptSvfFilter::LowPass, p.is24dBFilter);
    filterR_LP.setCutoffAndType(p.masterLpfHz, TptSvfFilter::LowPass, p.is24dBFilter);

    limiter.setReleaseMs(p.limReleaseMs);
    const float ceilingLin = juce::Decibels::decibelsToGain(p.ceilingDb);

    float* outL = normalBuffer.getWritePointer(0);
    float* outR = normalBuffer.getNumChannels() > 1 ? normalBuffer.getWritePointer(1) : outL;
    const float masterGain = juce::Decibels::decibelsToGain(p.outGainDb);

    for (int s = 0; s < numSamples; ++s)
    {
        float l = filterL_HP.processSample(outL[s]);
        l = filterL_LP.processSample(l);

        float r = filterR_HP.processSample(outR[s]);
        r = filterR_LP.processSample(r);

        l *= masterGain;
        r *= masterGain;

        limiter.process(l, r, ceilingLin);

        outL[s] = l;
        outR[s] = r;
    }
}
