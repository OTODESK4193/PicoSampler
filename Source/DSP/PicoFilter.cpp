// ==========================================
// File: PicoFilter.cpp
// 単系統フィルター実装 (Wavetableプロジェクト DualFilterEngine より移植)
// ==========================================
#include "PicoFilter.h"

namespace
{
    // A, E, I, O, U の基本 3 フォルマント (Hz)
    static const float vowelFreqs[5][3] = {
        { 800.0f, 1150.0f, 2900.0f }, // A
        { 350.0f, 2000.0f, 2800.0f }, // E
        { 270.0f, 2300.0f, 3000.0f }, // I
        { 450.0f,  800.0f, 2830.0f }, // O
        { 325.0f,  700.0f, 2700.0f }  // U
    };
}

void PicoFilter::getVowelFreqs(float formant01, float cutoffHz, float sampleRate,
                               float& f1, float& f2, float& f3) noexcept
{
    const float pos  = juce::jlimit(0.0f, 4.0f, juce::jlimit(0.0f, 1.0f, formant01) * 4.0f);
    const int   idx  = juce::jlimit(0, 4, (int)pos);
    const float frac = pos - (float)idx;
    const int   nextIdx = std::min(4, idx + 1);

    f1 = juce::jmap(frac, vowelFreqs[idx][0], vowelFreqs[nextIdx][0]);
    f2 = juce::jmap(frac, vowelFreqs[idx][1], vowelFreqs[nextIdx][1]);
    f3 = juce::jmap(frac, vowelFreqs[idx][2], vowelFreqs[nextIdx][2]);

    // Cutoff ノブでフォルマント全体をシフト
    const float shift = juce::jlimit(0.02f, 20.0f, cutoffHz / 1000.0f);
    const float nyq = sampleRate * 0.45f;
    f1 = juce::jlimit(50.0f, nyq, f1 * shift);
    f2 = juce::jlimit(50.0f, nyq, f2 * shift);
    f3 = juce::jlimit(50.0f, nyq, f3 * shift);
}

void PicoFilter::prepare(double sampleRate, int) noexcept
{
    sr = sampleRate > 1000.0 ? sampleRate : 44100.0;

    // 時定数 12ms の一次ローパス。ノブを回した時に段差が聴こえず、
    // かつ LFO の変調にはきちんと追従する妥協点。
    smCoef = 1.0f - std::exp(-1.0f / (0.012f * (float)sr));

    ladderL.prepare(sr);
    ladderR.prepare(sr);
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
    std::memset(apState, 0, sizeof(apState));
    apPrev[0] = apPrev[1] = 0.0f;
    ladderL.reset();
    ladderR.reset();

    // 次のブロックで現在のパラメータ値へスナップさせる
    smInitialised = false;
}

