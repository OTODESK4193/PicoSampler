// ==========================================
// File: PicoFilter.cpp
// CleanSVF, Vowel (Formant), Comb フィルター実装 (QuadMorphFilter完全高精度移植)
// ==========================================
#include "PicoFilter.h"

void PicoFilter::prepare(double sampleRate, int) noexcept
{
    sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
    reset();
}

void PicoFilter::reset() noexcept
{
    std::memset(svf_s1, 0, sizeof(svf_s1));
    std::memset(svf_s2, 0, sizeof(svf_s2));
    std::memset(form_s1, 0, sizeof(form_s1));
    std::memset(form_s2, 0, sizeof(form_s2));
    std::memset(combBuffer, 0, sizeof(combBuffer));
    std::memset(combWriteIdx, 0, sizeof(combWriteIdx));
}

void PicoFilter::process(juce::AudioBuffer<float>& buffer, const Params& p) noexcept
{
    if (!p.enable) return;

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        const int safeCh = std::min(ch, 1);

        for (int s = 0; s < numSamples; ++s)
        {
            channelData[s] = processSample(safeCh, channelData[s], p);
        }
    }
}

float PicoFilter::processSample(int ch, float x, const Params& p) noexcept
{
    if (p.model == CleanSVF)
    {
        // --- 1. Clean Zavalishin TPT SVF (12dB / 24dB) ---
        const float normFc = juce::jlimit(0.0001f, 0.46f, p.cutoff / (float)sr);
        const float g = std::tan(juce::MathConstants<float>::pi * normFc);
        const float res = juce::jlimit(0.1f, 10.0f, p.res);
        const float R = 1.0f / (2.0f * res);
        const float h = 1.0f / (1.0f + 2.0f * R * g + g * g);
        const int stages = (p.slope == 0) ? 1 : 2;

        float currInput = x;
        for (int st = 0; st < stages; ++st)
        {
            // デノーム保護
            svf_s1[st][ch] = snapToZero(svf_s1[st][ch]);
            svf_s2[st][ch] = snapToZero(svf_s2[st][ch]);

            float hp = (currInput - (2.0f * R + g) * svf_s1[st][ch] - svf_s2[st][ch]) * h;
            float bp = g * hp + svf_s1[st][ch];

            // 状態飽和・ノンリニア保護 (過度なレゾナンスによる爆発・歪みをソフト制限)
            bp = bp / (1.0f + 0.15f * std::abs(bp));

            svf_s1[st][ch] = g * hp + bp;

            float lp = g * bp + svf_s2[st][ch];
            svf_s2[st][ch] = g * bp + lp;

            if (p.type == LowPass) currInput = lp;
            else if (p.type == BandPass) currInput = bp;
            else if (p.type == HighPass) currInput = hp;
            else if (p.type == Notch) currInput = hp + lp;
            else currInput = lp;
        }
        return currInput;
    }
    else if (p.model == VowelFormant)
    {
        // --- 2. Vowel Formant (A, E, I, O, U モーフィング) ---
        // 各母音の基本 3 フォルマント周波数テーブル (Hz)
        static const float vowelFreqs[5][3] = {
            { 800.0f,  1150.0f, 2900.0f }, // A
            { 350.0f,  2000.0f, 2800.0f }, // E
            { 270.0f,  2300.0f, 3000.0f }, // I
            { 450.0f,   800.0f, 2830.0f }, // O
            { 325.0f,   700.0f, 2700.0f }  // U
        };

        const float pos = juce::jlimit(0.0f, 4.0f, p.formant * 4.0f);
        const int idx = (int)pos;
        const float frac = pos - (float)idx;
        const int nextIdx = std::min(4, idx + 1);

        float f1 = juce::jmap(frac, vowelFreqs[idx][0], vowelFreqs[nextIdx][0]);
        float f2 = juce::jmap(frac, vowelFreqs[idx][1], vowelFreqs[nextIdx][1]);
        float f3 = juce::jmap(frac, vowelFreqs[idx][2], vowelFreqs[nextIdx][2]);

        // Cutoff ノブによる全体フォルマントシフト補正
        const float shift = p.cutoff / 1000.0f;
        f1 = juce::jlimit(50.0f, (float)(sr * 0.45), f1 * shift);
        f2 = juce::jlimit(50.0f, (float)(sr * 0.45), f2 * shift);
        f3 = juce::jlimit(50.0f, (float)(sr * 0.45), f3 * shift);

        const float targetRes = juce::jlimit(0.5f, 8.0f, p.res * 2.0f);

        auto processFormantBand = [&](int b, float freq, float inSample) -> float {
            form_s1[b][ch] = snapToZero(form_s1[b][ch]);
            form_s2[b][ch] = snapToZero(form_s2[b][ch]);

            float normF = juce::jlimit(0.001f, 0.46f, freq / (float)sr);
            float g = std::tan(juce::MathConstants<float>::pi * normF);
            float R = 1.0f / (2.0f * targetRes);
            float h = 1.0f / (1.0f + 2.0f * R * g + g * g);

            float hp = (inSample - (2.0f * R + g) * form_s1[b][ch] - form_s2[b][ch]) * h;
            float bp = g * hp + form_s1[b][ch];
            form_s1[b][ch] = g * hp + bp;
            float lp = g * bp + form_s2[b][ch];
            form_s2[b][ch] = g * bp + lp;
            return bp;
        };

        float out1 = processFormantBand(0, f1, x);
        float out2 = processFormantBand(1, f2, x);
        float out3 = processFormantBand(2, f3, x);

        return (out1 * 1.2f + out2 * 1.0f + out3 * 0.8f);
    }
    else if (p.model == CombFilter)
    {
        // --- 3. Comb Filter (Feedback Comb) ---
        const float fc = juce::jlimit(20.0f, 10000.0f, p.cutoff);
        const float delaySamples = juce::jlimit(2.0f, (float)(kCombBufSize - 2), (float)sr / fc);
        const float fb = juce::jmap(juce::jlimit(0.1f, 10.0f, p.res), 0.1f, 10.0f, 0.0f, 0.95f);

        int wIdx = combWriteIdx[ch];
        float readPos = (float)wIdx - delaySamples;
        if (readPos < 0.0f) readPos += (float)kCombBufSize;

        int rIdx1 = juce::jlimit(0, kCombBufSize - 1, (int)readPos);
        int rIdx2 = (rIdx1 + 1) % kCombBufSize;
        float frac = readPos - (float)rIdx1;

        float delayedSample = snapToZero(combBuffer[ch][rIdx1] * (1.0f - frac) + combBuffer[ch][rIdx2] * frac);

        float writeSample = snapToZero(x + delayedSample * fb);
        combBuffer[ch][wIdx] = writeSample;
        combWriteIdx[ch] = (wIdx + 1) % kCombBufSize;

        const float wet = p.combMix;
        return x * (1.0f - wet) + delayedSample * wet;
    }

    return x;
}

