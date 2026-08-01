// ==========================================
// File: FxChain.h
// 5スロット直列FXチェーン (Granularより移植)
// ADAA Saturation / Chorus / Tape Delay / Freeze / Shimmer Reverb
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <random>
#include <vector>

namespace gfx
{
    inline float antiDenormal(float x) noexcept
    {
        return (std::abs(x) < 1.0e-20f) ? 0.0f : x;
    }

    inline float safeLoopSaturate(float x) noexcept
    {
        if (x > 1.5f) return 1.5f + std::tanh(x - 1.5f) * 0.1f;
        if (x < -1.5f) return -1.5f + std::tanh(x + 1.5f) * 0.1f;
        return x;
    }

    inline int findNearestPrime(int n)
    {
        auto isPrime = [](int num)
        {
            if (num <= 1) return false;
            if (num <= 3) return true;
            if (num % 2 == 0 || num % 3 == 0) return false;
            for (int i = 5; i * i <= num; i += 6)
                if (num % i == 0 || num % (i + 2) == 0) return false;
            return true;
        };
        if (n <= 2) return 2;
        int up = n;
        while (!isPrime(up)) up++;
        int down = n;
        while (down > 2 && !isPrime(down)) down--;
        return ((up - n) < (n - down)) ? up : down;
    }

    class ChaosLFO
    {
        float phase1 = 0.0f, phase2 = 0.0f, inc1 = 0.0f, inc2 = 0.0f;
    public:
        void setFrequency(float freq, float sampleRate)
        {
            const float safeFreq = (freq > 0.0f) ? freq : 0.0f;
            inc1 = safeFreq / sampleRate;
            inc2 = (safeFreq * 1.41421356f) / sampleRate;
        }
        void setPhase(float p) { phase1 = p; phase2 = p * 1.618f; }
        inline float process() noexcept
        {
            if (inc1 <= 0.0f) return 0.0f;
            phase1 += inc1; if (phase1 >= 1.0f) phase1 -= 1.0f;
            phase2 += inc2; if (phase2 >= 1.0f) phase2 -= 1.0f;
            return (std::sin(phase1 * juce::MathConstants<float>::twoPi)
                  + std::sin(phase2 * juce::MathConstants<float>::twoPi)) * 0.5f;
        }
    };

    class OctaveShifter
    {
        std::array<float, 4096> buffer {};
        int writeIdx = 0;
        float phase = 0.0f;
    public:
        float process(float in) noexcept
        {
            buffer[(size_t)writeIdx] = in;
            const float windowSize = 2048.0f;
            phase += 1.0f / windowSize;
            if (phase >= 1.0f) phase -= 1.0f;

            float phaseB = phase + 0.5f;
            if (phaseB >= 1.0f) phaseB -= 1.0f;

            const float delayA = (1.0f - phase) * windowSize;
            const float delayB = (1.0f - phaseB) * windowSize;

            auto getInterpolated = [&](float d) noexcept
            {
                float readPos = (float)writeIdx - d;
                if (readPos < 0.0f) readPos += 4096.0f;
                const int idx1 = (int)readPos;
                const float frac = readPos - (float)idx1;
                const int idx2 = (idx1 + 1) % 4096;
                return buffer[(size_t)idx1] * (1.0f - frac) + buffer[(size_t)idx2] * frac;
            };

            const float outA = getInterpolated(delayA);
            const float outB = getInterpolated(delayB);
            const float winA = 0.5f * (1.0f - std::cos(phase * juce::MathConstants<float>::twoPi));
            const float winB = 0.5f * (1.0f - std::cos(phaseB * juce::MathConstants<float>::twoPi));

            writeIdx = (writeIdx + 1) % 4096;
            return outA * winA + outB * winB;
        }
    };

    struct VelvetNoiseDiffuser
    {
        std::vector<float> buffer;
        int writePos = 0;
        int bufferMask = 0;
        struct Tap { int delay; float gain; };
        std::vector<Tap> taps;
        float amount = 0.0f;