void PicoFilter::process(juce::AudioBuffer<float>& buffer, const Params& p) noexcept
{
    if (!p.enable)
    {
        // OFF の間もスムーザは現在値へ張り付かせておく。
        // こうしないと OFF 中に動かしたノブの分だけ ON 直後に滑って聴こえる。
        smCutoff = p.cutoff;
        smRes = p.res;
        smFormant = p.formant;
        smCombMix = p.combMix;
        smInitialised = true;
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    float* channelL = buffer.getWritePointer(0);
    float* channelR = numCh > 1 ? buffer.getWritePointer(1) : channelL;

    if (!smInitialised)
    {
        smCutoff = p.cutoff;
        smRes = p.res;
        smFormant = p.formant;
        smCombMix = p.combMix;
        smInitialised = true;
    }

    // サンプル単位で平滑化した値を入れる作業用コピー。
    // type / slope24 / enable は離散値なので p のまま使う。
    Params sp = p;

    // LadderLPF はカットオフ / レゾナンスをブロック先頭で一度だけ設定する
    // (setParameters が重いため)。平滑化済みの値を渡すので、
    // ノブを急に回しても段差ではなく滑らかな軌跡をたどる。
    if (p.type == LadderLPF)
    {
        const float resN = res01(smRes);
        ladderL.setParameters(juce::jlimit(20.0f, 20000.0f, smCutoff), resN);
        ladderR.setParameters(juce::jlimit(20.0f, 20000.0f, smCutoff), resN);
    }

    for (int s = 0; s < numSamples; ++s)
    {
        smCutoff  += smCoef * (p.cutoff  - smCutoff);
        smRes     += smCoef * (p.res     - smRes);
        smFormant += smCoef * (p.formant - smFormant);
        smCombMix += smCoef * (p.combMix - smCombMix);

        sp.cutoff  = smCutoff;
        sp.res     = smRes;
        sp.formant = smFormant;
        sp.combMix = smCombMix;

        channelL[s] = processSample(0, channelL[s], sp);
        if (numCh > 1)
            channelR[s] = processSample(1, channelR[s], sp);
    }
}

float PicoFilter::processSample(int ch, float x, const Params& p) noexcept
{
    switch (p.type)
    {
        case LPF: case HPF: case BPF: case Notch:
        {
            const float normFc = juce::jlimit(0.0001f, 0.46f, p.cutoff / (float)sr);
            const float g = std::tan(juce::MathConstants<float>::pi * normFc);
            const float q = resToQ(p.res);
            const float k = 1.0f / q;
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const int   stages = p.slope24 ? 2 : 1;
            const int   outMode = (p.type == Notch) ? BPF : p.type; // Notch は中で BP を作って x - BP する

            float currInput = x;
            for (int st = 0; st < stages; ++st)
            {
                float& s1 = svf_s1[st][ch];
                float& s2 = svf_s2[st][ch];
                s1 = snapToZero(s1);
                s2 = snapToZero(s2);

                const float in = currInput;
                const float v3 = in - s2;
                const float v1 = a1 * s1 + a2 * v3;
                const float v2 = s2 + a2 * s1 + a3 * v3;

                s1 = snapToZero(2.0f * v1 - s1);
                s2 = snapToZero(2.0f * v2 - s2);

                if (!std::isfinite(s1) || !std::isfinite(s2) ||
                    std::abs(s1) > kStateLimit || std::abs(s2) > kStateLimit)
                {
                    s1 = 0.0f; s2 = 0.0f;
                }

                const float hp = in - k * v1 - v2;
                const float bp = v1;
                const float lp = v2;

                if (p.type == LPF)        currInput = lp;
                else if (p.type == BPF)   currInput = bp;
                else if (p.type == HPF)   currInput = hp;
                else /* Notch */          currInput = in - bp; // x - BandPass
            }
            juce::ignoreUnused(outMode);
            return currInput;
        }

        case LadderLPF:
            return (ch == 0) ? ladderL.processSample(x) : ladderR.processSample(x);

        case Comb:
        case CombPlus:
        {
            const float fc = juce::jlimit(20.0f, 10000.0f, p.cutoff);
            const float delaySamples = juce::jlimit(2.0f, (float)(kCombBufSize - 2), (float)sr / fc);
            const float fb = juce::jmap(resToQ(p.res), 0.1f, 10.0f, 0.0f, 0.95f);

            int wIdx = combWriteIdx[ch];
            float readPos = (float)wIdx - delaySamples;
            if (readPos < 0.0f) readPos += (float)kCombBufSize;

            int rIdx1 = juce::jlimit(0, kCombBufSize - 1, (int)readPos);
            int rIdx2 = (rIdx1 + 1) % kCombBufSize;
            float frac = readPos - (float)rIdx1;

            float delayed = snapToZero(combBuffer[ch][rIdx1] * (1.0f - frac) + combBuffer[ch][rIdx2] * frac);

            if (p.type == Comb)
            {
                // フィードバックコム (tanh 飽和あり)
                float out = std::tanh(x + delayed * fb);
                combBuffer[ch][wIdx] = out;
                combWriteIdx[ch] = (wIdx + 1) % kCombBufSize;
                return out;
            }
            else
            {
                // CombPlus: 飽和なし + Wet Mix
                combBuffer[ch][wIdx] = snapToZero(x + delayed * fb);
                combWriteIdx[ch] = (wIdx + 1) % kCombBufSize;
                const float wet = juce::jlimit(0.0f, 1.0f, p.combMix);
                return x * (1.0f - wet) + delayed * wet;
            }
        }

        case Vowel:
        {
            float f1 = 800.0f, f2 = 1150.0f, f3 = 2900.0f;
            getVowelFreqs(p.formant, p.cutoff, (float)sr, f1, f2, f3);
            const float vq = vowelQ(p.res);
            const float fR = 1.0f / (2.0f * vq);
            static const float bandGain[3] = { 1.2f, 1.0f, 0.8f };
            const float freqs[3] = { f1, f2, f3 };
            const float norm = 2.0f * fR;

            float out = 0.0f;
            for (int b = 0; b < 3; ++b)
            {
                float& s1 = form_s1[b][ch];
                float& s2 = form_s2[b][ch];
                s1 = snapToZero(s1);
                s2 = snapToZero(s2);

                const float normF = juce::jlimit(0.001f, 0.46f, freqs[b] / (float)sr);
                const float g = std::tan(juce::MathConstants<float>::pi * normF);
                const float h = 1.0f / (1.0f + 2.0f * fR * g + g * g);

                const float hp = (x - (2.0f * fR + g) * s1 - s2) * h;
                const float bp = g * hp + s1;  s1 = g * hp + bp;
                const float lp = g * bp + s2;  s2 = g * bp + lp;

                if (!std::isfinite(s1) || !std::isfinite(s2) ||
                    std::abs(s1) > kStateLimit || std::abs(s2) > kStateLimit)
                {
                    s1 = 0.0f; s2 = 0.0f;
                    continue;
                }
                s1 = snapToZero(s1);
                s2 = snapToZero(s2);

                out += bp * bandGain[b] * norm;
            }
            return out;
        }

        case Phaser:
        {
            const float fb = juce::jlimit(0.0f, 0.95f, juce::jmap(resToQ(p.res), 0.1f, 10.0f, 0.0f, 0.95f));
            const int stages = p.slope24 ? 8 : 4;
            const float normFc = juce::jlimit(0.0001f, 0.46f, p.cutoff / (float)sr);
            const float g = std::tan(juce::MathConstants<float>::pi * normFc);
            const float ladderG = g / (1.0f + g);

            float inAp = std::tanh(x + fb * apPrev[ch]);

            for (int s = 0; s < stages; ++s)
            {
                float& z = apState[s][ch];
                const float v = (inAp - z) * ladderG;
                const float lp = v + z;
                z = lp + v;
                inAp = 2.0f * lp - inAp;
            }

            if (!std::isfinite(inAp) || std::abs(inAp) > kStateLimit)
            {
                for (int s = 0; s < 8; ++s) apState[s][ch] = 0.0f;
                apPrev[ch] = 0.0f;
                return 0.0f;
            }

            apPrev[ch] = snapToZero(inAp);
            const float mix = juce::jlimit(0.0f, 1.0f, p.combMix);
            return x * (1.0f - mix) + inAp * mix;
        }

        default:
            return x;
    }
}

float PicoFilter::getMagnitudeForFrequency(float freqHz, const Params& p, double sampleRate) noexcept
{
    if (!p.enable) return 1.0f;

    const float fc = juce::jlimit(20.0f, 20000.0f, p.cutoff);
    const float q  = resToQ(p.res);

    switch (p.type)
    {
        case LPF: case HPF: case BPF: case Notch:
        {
            const float w  = freqHz / fc;
            const float w2 = w * w;
            const float d  = 1.0f / q;
            const float den = std::sqrt((1.0f - w2) * (1.0f - w2) + (w * d) * (w * d));
            const float base = 1.0f / std::max(1.0e-4f, den);

            float m = base;
            if      (p.type == BPF)   m = base * w;
            else if (p.type == HPF)   m = base * w2;
            else if (p.type == Notch) m = std::abs(1.0f - base * w);

            const bool canCascade = (p.type != Notch);
            if (p.slope24 && canCascade) m = m * m;
            return m;
        }

        case LadderLPF:
        {
            const float w  = freqHz / fc;
            const float w2 = w * w;
            const float d  = 1.0f / std::max(0.35f, q);
            const float den = std::sqrt((1.0f - w2) * (1.0f - w2) + (w * d) * (w * d));
            const float m = 1.0f / std::max(1.0e-4f, den);
            return m * m;
        }

        case Comb:
        case CombPlus:
        {
            const float delaySamples = (float)sampleRate / std::max(20.0f, fc);
            const float fb = juce::jmap(q, 0.1f, 10.0f, 0.0f, 0.95f);
            const float wD = 2.0f * juce::MathConstants<float>::pi * freqHz * (delaySamples / (float)sampleRate);
            const float combMag = 1.0f / std::sqrt(std::max(1.0e-4f, 1.0f + fb * fb - 2.0f * fb * std::cos(wD)));
            if (p.type == Comb) return combMag;

            const float wet = juce::jlimit(0.0f, 1.0f, p.combMix);
            return (1.0f - wet) + combMag * wet;
        }

        case Phaser:
        {
            const int   stages = p.slope24 ? 8 : 4;
            const float phi = -2.0f * (float)stages * std::atan(freqHz / fc);
            const float mix = juce::jlimit(0.0f, 1.0f, p.combMix);
            const float dry = 1.0f - mix;

            float m = std::sqrt(juce::jmax(0.0f, dry * dry + mix * mix + 2.0f * dry * mix * std::cos(phi)));

            const float fb = juce::jlimit(0.0f, 0.95f, juce::jmap(q, 0.1f, 10.0f, 0.0f, 0.95f));
            m /= juce::jmax(0.25f, 1.0f - fb * std::cos(phi) * 0.75f);
            return m;
        }

        case Vowel:
        {
            float f1 = 800.0f, f2 = 1150.0f, f3 = 2900.0f;
            getVowelFreqs(p.formant, p.cutoff, (float)sampleRate, f1, f2, f3);
            const float vq = vowelQ(p.res);

            auto bandMag = [&](float centreF)
            {
                const float w  = freqHz / std::max(10.0f, centreF);
                const float w2 = w * w;
                const float d  = 1.0f / vq;
                const float den = std::sqrt((1.0f - w2) * (1.0f - w2) + (w * d) * (w * d));
                return w / std::max(1.0e-4f, den);
            };
            return bandMag(f1) * 1.2f + bandMag(f2) * 1.0f + bandMag(f3) * 0.8f;
        }

        default: return 1.0f;
    }
}
