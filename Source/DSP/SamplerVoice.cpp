// ==========================================
// File: SamplerVoice.cpp
// PicoVoice レンダリング実装 (Reverseオン時の反転波形正規化レスポンス対応)
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

    if (!slot.isReady())
    {
        active = false;
        return;
    }

    const auto& meta = slot.getMetadata();
    const int effectiveRoot = (p.rootKeyOverride >= 0) ? p.rootKeyOverride : meta.rootKey;
    const int stOffset = midiNoteNumber - effectiveRoot + (p.octave * 12) + p.semitone;

    const auto* buffer = p.isStretchMode ? slot.getAnchorBuffer(stOffset) : &slot.getOriginalBuffer();

    if (!buffer || buffer->getNumSamples() < 4 || buffer->getNumChannels() < 1)
    {
        active = false;
        return;
    }

    const int bufLen = buffer->getNumSamples();
    const float startR = juce::jlimit(0.0f, 1.0f, p.sampleStartRatio);
    const float endR   = juce::jlimit(0.01f, 1.0f, p.sampleEndRatio);

    if (p.isReverse)
    {
        // Reverse ON: 画面上の左端 (sampleStartRatio) はファイル末尾側の 1.0 - startR
        readPosition = (double)juce::jlimit(0.0f, (float)(bufLen - 1), (float)bufLen * (1.0f - startR));
    }
    else
    {
        readPosition = (double)juce::jlimit(0.0f, (float)(bufLen - 1), (float)bufLen * startR);
    }

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
    if (!active || !slot.isReady()) return;

    const auto& meta = slot.getMetadata();
    const int effectiveRoot = (p.rootKeyOverride >= 0) ? p.rootKeyOverride : meta.rootKey;
    const int stOffset = midiNote - effectiveRoot + (p.octave * 12) + p.semitone;

    const auto* buffer = p.isStretchMode ? slot.getAnchorBuffer(stOffset) : &slot.getOriginalBuffer();

    if (!buffer || buffer->getNumSamples() < 4 || buffer->getNumChannels() < 1)
    {
        active = false;
        return;
    }

    const int bufLen = buffer->getNumSamples();
    const int numCh = buffer->getNumChannels();

    const double fileSR = meta.fileSampleRate > 1000.0 ? meta.fileSampleRate : 44100.0;
    const double srRatio = fileSR / sampleRate;

    if (p.isStretchMode)
    {
        const int anchorSemis = juce::jlimit(-24, 24, stOffset);
        const float residualSemis = (float)(stOffset - anchorSemis) + (p.fineTune / 100.0f);
        const double pitchRatio = std::pow(2.0, (double)residualSemis / 12.0);
        pitchInc = srRatio * pitchRatio;
    }
    else
    {
        const float totalSemis = (float)stOffset + (p.fineTune / 100.0f);
        const double pitchRatio = std::pow(2.0, (double)totalSemis / 12.0);
        pitchInc = srRatio * pitchRatio;
    }

    // マーカー・再生制御
    static constexpr int kMinSpan = 32;

    int smpStartFile = 0, smpEndFile = bufLen - 1;
    int lpStartFile = 0, lpEndFile = bufLen - 1;

    if (!p.isReverse)
    {
        smpStartFile = (int)(bufLen * juce::jlimit(0.0f, 0.98f, p.sampleStartRatio));
        smpEndFile   = (int)(bufLen * juce::jlimit(0.02f, 1.0f, p.sampleEndRatio));
        if (smpEndFile <= smpStartFile + kMinSpan)
            smpEndFile = std::min(bufLen - 1, smpStartFile + kMinSpan);

        lpStartFile  = (int)(bufLen * juce::jlimit(0.0f, 0.98f, p.loopStartRatio));
        lpEndFile    = (int)(bufLen * juce::jlimit(0.02f, 1.0f, p.loopEndRatio));
        lpStartFile  = juce::jlimit(smpStartFile, smpEndFile - kMinSpan, lpStartFile);
        lpEndFile    = juce::jlimit(lpStartFile + kMinSpan, smpEndFile, lpEndFile);
    }
    else
    {
        // Reverse ON
        smpStartFile = (int)(bufLen * (1.0f - juce::jlimit(0.0f, 0.98f, p.sampleStartRatio)));
        smpEndFile   = (int)(bufLen * (1.0f - juce::jlimit(0.02f, 1.0f, p.sampleEndRatio)));
        if (smpStartFile <= smpEndFile + kMinSpan)
            smpStartFile = std::min(bufLen - 1, smpEndFile + kMinSpan);

        lpStartFile  = (int)(bufLen * (1.0f - juce::jlimit(0.0f, 0.98f, p.loopStartRatio)));
        lpEndFile    = (int)(bufLen * (1.0f - juce::jlimit(0.02f, 1.0f, p.loopEndRatio)));
        lpStartFile  = juce::jlimit(smpEndFile + kMinSpan, smpStartFile, lpStartFile);
        lpEndFile    = juce::jlimit(smpEndFile, lpStartFile - kMinSpan, lpEndFile);

        pitchInc = -pitchInc; // 逆再生方向へ進行
    }

    const int lpLen = std::max(kMinSpan, std::abs(lpEndFile - lpStartFile));
    const int xfadeLen = juce::jlimit(8, lpLen / 2, (int)(lpLen * juce::jlimit(0.0f, 0.5f, p.crossfadeRatio)));

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

        if (!p.isReverse)
        {
            if (p.isLooping)
            {
                while (readPosition >= (double)lpEndFile)
                    readPosition -= (double)std::max(1, lpEndFile - lpStartFile);
                if (readPosition < (double)lpStartFile)
                    readPosition = (double)lpStartFile;
            }
            else
            {
                if (readPosition >= (double)smpEndFile) { envStage = EnvStage::Release; }
            }
        }
        else
        {
            // Reverse ON: ファイル上では減少方向へ進む
            if (p.isLooping)
            {
                while (readPosition <= (double)lpEndFile)
                    readPosition += (double)std::max(1, lpStartFile - lpEndFile);
                if (readPosition > (double)lpStartFile)
                    readPosition = (double)lpStartFile;
            }
            else
            {
                if (readPosition <= (double)smpEndFile) { envStage = EnvStage::Release; }
            }
        }

        const int iPos = (int)readPosition;
        const float frac = (float)(readPosition - (double)iPos);

        const int i0 = juce::jlimit(0, bufLen - 1, iPos - 1);
        const int i1 = juce::jlimit(0, bufLen - 1, iPos);
        const int i2 = juce::jlimit(0, bufLen - 1, iPos + 1);
        const int i3 = juce::jlimit(0, bufLen - 1, iPos + 2);

        float sL = 0.0f, sR = 0.0f;
        const float* chL = buffer->getReadPointer(0);
        if (chL == nullptr) { active = false; break; }

        if (numCh >= 2)
        {
            const float* chR = buffer->getReadPointer(1);
            if (chR == nullptr) chR = chL;
            sL = lagrange3rdInterpolate(chL[i0], chL[i1], chL[i2], chL[i3], frac);
            sR = lagrange3rdInterpolate(chR[i0], chR[i1], chR[i2], chR[i3], frac);
        }
        else
        {
            sL = sR = lagrange3rdInterpolate(chL[i0], chL[i1], chL[i2], chL[i3], frac);
        }

        // ループ Crossfade
        if (p.isLooping)
        {
            bool isCrossfading = false;
            double loopOffsetPos = 0.0;
            float xfFactor = 0.0f;

            if (!p.isReverse && readPosition >= (double)(lpEndFile - xfadeLen))
            {
                isCrossfading = true;
                xfFactor = (float)(readPosition - (double)(lpEndFile - xfadeLen)) / (float)xfadeLen;
                loopOffsetPos = readPosition - (double)(lpEndFile - lpStartFile);
            }
            else if (p.isReverse && readPosition <= (double)(lpEndFile + xfadeLen))
            {
                isCrossfading = true;
                xfFactor = (float)((double)(lpEndFile + xfadeLen) - readPosition) / (float)xfadeLen;
                loopOffsetPos = readPosition + (double)(lpStartFile - lpEndFile);
            }

            if (isCrossfading)
            {
                const float fadeOut = std::cos(xfFactor * juce::MathConstants<float>::halfPi);
                const float fadeIn  = std::sin(xfFactor * juce::MathConstants<float>::halfPi);

                const int li0 = juce::jlimit(0, bufLen - 1, (int)loopOffsetPos - 1);
                const int li1 = juce::jlimit(0, bufLen - 1, (int)loopOffsetPos);
                const int li2 = juce::jlimit(0, bufLen - 1, (int)loopOffsetPos + 1);
                const int li3 = juce::jlimit(0, bufLen - 1, (int)loopOffsetPos + 2);

                const float* chR = numCh >= 2 ? buffer->getReadPointer(1) : chL;
                if (chR == nullptr) chR = chL;
                const float inL = lagrange3rdInterpolate(chL[li0], chL[li1], chL[li2], li3 < bufLen ? chL[li3] : chL[li2], frac);
                const float inR = lagrange3rdInterpolate(chR[li0], chR[li1], chR[li2], li3 < bufLen ? chR[li3] : chR[li2], frac);

                sL = sL * fadeOut + inL * fadeIn;
                sR = sR * fadeOut + inR * fadeIn;
            }
        }

        outL[s] += sL * gainL * envValue;
        outR[s] += sR * gainR * envValue;

        readPosition += pitchInc;
    }
}
