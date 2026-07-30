// ==========================================
// File: SamplerVoice.cpp
// PicoVoice レンダリング実装 (Reverseオン時の反転波形正規化レスポンス対応)
// ==========================================
#include "SamplerVoice.h"

void PicoVoice::startNote(int midiNoteNumber, float noteVelocity, int slotIdx,
                          const SampleSlot& slot, const SamplerVoiceParams& p) noexcept
{
    bool isLegato = p.portaEnable && active && !releasing && (slotIndex == slotIdx);

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

    if (!isLegato)
    {
        if (p.isStretchMode) currentAnchorSemis = juce::jlimit(-24, 24, stOffset);
        else currentAnchorSemis = 0;
    }

    const auto* buffer = p.isStretchMode ? slot.getAnchorBuffer(currentAnchorSemis) : &slot.getOriginalBuffer();

    if (!buffer || buffer->getNumSamples() < 4 || buffer->getNumChannels() < 1)
    {
        active = false;
        return;
    }

    if (!isLegato)
    {
        if (p.portaEnable && p.portaStartMidiNote >= 0.0f)
        {
            float startSemis = 0.0f;
            if (p.isStretchMode)
            {
                startSemis = p.portaStartMidiNote - (float)effectiveRoot + (p.octave * 12) + p.semitone - currentAnchorSemis + (p.fineTune / 100.0f);
            }
            else
            {
                startSemis = p.portaStartMidiNote - (float)effectiveRoot + (p.octave * 12) + p.semitone + (p.fineTune / 100.0f);
            }
            currentPitchRatio = std::pow(2.0, (double)startSemis / 12.0);
        }
        else
        {
            if (p.isStretchMode)
            {
                currentPitchRatio = std::pow(2.0, (double)(stOffset - currentAnchorSemis + (p.fineTune / 100.0f)) / 12.0);
            }
            else
            {
                currentPitchRatio = std::pow(2.0, (double)(stOffset + (p.fineTune / 100.0f)) / 12.0);
            }
        }

        const int bufLen = buffer->getNumSamples();
        const float startR = juce::jlimit(0.0f, 1.0f, p.sampleStartRatio);

        if (p.isReverse)
        {
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

        // Loop ON時でも、まず Start -> End (L-Endではない) の初回パスを再生してから
        // L-Start/L-End 間のループに入る仕様。Legato で音を継続する場合は
        // 既にループに入っているかもしれないため状態を維持する。
        hasEnteredLoop = false;
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

    // このブロックを描画し終えるまでスロットのバッファが解放されないよう保持する。
    // 掴めなかった (= ロード中 / クリア中) 場合は何も出さずに戻る。
    const SampleSlot::ReadGuard slotGuard(slot);
    if (!slotGuard.isValid()) return;

    const auto& meta = slot.getMetadata();
    const int effectiveRoot = (p.rootKeyOverride >= 0) ? p.rootKeyOverride : meta.rootKey;
    const int stOffset = midiNote - effectiveRoot + (p.octave * 12) + p.semitone;

    const int anchorSemis = p.isStretchMode ? currentAnchorSemis : 0;
    const auto* buffer = p.isStretchMode ? slot.getAnchorBuffer(anchorSemis) : &slot.getOriginalBuffer();

    if (!buffer || buffer->getNumSamples() < 4 || buffer->getNumChannels() < 1)
    {
        active = false;
        return;
    }

    const int bufLen = buffer->getNumSamples();
    const int numCh = buffer->getNumChannels();

    const double fileSR = meta.fileSampleRate > 1000.0 ? meta.fileSampleRate : 44100.0;
    const double srRatio = fileSR / sampleRate;

    double targetPitchRatio = 1.0;
    if (p.isStretchMode)
    {
        const float residualSemis = (float)(stOffset - anchorSemis) + (p.fineTune / 100.0f);
        targetPitchRatio = std::pow(2.0, (double)residualSemis / 12.0);
    }
    else
    {
        const float totalSemis = (float)stOffset + (p.fineTune / 100.0f);
        targetPitchRatio = std::pow(2.0, (double)totalSemis / 12.0);
    }

    double portaMultiplier = 1.0;
    if (p.portaEnable && currentPitchRatio != targetPitchRatio)
    {
        const double timeMs = juce::jmap(p.portaTime, 0.0f, 1.0f, 5.0f, 1000.0f);
        portaMultiplier = std::exp(-4.605 / ((timeMs * 0.001) * sampleRate));
    }
    else
    {
        currentPitchRatio = targetPitchRatio;
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
    }

    const int lpLen = std::max(kMinSpan, std::abs(lpEndFile - lpStartFile));
    const int xfadeLen = juce::jlimit(8, lpLen / 2, (int)(lpLen * juce::jlimit(0.0f, 0.5f, p.crossfadeRatio)));

    // ------------------------------------------------------------------
    // Edge Fade
    // Start/End マーカーが波形の途中 (振幅が 0 でない位置) に置かれると
    // 再生開始/終了の瞬間に段差が生じてプチッと鳴る。
    // その端だけを短時間フェードして段差を殺す。
    //
    // ・長さは Config の Fade In / Fade Out (ms) から算出。
    // ・区間長の 1/3 を超えないようにクランプ (極短スライスで音が消えるのを防ぐ)。
    // ・カーブは raised cosine。直線フェードより可聴なカドが出にくい。
    // ------------------------------------------------------------------
    const int spanLen = std::max(kMinSpan, std::abs(smpEndFile - smpStartFile));
    const int maxFade = std::max(1, spanLen / 3);

    const int fadeInLen  = juce::jlimit(0, maxFade,
                              (int)(juce::jmax(0.0f, p.edgeFadeInMs)  * 0.001f * (float)sampleRate));
    const int fadeOutLen = juce::jlimit(0, maxFade,
                              (int)(juce::jmax(0.0f, p.edgeFadeOutMs) * 0.001f * (float)sampleRate));

    // ループ中は End 側をループ Crossfade が担当するので Fade Out は掛けない
    const bool applyFadeOut = (fadeOutLen > 0) && !p.isLooping;
    const bool applyFadeIn  = (fadeInLen > 0);

    auto raisedCos = [](float t) noexcept
    {
        t = juce::jlimit(0.0f, 1.0f, t);
        return 0.5f - 0.5f * std::cos(t * juce::MathConstants<float>::pi);
    };

    const float pan = juce::jlimit(-1.0f, 1.0f, p.pan);
    const float gainL = std::sqrt(0.5f * (1.0f - pan)) * velocity * juce::Decibels::decibelsToGain(p.slotGainDb);
    const float gainR = std::sqrt(0.5f * (1.0f + pan)) * velocity * juce::Decibels::decibelsToGain(p.slotGainDb);

    const float blockSec = 1.0f / (float)sampleRate;
    float* outL = outputBuffer.getWritePointer(0, startSample);
    float* outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1, startSample) : outL;

    for (int s = 0; s < numSamples; ++s)
    {
        if (p.portaEnable && currentPitchRatio != targetPitchRatio)
        {
            currentPitchRatio = targetPitchRatio + (currentPitchRatio - targetPitchRatio) * portaMultiplier;
            if (std::abs(currentPitchRatio - targetPitchRatio) < 0.0001) currentPitchRatio = targetPitchRatio;
        }
        pitchInc = srRatio * currentPitchRatio;
        if (p.isReverse) pitchInc = -pitchInc;

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
                // 仕様: Note-On直後はまず Start -> End (Endマーカーまで) の初回パスを
                // そのまま再生し、Endに到達して初めて L-Start/L-End 間のループに入る。
                // hasEnteredLoop が立つまでは lpEndFile ではなく smpEndFile を境界に使う。
                const int wrapBoundary = hasEnteredLoop ? lpEndFile : smpEndFile;
                if (readPosition >= (double)wrapBoundary)
                {
                    hasEnteredLoop = true;
                    while (readPosition >= (double)lpEndFile)
                        readPosition -= (double)std::max(1, lpEndFile - lpStartFile);
                    if (readPosition < (double)lpStartFile)
                        readPosition = (double)lpStartFile;
                }
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
                const int wrapBoundary = hasEnteredLoop ? lpEndFile : smpEndFile;
                if (readPosition <= (double)wrapBoundary)
                {
                    hasEnteredLoop = true;
                    while (readPosition <= (double)lpEndFile)
                        readPosition += (double)std::max(1, lpStartFile - lpEndFile);
                    if (readPosition > (double)lpStartFile)
                        readPosition = (double)lpStartFile;
                }
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

        // ループ Crossfade (初回の Start->End パス中はまだループしていないので対象外)
        if (p.isLooping && hasEnteredLoop)
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

        // --- Edge Fade 適用 ---
        // Reverse 時はファイル上を逆走するので、距離の測り方も反転させる。
        if (applyFadeIn || applyFadeOut)
        {
            float edgeGain = 1.0f;

            const double distFromStart = p.isReverse ? ((double)smpStartFile - readPosition)
                                                     : (readPosition - (double)smpStartFile);
            const double distToEnd     = p.isReverse ? (readPosition - (double)smpEndFile)
                                                     : ((double)smpEndFile - readPosition);

            if (applyFadeIn && distFromStart >= 0.0 && distFromStart < (double)fadeInLen)
                edgeGain *= raisedCos((float)(distFromStart / (double)fadeInLen));

            if (applyFadeOut && distToEnd >= 0.0 && distToEnd < (double)fadeOutLen)
                edgeGain *= raisedCos((float)(distToEnd / (double)fadeOutLen));

            sL *= edgeGain;
            sR *= edgeGain;
        }

        outL[s] += sL * gainL * envValue;
        outR[s] += sR * gainR * envValue;

        readPosition += pitchInc;
    }
}