        void prepare(float fs)
        {
            const int size = 16384;
            buffer.assign((size_t)size, 0.0f);
            bufferMask = size - 1;
            writePos = 0;

            taps.clear();
            const int numTaps = 48;
            const float durationMs = 50.0f;
            const float totalSamples = durationMs * fs * 0.001f;
            const float grid = totalSamples / (float)numTaps;

            std::mt19937 gen(12345);
            std::uniform_real_distribution<float> distOffset(0.0f, grid - 1.0f);
            std::uniform_int_distribution<> sign(0, 1);
            const float normGain = 1.0f / std::sqrt((float)numTaps);

            for (int i = 0; i < numTaps; ++i)
            {
                Tap t;
                t.delay = (int)((float)i * grid + distOffset(gen));
                t.delay = juce::jlimit(1, size - 1, t.delay);
                t.gain = (sign(gen) == 0 ? 1.0f : -1.0f) * normGain;
                taps.push_back(t);
            }
        }
        void setAmount(float a) { amount = a; }
        inline float process(float in) noexcept
        {
            if (buffer.empty()) return in;
            buffer[(size_t)writePos] = in;
            float out = 0.0f;
            if (amount >= 0.01f)
            {
                for (const auto& t : taps)
                {
                    const int r = (writePos - t.delay) & bufferMask;
                    out += buffer[(size_t)r] * t.gain;
                }
            }
            writePos = (writePos + 1) & bufferMask;
            if (amount < 0.01f) return in;
            return in * (1.0f - amount) + out * amount;
        }
    };

    static inline void matrixHadamard(float* x) noexcept
    {
        for (int i = 0; i < 16; i += 2) { const float a = x[i]; const float b = x[i + 1]; x[i] = a + b; x[i + 1] = a - b; }
        for (int i = 0; i < 16; i += 4)
        {
            const float a0 = x[i]; const float b0 = x[i + 2]; x[i] = a0 + b0; x[i + 2] = a0 - b0;
            const float a1 = x[i + 1]; const float b1 = x[i + 3]; x[i + 1] = a1 + b1; x[i + 3] = a1 - b1;
        }
        for (int i = 0; i < 16; i += 8)
            for (int j = 0; j < 4; ++j)
            { const float a = x[i + j]; const float b = x[i + j + 4]; x[i + j] = a + b; x[i + j + 4] = a - b; }
        for (int i = 0; i < 8; ++i) { const float a = x[i]; const float b = x[i + 8]; x[i] = a + b; x[i + 8] = a - b; }
        for (int i = 0; i < 16; ++i) x[i] *= 0.25f;
    }

    class ShimmerReverb
    {
        static constexpr int NUM_CHANNELS = 16;
        std::vector<std::vector<float>> delayBuffers;
        std::array<int, NUM_CHANNELS> writePos {};
        std::array<int, NUM_CHANNELS> delayLengths {};
        std::array<float, NUM_CHANNELS> dampStates {};
        std::array<float, NUM_CHANNELS> dcX1 {}, dcY1 {};
        float curAmount = 0.0f;
        std::array<ChaosLFO, NUM_CHANNELS> lfos;
        VelvetNoiseDiffuser velvetL, velvetR;
        OctaveShifter shifter;

    public:
        ShimmerReverb() { delayBuffers.resize(NUM_CHANNELS); }

        void prepareToPlay(double sr)
        {
            velvetL.prepare((float)sr);
            velvetR.prepare((float)sr);
            const int maxDelaySamples = (int)(sr * 2.0);

            static const float LFO_RATIOS[16] = {
                1.000f, 0.618f, 1.272f, 0.786f, 1.618f, 0.382f, 1.414f, 0.528f,
                1.175f, 0.854f, 1.324f, 0.472f, 1.089f, 0.927f, 1.236f, 0.691f };
            static const float baseDelaysMs[16] = {
                31.0f, 37.0f, 41.0f, 43.0f, 47.0f, 53.0f, 59.0f, 61.0f,
                67.0f, 71.0f, 73.0f, 79.0f, 83.0f, 89.0f, 97.0f, 101.0f };

            for (int i = 0; i < NUM_CHANNELS; ++i)
            {
                delayBuffers[(size_t)i].assign((size_t)maxDelaySamples, 0.0f);
                const float targetSamps = baseDelaysMs[i] * 0.001f * (float)sr;
                delayLengths[(size_t)i] = findNearestPrime((int)targetSamps);
                if (delayLengths[(size_t)i] > maxDelaySamples - 100) delayLengths[(size_t)i] = maxDelaySamples - 100;
                writePos[(size_t)i] = 0;
                dampStates[(size_t)i] = 0.0f;
                dcX1[(size_t)i] = dcY1[(size_t)i] = 0.0f;
                curAmount = 0.0f;
                lfos[(size_t)i].setFrequency(0.5f * LFO_RATIOS[i], (float)sr);
                lfos[(size_t)i].setPhase((float)i / (float)NUM_CHANNELS);
            }
        }