float PicoFilter::getMagnitudeForFrequency(float freqHz, const Params& p) const noexcept
{
    if (!p.enable) return 1.0f;

    const float fc = juce::jlimit(20.0f, 20000.0f, p.cutoff);
    const float res = juce::jlimit(0.1f, 10.0f, p.res);

    if (p.model == CleanSVF)
    {
        const int stages = (p.slope == 0) ? 1 : 2;
        const float w_norm = freqHz / fc;
        const float w2 = w_norm * w_norm;
        const float d = 1.0f / res;

        const float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d, 2.0f));
        float m = 1.0f / std::max(0.001f, den);

        if (p.type == LowPass) m *= 1.0f;
        else if (p.type == BandPass) m *= w_norm;
        else if (p.type == HighPass) m *= w2;
        else if (p.type == Notch) m *= std::abs(1.0f - w2);

        return std::pow(m, (float)stages);
    }
    else if (p.model == VowelFormant)
    {
        static const float vowelFreqs[5][3] = {
            { 800.0f,  1150.0f, 2900.0f },
            { 350.0f,  2000.0f, 2800.0f },
            { 270.0f,  2300.0f, 3000.0f },
            { 450.0f,   800.0f, 2830.0f },
            { 325.0f,   700.0f, 2700.0f }
        };

        const float pos = juce::jlimit(0.0f, 4.0f, p.formant * 4.0f);
        const int idx = (int)pos;
        const float frac = pos - (float)idx;
        const int nextIdx = std::min(4, idx + 1);

        float f1 = juce::jmap(frac, vowelFreqs[idx][0], vowelFreqs[nextIdx][0]);
        float f2 = juce::jmap(frac, vowelFreqs[idx][1], vowelFreqs[nextIdx][1]);
        float f3 = juce::jmap(frac, vowelFreqs[idx][2], vowelFreqs[nextIdx][2]);

        const float shift = p.cutoff / 1000.0f;
        f1 *= shift; f2 *= shift; f3 *= shift;

        auto getBandMag = [&](float centerF) -> float {
            float w_norm = freqHz / std::max(10.0f, centerF);
            float w2 = w_norm * w_norm;
            float d = 1.0f / (res * 2.0f);
            float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d, 2.0f));
            return (w_norm / std::max(0.001f, den));
        };

        return (getBandMag(f1) * 1.2f + getBandMag(f2) * 1.0f + getBandMag(f3) * 0.8f);
    }
    else if (p.model == CombFilter)
    {
        const float delaySamples = (float)sr / std::max(20.0f, fc);
        const float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f);
        const float wD = 2.0f * juce::MathConstants<float>::pi * freqHz * (delaySamples / (float)sr);

        const float combMag = 1.0f / std::sqrt(std::max(0.001f, 1.0f + fb * fb - 2.0f * fb * std::cos(wD)));
        const float wet = p.combMix;
        return (1.0f - wet) + combMag * wet;
    }

    return 1.0f;
}
