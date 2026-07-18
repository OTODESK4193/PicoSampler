// ==========================================
// File: TptSvfFilter.h
// TPT (Topology-Preserving Transform) ステートバリアブルフィルター
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>

class TptSvfFilter
{
public:
    enum FilterType { LowPass, HighPass, BandPass };

    TptSvfFilter() = default;
    ~TptSvfFilter() = default;

    void prepare(double sr) noexcept
    {
        sampleRate = sr;
        s1_1 = s2_1 = 0.0f;
        s1_2 = s2_2 = 0.0f;
        g = gTarget = 0.05f;
    }

    void reset() noexcept
    {
        s1_1 = s2_1 = 0.0f;
        s1_2 = s2_2 = 0.0f;
    }

    void setCutoffAndType(float freqHz, FilterType type, bool is24dB = false) noexcept
    {
        filterType = type;
        use24dB = is24dB;
        const float nyq = (float)(sampleRate * 0.5);
        freqHz = juce::jlimit(10.0f, nyq * 0.99f, freqHz);
        gTarget = std::tan(juce::MathConstants<float>::pi * freqHz / (float)sampleRate);
        R2 = 1.41421356f;
    }

    inline float processSample(float x) noexcept
    {
        g += 0.004f * (gTarget - g);

        // Stage 1
        const float hp1 = (x - (R2 + g) * s1_1 - s2_1) / (1.0f + R2 * g + g * g);
        const float bp1 = g * hp1 + s1_1; s1_1 = g * hp1 + bp1;
        const float lp1 = g * bp1 + s2_1; s2_1 = g * bp1 + lp1;

        float out1 = (filterType == HighPass) ? hp1 : ((filterType == BandPass) ? bp1 : lp1);

        if (!use24dB) return out1;

        // Stage 2 (24dB/oct)
        const float hp2 = (out1 - (R2 + g) * s1_2 - s2_2) / (1.0f + R2 * g + g * g);
        const float bp2 = g * hp2 + s1_2; s1_2 = g * hp2 + bp2;
        const float lp2 = g * bp2 + s2_2; s2_2 = g * bp2 + lp2;

        return (filterType == HighPass) ? hp2 : ((filterType == BandPass) ? bp2 : lp2);
    }

private:
    double sampleRate = 44100.0;
    FilterType filterType = LowPass;
    bool use24dB = false;

    float g = 0.05f, gTarget = 0.05f, R2 = 1.41421356f;
    float s1_1 = 0.0f, s2_1 = 0.0f;
    float s1_2 = 0.0f, s2_2 = 0.0f;
};