        void process(float& inOutL, float& inOutR, float amount,
                     float decay, float shimmer, float damp, float mod) noexcept
        {
            curAmount += 0.008f * (amount - curAmount);
            if (amount <= 0.0f && curAmount < 1.0e-4f) return;
            amount = curAmount;

            velvetL.setAmount(amount * 0.8f);
            velvetR.setAmount(amount * 0.8f);

            const float vL = velvetL.process(inOutL);
            const float vR = velvetR.process(inOutR);

            const float monoIn = (vL + vR) * 0.5f;
            const float shimmerSig = shifter.process(monoIn);

            const float shimmerMix = shimmer * 0.7f;
            const float injectL = vL + shimmerSig * shimmerMix;
            const float injectR = vR + shimmerSig * shimmerMix;

            float delayOutputs[16] = {};
            float feedbackInputs[16] = {};

            const float lfoDepth = mod * 15.0f;
            const float dampFactor = 0.05f + damp * 0.6f;
            const float feedback = std::min(0.98f, 0.5f + decay * 0.48f);

            for (int i = 0; i < NUM_CHANNELS; ++i)
            {
                const float lfoVal = lfos[(size_t)i].process() * lfoDepth;
                float readPos = (float)writePos[(size_t)i] - ((float)delayLengths[(size_t)i] + lfoVal);
                const int bufSize = (int)delayBuffers[(size_t)i].size();
                if (bufSize == 0) continue;

                while (readPos < 0.0f) readPos += (float)bufSize;
                while (readPos >= (float)bufSize) readPos -= (float)bufSize;

                const int idx1 = (int)readPos;
                const float frac = readPos - (float)idx1;
                int idx2 = idx1 + 1;
                if (idx2 >= bufSize) idx2 = 0;

                const float rawRead = delayBuffers[(size_t)i][(size_t)idx1] * (1.0f - frac)
                                    + delayBuffers[(size_t)i][(size_t)idx2] * frac;

                dampStates[(size_t)i] = rawRead * (1.0f - dampFactor) + dampStates[(size_t)i] * dampFactor;

                const float dcIn = dampStates[(size_t)i];
                const float hp = dcIn - dcX1[(size_t)i] + 0.9975f * dcY1[(size_t)i];
                dcX1[(size_t)i] = dcIn;
                dcY1[(size_t)i] = hp;

                delayOutputs[i] = hp;
                feedbackInputs[i] = hp;
            }

            matrixHadamard(feedbackInputs);

            float sumL = 0.0f, sumR = 0.0f;
            for (int i = 0; i < NUM_CHANNELS; ++i)
            {
                const int bufSize = (int)delayBuffers[(size_t)i].size();
                if (bufSize == 0) continue;

                const float inSig = (i % 2 == 0) ? injectL : injectR;
                float v_n = inSig * (1.0f - feedback) + feedbackInputs[i] * feedback;
                v_n = antiDenormal(safeLoopSaturate(v_n));

                delayBuffers[(size_t)i][(size_t)writePos[(size_t)i]] = v_n;
                if (++writePos[(size_t)i] >= bufSize) writePos[(size_t)i] = 0;

                if (i < 8) sumL += delayOutputs[i];
                else       sumR += delayOutputs[i];
            }

            sumL *= 0.25f;
            sumR *= 0.25f;

            auto softClipOutput = [](float x) noexcept
            {
                const float ax = std::abs(x);
                if (ax > 1.0f) return x > 0.0f ? 1.0f : -1.0f;
                return x * (1.5f - 0.5f * x * x);
            };

            inOutL = inOutL * (1.0f - amount) + softClipOutput(sumL) * amount * 1.2f;
            inOutR = inOutR * (1.0f - amount) + softClipOutput(sumR) * amount * 1.2f;
        }
    };

    class EnsembleChorus
    {
    public:
        void prepareToPlay(double sr)
        {
            sampleRate = sr;
            delayBuffer.fill(0.0f);
            writeIndex = 0;
            lfoPhase1 = lfoPhase2 = 0.0f;
        }

