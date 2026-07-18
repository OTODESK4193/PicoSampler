// ==========================================
// File: SamplerVoice.cpp
// PicoVoice レンダリング実装 (サンプルレート補正 & 正確なピッチマッピング)
// 
// ★ 根本バグ修正:
//   1. fileSR / engineSR 補正係数を pitchInc のベースに適用
//   2. rootKey から MIDI ノートへの変換は純粋な半音差のみ
//   3. Stretch/Repitch で同一の音階を保証
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
    // rootKeyOverride >= 0 の場合はユーザー手動設定を優先
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
        readPosition = (double)juce::jlimit(0.0f, (float)(bufLen - 1), (float)bufLen * endR - 1.0f);
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

    const int smpStart = (int)(bufLen * juce::jlimit(0.0f, 0.99f, p.sampleStartRatio));
    const int smpEnd   = (int)(bufLen * juce::jlimit(0.01f, 1.0f, p.sampleEndRatio));
    const int lpStart  = (int)(bufLen * juce::jlimit(0.0f, 0.99f, p.loopStartRatio));
    const int lpEnd    = std::min(smpEnd, (int)(bufLen * juce::jlimit(0.01f, 1.0f, p.loopEndRatio)));
    const int lpLen    = std::max(64, lpEnd - lpStart);
    const int xfadeLen = juce::jmax(1, (int)(lpLen * juce::jlimit(0.0f, 0.5f, p.crossfadeRatio)));

    // ★ サンプルレート補正: ファイルのSRとエンジンのSRの比率をベースレートに適用
    const double fileSR = meta.fileSampleRate > 1000.0 ? meta.fileSampleRate : 44100.0;
    const double srRatio = fileSR / sampleRate;  // sampleRate はエンジンのSR (PicoVoice::prepare で設定)

    if (p.isStretchMode)
    {
        // Stretch モード: アンカー(事前ピッチシフト済みバッファ)を原速再生
        // アンカー範囲外の残差のみピッチシフト + SR補正
        const int anchorSemis = juce::jlimit(-12, 11, stOffset);
        const float residualSemis = (float)(stOffset - anchorSemis) + (p.fineTune / 100.0f);
        const double pitchRatio = std::pow(2.0, (double)residualSemis / 12.0);
        pitchInc = srRatio * pitchRatio;
    }
    else
    {
        // Repitch モード: 原音バッファから直接リサンプリング再生
        // midiNote と rootKey の半音差 + fineTune を SR 補正付きで適用
        const float totalSemis = (float)stOffset + (p.fineTune / 100.0f);
        const double pitchRatio = std::pow(2.0, (double)totalSemis / 12.0);
        pitchInc = srRatio * pitchRatio;
    }

    if (p.isReverse) pitchInc = -pitchInc;

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
                if (readPosition >= (double)lpEnd) readPosition -= (double)(lpEnd - lpStart);
            }
            else
            {
                if (readPosition >= (double)smpEnd) { envStage = EnvStage::Release; }
            }
        }
        else
        {
            if (p.isLooping)
            {
                if (readPosition <= (double)lpStart) readPosition += (double)(lpEnd - lpStart);
            }
            else
            {
                if (readPosition <= (double)smpStart) { envStage = EnvStage::Release; }
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

        // 滑らかなループ Crossfade (ループ終端手前の X-Fade 区間で
        // フェードアウト → ループ開始点のフェードインを等パワークロスフェード)
        if (p.isLooping && !p.isReverse && readPosition >= (double)(lpEnd - xfadeLen))
        {
            const float xfFactor = (float)(readPosition - (double)(lpEnd - xfadeLen)) / (float)xfadeLen;
            const float fadeOut = std::cos(xfFactor * juce::MathConstants<float>::halfPi);
            const float fadeIn  = std::sin(xfFactor * juce::MathConstants<float>::halfPi);

            const int loopOffsetPos = (int)(readPosition - (double)(lpEnd - lpStart));
            const int li0 = juce::jlimit(0, bufLen - 1, loopOffsetPos - 1);
            const int li1 = juce::jlimit(0, bufLen - 1, loopOffsetPos);
            const int li2 = juce::jlimit(0, bufLen - 1, loopOffsetPos + 1);
            const int li3 = juce::jlimit(0, bufLen - 1, loopOffsetPos + 2);

            const float* chR = numCh >= 2 ? buffer->getReadPointer(1) : chL;
            if (chR == nullptr) chR = chL;
            const float inL = lagrange3rdInterpolate(chL[li0], chL[li1], chL[li2], li3 < bufLen ? chL[li3] : chL[li2], frac);
            const float inR = lagrange3rdInterpolate(chR[li0], chR[li1], chR[li2], li3 < bufLen ? chR[li3] : chR[li2], frac);

            sL = sL * fadeOut + inL * fadeIn;
            sR = sR * fadeOut + inR * fadeIn;
        }

        outL[s] += sL * gainL * envValue;
        outR[s] += sR * gainR * envValue;

        readPosition += pitchInc;
    }
}
