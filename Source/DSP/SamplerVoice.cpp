// ==========================================
// File: SamplerVoice.cpp
// PicoVoice レンダリング実装 (Repitch / StretchMode 分岐対応)
// ==========================================
#include "SamplerVoice.h"

void PicoVoice::startNote(int midiNoteNumber, float noteVelocity, int slotIdx,
                          const SampleSlot& slot, const SamplerVoiceParams& p) noexcept
{
    midiNote = midiNoteNumber;
    velocity = noteVelocity;
    slotIndex = slotIdx;
    active = true;
    releasing = false;
    ageCounter++;

    const auto& meta = slot.getMetadata();
    const int stOffset = midiNoteNumber - meta.rootKey + (p.octave * 12) + p.semitone;
    const auto* buffer = slot.getAnchorBuffer(stOffset);

    if (!buffer || buffer->getNumSamples() == 0)
    {
        active = false;
        return;
    }

    const int bufLen = buffer->getNumSamples();
    const int startSmp = (int)(bufLen * juce::jlimit(0.0f, 1.0f, p.sampleStartRatio));
    readPosition = (double)startSmp;

    if (p.attack <= 0.005f)
    {
        envStage = EnvStage::Decay;
        envValue = 1.0f;
    }
    else
    {
        envStage = EnvStage::Attack;
        envValue = 0.0f;
    }
}

void PicoVoice::stopNote(bool allowTailOff) noexcept
{
    if (!allowTailOff)
    {
        reset();
    }
    else
    {
        releasing = true;
        envStage = EnvStage::Release;
    }
}

void PicoVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples,
                                 const SampleSlot& slot, const SamplerVoiceParams& p) noexcept
{
    if (!active) return;

    const auto& meta = slot.getMetadata();
    const int stOffset = midiNote - meta.rootKey + (p.octave * 12) + p.semitone;

    const int anchorIdx = juce::jlimit(0, SampleSlot::kNumAnchors - 1, stOffset + 12);
    const int anchorSemis = anchorIdx - 12;
    const float residualSemis = (float)(stOffset - anchorSemis) + (p.fineTune / 100.0f);

    const auto* buffer = slot.getAnchorBuffer(stOffset);

    if (!buffer || buffer->getNumSamples() < 4)
    {
        active = false;
        return;
    }

    const int bufLen = buffer->getNumSamples();
    const int numCh = buffer->getNumChannels();

    const int smpEnd   = (int)(bufLen * juce::jlimit(0.01f, 1.0f, p.sampleEndRatio));
    const int lpStart  = (int)(bufLen * juce::jlimit(0.0f, 0.99f, p.loopStartRatio));
    const int lpLen    = juce::jmax(64, (int)(bufLen * juce::jlimit(0.01f, 1.0f, p.loopLengthRatio)));
    const int lpEnd    = std::min(smpEnd, lpStart + lpLen);
    const int xfadeLen = juce::jmax(1, (int)(lpLen * juce::jlimit(0.0f, 0.5f, p.crossfadeRatio)));

    // 再生インクリメント: StretchMode 時はピッチに依らず時間不変 (1.0 * speed)
    double pitchInc = (double)p.speed;
    if (!p.isStretchMode)
    {
        const double pitchRatio = std::pow(2.0, (double)residualSemis / 12.0);
        pitchInc *= pitchRatio;
    }

    const float pan = juce::jlimit(-1.0f, 1.0f, p.pan);
    const float gainL = std::sqrt(0.5f * (1.0f - pan)) * velocity * juce::Decibels::decibelsToGain(p.slotGainDb);
    const float gainR = std::sqrt(0.5f * (1.0f + pan)) * velocity * juce::Decibels::decibelsToGain(p.slotGainDb);

    const float blockSec = 1.0f / (float)sampleRate;
    float* outL = outputBuffer.getWritePointer(0, startSample);
    float* outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1, startSample) : outL;

    for (int s = 0; s < numSamples; ++s)
    {
        switch (envStage)
        {
        case EnvStage::Attack:
            envValue += blockSec / juce::jmax(0.001f, p.attack);
            if (envValue >= 1.0f) { envValue = 1.0f; envStage = EnvStage::Decay; }
            break;
        case EnvStage::Decay:
            envValue -= (1.0f - p.sustain) * blockSec / juce::jmax(0.001f, p.decay);
            if (envValue <= p.sustain) { envValue = p.sustain; envStage = EnvStage::Sustain; }
            break;
        case EnvStage::Sustain:
            envValue = p.sustain;
            break;
        case EnvStage::Release:
            envValue -= blockSec / juce::jmax(0.001f, p.release);
            if (envValue <= 0.0f) { envValue = 0.0f; envStage = EnvStage::Idle; active = false; break; }
            break;
        default:
            active = false;
            break;
        }

        if (!active) break;

        if (p.isLooping)
        {
            if (readPosition >= (double)lpEnd)
            {
                readPosition -= (double)(lpEnd - lpStart);
            }
        }
        else
        {
            if (readPosition >= (double)smpEnd)
            {
                envStage = EnvStage::Release;
            }
        }

        const int iPos = (int)readPosition;
        const float frac = (float)(readPosition - (double)iPos);

        const int i0 = juce::jlimit(0, bufLen - 1, iPos - 1);
        const int i1 = juce::jlimit(0, bufLen - 1, iPos);
        const int i2 = juce::jlimit(0, bufLen - 1, iPos + 1);
        const int i3 = juce::jlimit(0, bufLen - 1, iPos + 2);

        float sL = 0.0f, sR = 0.0f;
        if (numCh >= 2)
        {
            const float* chL = buffer->getReadPointer(0);
            const float* chR = buffer->getReadPointer(1);
            sL = lagrange3rdInterpolate(chL[i0], chL[i1], chL[i2], chL[i3], frac);
            sR = lagrange3rdInterpolate(chR[i0], chR[i1], chR[i2], chR[i3], frac);
        }
        else
        {
            const float* chL = buffer->getReadPointer(0);
            sL = sR = lagrange3rdInterpolate(chL[i0], chL[i1], chL[i2], chL[i3], frac);
        }

        if (p.isLooping && readPosition >= (double)(lpEnd - xfadeLen))
        {
            const float xfFactor = (float)(readPosition - (double)(lpEnd - xfadeLen)) / (float)xfadeLen;
            const float fadeOut = std::cos(xfFactor * juce::MathConstants<float>::halfPi);
            const float fadeIn  = std::sin(xfFactor * juce::MathConstants<float>::halfPi);

            const int loopOffsetPos = (int)(readPosition - (double)(lpEnd - lpStart));
            const int li0 = juce::jlimit(0, bufLen - 1, loopOffsetPos - 1);
            const int li1 = juce::jlimit(0, bufLen - 1, loopOffsetPos);
            const int li2 = juce::jlimit(0, bufLen - 1, loopOffsetPos + 1);
            const int li3 = juce::jlimit(0, bufLen - 1, loopOffsetPos + 2);

            const float* chL = buffer->getReadPointer(0);
            const float* chR = numCh >= 2 ? buffer->getReadPointer(1) : chL;
            const float inL = lagrange3rdInterpolate(chL[li0], chL[li1], chL[li2], chL[li3], frac);
            const float inR = lagrange3rdInterpolate(chR[li0], chR[li1], chR[li2], chR[li3], frac);

            sL = sL * fadeOut + inL * fadeIn;
            sR = sR * fadeOut + inR * fadeIn;
        }

        outL[s] += sL * gainL * envValue;
        outR[s] += sR * gainR * envValue;

        readPosition += pitchInc;
    }
}