        void process(float& inOutL, float& inOutR, float amount,
                     float rate, float depth, float width) noexcept
        {
            const float lfoRate1 = rate;
            const float lfoRate2 = rate * 1.5f;

            lfoPhase1 += lfoRate1 / (float)sampleRate;
            if (lfoPhase1 >= 1.0f) lfoPhase1 -= 1.0f;
            lfoPhase2 += lfoRate2 / (float)sampleRate;
            if (lfoPhase2 >= 1.0f) lfoPhase2 -= 1.0f;

            const float mix = amount * 0.5f;
            float outL = 0.0f, outR = 0.0f;

            auto hermite = [](float frac, float y0, float y1, float y2, float y3) noexcept
            {
                const float c0 = y1;
                const float c1 = 0.5f * (y2 - y0);
                const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                return ((c3 * frac + c2) * frac + c1) * frac + c0;
            };

            const int halfSize = (int)(delayBuffer.size() / 2);

            float mods[4];
            mods[0] = std::sin(lfoPhase1 * juce::MathConstants<float>::twoPi);
            mods[1] = -mods[0];
            mods[2] = std::sin(lfoPhase2 * juce::MathConstants<float>::twoPi);
            mods[3] = -mods[2];

            for (int i = 0; i < 4; ++i)
            {
                const float delayMs = 12.0f + (mods[i] * 5.0f * depth) + ((float)i * 3.0f);
                const float delaySamps = delayMs * (float)(sampleRate / 1000.0);

                float readPos = (float)writeIndex - delaySamps;
                if (readPos < 0.0f) readPos += (float)halfSize;

                int idx1 = (int)readPos;
                if (idx1 >= halfSize) idx1 = 0;
                if (idx1 < 0) idx1 += halfSize;
                const float frac = readPos - (float)idx1;

                int idx0 = idx1 - 1; if (idx0 < 0) idx0 += halfSize;
                int idx2 = idx1 + 1; if (idx2 >= halfSize) idx2 -= halfSize;
                int idx3 = idx1 + 2; if (idx3 >= halfSize) idx3 -= halfSize;

                const float chL = hermite(frac, delayBuffer[(size_t)idx0 * 2], delayBuffer[(size_t)idx1 * 2],
                                          delayBuffer[(size_t)idx2 * 2], delayBuffer[(size_t)idx3 * 2]);
                const float chR = hermite(frac, delayBuffer[(size_t)idx0 * 2 + 1], delayBuffer[(size_t)idx1 * 2 + 1],
                                          delayBuffer[(size_t)idx2 * 2 + 1], delayBuffer[(size_t)idx3 * 2 + 1]);

                if (i == 0 || i == 2)
                {
                    outL += chL * 0.7f + chR * 0.3f;
                    outR += chR * 0.3f - chL * 0.3f;
                }
                else
                {
                    outR += chR * 0.7f + chL * 0.3f;
                    outL += chL * 0.3f - chR * 0.3f;
                }
            }

            delayBuffer[(size_t)writeIndex * 2] = inOutL;
            delayBuffer[(size_t)writeIndex * 2 + 1] = inOutR;

            if (++writeIndex >= halfSize) writeIndex = 0;

            float wetL = outL * 0.25f;
            float wetR = outR * 0.25f;
            const float mono = (wetL + wetR) * 0.5f;
            wetL = mono + (wetL - mono) * width;
            wetR = mono + (wetR - mono) * width;

            if (amount > 0.0f)
            {
                inOutL = inOutL * (1.0f - mix) + wetL * mix;
                inOutR = inOutR * (1.0f - mix) + wetR * mix;
            }
        }

    private:
        double sampleRate = 44100.0;
        std::array<float, 96000 * 2> delayBuffer {};
        int writeIndex = 0;
        float lfoPhase1 = 0.0f, lfoPhase2 = 0.0f;
    };

    class TapeDelay
    {
    public:
        void prepareToPlay(double sr)
        {
            sampleRate = sr;
            delayBuffer.fill(0.0f);
            writeIndex = 0;
            envelope = 0.0f;
            currentDelaySamps = 0.0f;
            curAmount = 0.0f;
            curFeedback = 0.0f;
            lpStateL = lpStateR = 0.0f;
            dcX1L = dcY1L = dcX1R = dcY1R = 0.0f;
        }

        void process(float& inOutL, float& inOutR, float amount, double bpm,
                     float timeBeats, float feedback, float duck, float damp) noexcept
        {
            const float amountSmoothCoef = 1.0f - std::exp(-1.0f / (0.015f * (float)sampleRate));
            const float fbSmoothCoef = 1.0f - std::exp(-1.0f / (0.015f * (float)sampleRate));
            curAmount   += amountSmoothCoef * (amount - curAmount);
            curFeedback += fbSmoothCoef * (juce::jlimit(0.0f, 0.95f, feedback) - curFeedback);

            const float inSum = std::abs(inOutL) + std::abs(inOutR);
            const float attack = 0.001f;
            const float release = 0.0002f;
            if (inSum > envelope) envelope += attack * (inSum - envelope);
            else envelope += release * (inSum - envelope);

            const float duckingGain = 1.0f - juce::jlimit(0.0f, 0.85f, envelope * 4.0f * duck);

            const double effectiveBpm = bpm > 0.0 ? bpm : 120.0;
            const double beatSec = 60.0 / effectiveBpm;
            const int halfSize = (int)(delayBuffer.size() / 2);

            float targetDelaySamps = (float)(sampleRate * beatSec * (double)timeBeats);
            targetDelaySamps = juce::jlimit(32.0f, (float)(halfSize - 2), targetDelaySamps);

            if (currentDelaySamps == 0.0f) currentDelaySamps = targetDelaySamps;
            const float delaySmoothCoef = 1.0f - std::exp(-1.0f / (0.02f * (float)sampleRate));
            currentDelaySamps += delaySmoothCoef * (targetDelaySamps - currentDelaySamps);

            float readPos = (float)writeIndex - currentDelaySamps;
            if (readPos < 0.0f) readPos += (float)halfSize;

            int idx1 = (int)readPos;
            if (idx1 >= halfSize) idx1 = 0;
            if (idx1 < 0) idx1 += halfSize;
            const float frac = readPos - (float)idx1;

            int idx2 = idx1 + 1;
            if (idx2 >= halfSize) idx2 -= halfSize;

            float dL = delayBuffer[(size_t)idx1 * 2] * (1.0f - frac) + delayBuffer[(size_t)idx2 * 2] * frac;
            float dR = delayBuffer[(size_t)idx1 * 2 + 1] * (1.0f - frac) + delayBuffer[(size_t)idx2 * 2 + 1] * frac;

            const float lpCoef = 1.0f - damp * 0.85f;
            lpStateL += lpCoef * (dL - lpStateL);
            lpStateR += lpCoef * (dR - lpStateR);
            dL = lpStateL;
            dR = lpStateR;

            const float hpL = dL - dcX1L + 0.9975f * dcY1L; dcX1L = dL; dcY1L = hpL; dL = hpL;
            const float hpR = dR - dcX1R + 0.9975f * dcY1R; dcX1R = dR; dcY1R = hpR; dR = hpR;

            const float fb = curFeedback;
            delayBuffer[(size_t)writeIndex * 2] =
                gfx::antiDenormal(gfx::safeLoopSaturate((inOutL * duckingGain) + (dL * fb)));
            delayBuffer[(size_t)writeIndex * 2 + 1] =
                gfx::antiDenormal(gfx::safeLoopSaturate((inOutR * duckingGain) + (dR * fb)));

            if (++writeIndex >= halfSize) writeIndex = 0;

            inOutL += dL * curAmount;
            inOutR += dR * curAmount;
        }

    private:
        double sampleRate = 44100.0;
        std::array<float, 192000 * 2> delayBuffer {};
        int writeIndex = 0;
        float envelope = 0.0f;
        float currentDelaySamps = 0.0f;
        float lpStateL = 0.0f, lpStateR = 0.0f;
        float curAmount = 0.0f;
        float curFeedback = 0.0f;
        float dcX1L = 0.0f, dcY1L = 0.0f, dcX1R = 0.0f, dcY1R = 0.0f;
    };

    class FreezeSmear
    {
    public:
        void prepareToPlay(double sr)
        {
            sampleRate = sr;
            buffer.fill(0.0f);
            writeIndex = 0;
        }

        void process(float& inOutL, float& inOutR, float amount,
                     float sizeMs, float feedback, float damp) noexcept
        {
            const int halfSize = (int)(buffer.size() / 2);
            const int delaySamps = juce::jlimit(64, halfSize - 2,
                                                (int)(sampleRate * (double)sizeMs * 0.001));
            int readIndex = writeIndex - delaySamps;
            if (readIndex < 0) readIndex += halfSize;

            float smL = buffer[(size_t)readIndex * 2];
            float smR = buffer[(size_t)readIndex * 2 + 1];

            const float lpCoef = 1.0f - damp * 0.85f;
            lpStateL += lpCoef * (smL - lpStateL);
            lpStateR += lpCoef * (smR - lpStateR);
            smL = lpStateL;
            smR = lpStateR;

            const float fb = juce::jlimit(0.0f, 0.99f, feedback);
            buffer[(size_t)writeIndex * 2] =
                gfx::antiDenormal(gfx::safeLoopSaturate(inOutL * (1.0f - fb) + smL * fb));
            buffer[(size_t)writeIndex * 2 + 1] =
                gfx::antiDenormal(gfx::safeLoopSaturate(inOutR * (1.0f - fb) + smR * fb));

            if (++writeIndex >= halfSize) writeIndex = 0;

            if (amount > 0.0f)
            {
                inOutL = inOutL * (1.0f - amount) + smL * amount;
                inOutR = inOutR * (1.0f - amount) + smR * amount;
            }
        }

    private:
        double sampleRate = 44100.0;
        std::array<float, 96000 * 2> buffer {};
        int writeIndex = 0;
        float lpStateL = 0.0f, lpStateR = 0.0f;
    };

    struct SaturationState
    {
        float tapeHysteresis = 0.0f;
        float lastX = 0.0f;
        float lastF = 0.0f;
        bool  active = false;
        void reset() noexcept { tapeHysteresis = 0.0f; lastX = 0.0f; lastF = 0.0f; active = false; }
    };

    inline float calcADAAFunc(float x, int type) noexcept
    {
        switch (type)
        {
        case 0:
            if (std::abs(x) > 10.0f) return std::abs(x) - 0.693147f;
            return std::log(std::cosh(x));
        case 1:
            if (x < -1.0f) return -x - 0.5f;
            if (x > 1.0f)  return  x - 0.5f;
            return 0.5f * x * x;
        case 6:
        {
            const float k = 2.2f;
            const float scale = 0.58f;
            const float term1 = x * std::atan(k * x);
            const float term2 = (0.5f / k) * std::log(1.0f + k * k * x * x);
            return scale * (term1 - term2);
        }
        case 7:
            return -1.0f / juce::MathConstants<float>::pi * std::cos(x * juce::MathConstants<float>::pi);
        case 10:
            return (0.5f * x * x) - (x * x * x * x * 0.08333333f);
        default: return 0.0f;
        }
    }

    inline float processSaturationSampleADAA(float x, int type, float drive, SaturationState& state) noexcept
    {
        if (drive <= 1.001f)
        {
            state.active = false;
            state.lastX = x;
            return x;
        }

        if (type == 3)
        {
            const float g = x * drive;
            const float y = 0.92f * std::tanh(g + 0.08f * state.tapeHysteresis);
            state.tapeHysteresis = y;
            return y;
        }

        if (type == 2 || type == 4 || type == 5 || type == 8 || type == 9 || type == 10)
        {
            const float g = x * drive;
            switch (type)
            {
            case 2: { const float b = 0.25f; return std::tanh(g + b) - std::tanh(b); }
            case 4: return g / (1.0f + 0.45f * std::abs(g));
            case 5: return (std::abs(g) < 1.0f) ? g - (g * g * g) / 3.0f
                                                : (g > 0 ? (2.0f / 3.0f) : -(2.0f / 3.0f));
            case 8: { const float step = 1.0f / (1.0f + (25.0f - drive)); return std::round(g / step) * step; }
            case 9: { const float s = std::tanh(g); return s + 0.3f * (std::tanh(3.0f * g) - s); }
            case 10: {
                const float c = juce::jlimit(-1.0f, 1.0f, g);
                return 1.5f * c - 0.5f * c * c * c;
            }
            default: return g;
            }
        }

        const float g = x * drive;
        auto direct = [type](float v) noexcept
        {
            switch (type)
            {
            case 0: return std::tanh(v);
            case 1: return juce::jlimit(-1.0f, 1.0f, v);
            case 6: return std::atan(v * 2.2f) * 0.58f;
            case 7: return std::sin(v * juce::MathConstants<float>::pi);
            case 10: return v - (v * v * v) / 3.1f;
            default: return v;
            }
        };

        if (!state.active)
        {
            state.active = true;
            state.lastX = g;
            state.lastF = calcADAAFunc(g, type);
            return direct(g);
        }

        const float Fx = calcADAAFunc(g, type);
        const float delta = g - state.lastX;
        float output = (std::abs(delta) < 1.0e-5f) ? direct(g) : (Fx - state.lastF) / delta;
        state.lastX = g;
        state.lastF = Fx;
        return output;
    }
} // namespace gfx

class FxChain
{
public:
    FxChain() = default;
    static constexpr int kNumSlots = 5;

    enum FxType { None = 0, Saturation, Chorus, Delay, Freeze, Reverb };

    static juce::StringArray getTypeNames()
    {
        return { "None", "Saturation (ADAA)", "Ensemble Chorus", "Tape Delay", "Freeze", "Shimmer Reverb" };
    }
    static juce::StringArray getSatAlgoNames()
    {
        return { "Soft Tanh", "Hard Clip", "Triode", "Tape", "Transformer",
                 "JFET", "BJT", "Wavefold", "Exciter", "Cubic" };
    }
    static int satAlgoToType(int combo) noexcept
    {
        static const int map[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 10 };
        return map[juce::jlimit(0, 9, combo)];
    }
    static juce::StringArray getDelayTimeNames()
    {
        return { "1/2", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16.", "1/16", "1/16T" };
    }
    static float delayTimeToBeats(int idx) noexcept
    {
        static const float beats[10] = { 2.0f, 1.5f, 1.0f, 2.0f / 3.0f, 0.75f,
                                         0.5f, 1.0f / 3.0f, 0.375f, 0.25f, 1.0f / 6.0f };
        return beats[juce::jlimit(0, 9, idx)];
    }

    struct Params
    {
        std::array<int, kNumSlots> type {};
        std::array<float, kNumSlots> amount {};
        double bpm = 120.0;

        int   satAlgo = 0;
        float satDrive = 2.0f;
        float satPreHz = 20.0f;
        float satTrimDb = 0.0f;

        float choRate = 0.8f;
        float choDepth = 0.5f;
        float choWidth = 1.0f;

        int   dlyTime = 4;
        float dlyFeedback = 0.45f;
        float dlyDuck = 0.5f;
        float dlyDamp = 0.3f;

        float frzSize = 100.0f;
        float frzFeedback = 0.9f;
        float frzDamp = 0.2f;

        float revDecay = 0.7f;
        float revShimmer = 0.5f;
        float revDamp = 0.3f;
        float revMod = 0.4f;
    };

    void prepareToPlay(double sampleRate)
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        satStateL.reset();
        satStateR.reset();
        chorus.prepareToPlay(sr);
        delay.prepareToPlay(sr);
        freeze.prepareToPlay(sr);
        reverb.prepareToPlay(sr);

        // 時定数 12ms。ノブ操作でも MOD でも段差が出ない程度に速く、
        // かつ可聴なジッパーが残らない値。
        smCoef = 1.0f - std::exp(-1.0f / (0.012f * (float)sr));
        smInitialised = false;

        satHpX1L = satHpY1L = 0.0f;
        satHpX1R = satHpY1R = 0.0f;
    }

    void process(juce::AudioBuffer<float>& buffer, const Params& p) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        float* channelL = buffer.getWritePointer(0);
        float* channelR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : channelL;

        if (!smInitialised)
        {
            snapSmoothed(p);
            smInitialised = true;
        }

        // ------------------------------------------------------------------
        // Saturation 前段ハイパス (Sat Pre-HPF)。
        // 係数計算に exp/除算が入るのでブロック単位で更新する。
        // 既定の 20Hz 付近では実質バイパス扱いにして無駄な処理を省く。
        // ------------------------------------------------------------------
        const float preFc = juce::jlimit(20.0f, 2000.0f, sm.satPreHz);
        const float rc    = 1.0f / (2.0f * juce::MathConstants<float>::pi * preFc);
        const float dt    = 1.0f / (float)sr;
        const float hpA   = rc / (rc + dt);
        const bool  preHpfActive = (preFc > 25.0f);

        for (int s = 0; s < numSamples; ++s)
        {
            advanceSmoothed(p);

            float l = channelL[s];
            float r = channelR[s];

            for (int slot = 0; slot < kNumSlots; ++slot)
            {
                const int type = p.type[(size_t)slot];
                const float amt = sm.amount[(size_t)slot];

                switch (type)
                {
                case Saturation:
                    // amount は Saturation では ON/OFF ゲートとして働く仕様。
                    // 平滑値ではなく生値で判定して、切り替えを鈍らせない。
                    if (p.amount[(size_t)slot] > 0.0f)
                    {
                        const int algoType = satAlgoToType(p.satAlgo);
                        const float trimG = juce::Decibels::decibelsToGain(sm.satTrimDb);

                        float satInL = l;
                        float satInR = r;

                        if (preHpfActive)
                        {
                            satHpY1L = hpA * (satHpY1L + l - satHpX1L); satHpX1L = l; satInL = satHpY1L;
                            satHpY1R = hpA * (satHpY1R + r - satHpX1R); satHpX1R = r; satInR = satHpY1R;
                        }

                        l = gfx::processSaturationSampleADAA(satInL, algoType, sm.satDrive, satStateL) * trimG;
                        r = gfx::processSaturationSampleADAA(satInR, algoType, sm.satDrive, satStateR) * trimG;
                    }
                    break;
                case Chorus:
                    chorus.process(l, r, amt, sm.choRate, sm.choDepth, sm.choWidth);
                    break;
                case Delay:
                    delay.process(l, r, amt, p.bpm, delayTimeToBeats(p.dlyTime), sm.dlyFeedback, sm.dlyDuck, sm.dlyDamp);
                    break;
                case Freeze:
                    freeze.process(l, r, amt, sm.frzSize, sm.frzFeedback, sm.frzDamp);
                    break;
                case Reverb:
                    reverb.process(l, r, amt, sm.revDecay, sm.revShimmer, sm.revDamp, sm.revMod);
                    break;
                default:
                    break;
                }

                // スロット間ソフトクリッピング
                l = gfx::safeLoopSaturate(l);
                r = gfx::safeLoopSaturate(r);
            }

            channelL[s] = l;
            channelR[s] = r;
        }
    }

private:
    // ------------------------------------------------------------------
    // パラメータスムージング。
    // process() はサンプル単位ループだが、旧実装は Params の値を毎サンプル
    // そのまま使っていたため、ノブや MOD の変化がブロック境界の段差として
    // 現れていた (特に FX Amount / Chorus Depth / Sat Drive)。
    // Delay / Freeze / Reverb は内部にも平滑化を持つが、二重に掛かっても
    // 追従がわずかに緩むだけで害はない。
    //
    // type / satAlgo / dlyTime は離散値なので平滑化しない。
    // ------------------------------------------------------------------
    struct SmoothedParams
    {
        std::array<float, kNumSlots> amount {};

        float satDrive = 2.0f;
        float satPreHz = 20.0f;
        float satTrimDb = 0.0f;

        float choRate = 0.8f;
        float choDepth = 0.5f;
        float choWidth = 1.0f;

        float dlyFeedback = 0.45f;
        float dlyDuck = 0.5f;
        float dlyDamp = 0.3f;

        float frzSize = 100.0f;
        float frzFeedback = 0.9f;
        float frzDamp = 0.2f;

        float revDecay = 0.7f;
        float revShimmer = 0.5f;
        float revDamp = 0.3f;
        float revMod = 0.4f;
    };

    void snapSmoothed(const Params& p) noexcept
    {
        for (int i = 0; i < kNumSlots; ++i) sm.amount[(size_t)i] = p.amount[(size_t)i];
        sm.satDrive = p.satDrive;   sm.satPreHz = p.satPreHz;   sm.satTrimDb = p.satTrimDb;
        sm.choRate = p.choRate;     sm.choDepth = p.choDepth;   sm.choWidth = p.choWidth;
        sm.dlyFeedback = p.dlyFeedback; sm.dlyDuck = p.dlyDuck; sm.dlyDamp = p.dlyDamp;
        sm.frzSize = p.frzSize;     sm.frzFeedback = p.frzFeedback; sm.frzDamp = p.frzDamp;
        sm.revDecay = p.revDecay;   sm.revShimmer = p.revShimmer;
        sm.revDamp = p.revDamp;     sm.revMod = p.revMod;
    }

    inline void advanceSmoothed(const Params& p) noexcept
    {
        const float c = smCoef;
        for (int i = 0; i < kNumSlots; ++i)
            sm.amount[(size_t)i] += c * (p.amount[(size_t)i] - sm.amount[(size_t)i]);

        sm.satDrive    += c * (p.satDrive    - sm.satDrive);
        sm.satPreHz    += c * (p.satPreHz    - sm.satPreHz);
        sm.satTrimDb   += c * (p.satTrimDb   - sm.satTrimDb);
        sm.choRate     += c * (p.choRate     - sm.choRate);
        sm.choDepth    += c * (p.choDepth    - sm.choDepth);
        sm.choWidth    += c * (p.choWidth    - sm.choWidth);
        sm.dlyFeedback += c * (p.dlyFeedback - sm.dlyFeedback);
        sm.dlyDuck     += c * (p.dlyDuck     - sm.dlyDuck);
        sm.dlyDamp     += c * (p.dlyDamp     - sm.dlyDamp);
        sm.frzSize     += c * (p.frzSize     - sm.frzSize);
        sm.frzFeedback += c * (p.frzFeedback - sm.frzFeedback);
        sm.frzDamp     += c * (p.frzDamp     - sm.frzDamp);
        sm.revDecay    += c * (p.revDecay    - sm.revDecay);
        sm.revShimmer  += c * (p.revShimmer  - sm.revShimmer);
        sm.revDamp     += c * (p.revDamp     - sm.revDamp);
        sm.revMod      += c * (p.revMod      - sm.revMod);
    }

    double sr = 44100.0;
    gfx::SaturationState satStateL, satStateR;
    gfx::EnsembleChorus chorus;
    gfx::TapeDelay delay;
    gfx::FreezeSmear freeze;
    gfx::ShimmerReverb reverb;

    SmoothedParams sm;
    bool  smInitialised = false;
    float smCoef = 0.0015f;

    // Saturation 前段の1次ハイパス (Sat Pre-HPF) 状態
    float satHpX1L = 0.0f, satHpY1L = 0.0f;
    float satHpX1R = 0.0f, satHpY1R = 0.0f;
};
