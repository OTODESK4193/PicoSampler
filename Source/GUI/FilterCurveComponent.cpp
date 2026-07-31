// ==========================================
// File: FilterCurveComponent.cpp
// リアルタイム フィルターレスポンスカーブ表示実装 (QuadMorphFilter完全移植)
// ==========================================
#include "FilterCurveComponent.h"

namespace
{
    // ==========================================================================
    // Wavetable プロジェクト (FilterCurve.h) と同じ縮尺に統一。
    //   20Hz .. 20000Hz を 3 デケードの単純な対数配置で描く。
    //   以前は低域(20Hz-1kHz)に横幅70%を割く2区間スケールだったが、
    //   ノブの回転量とカーブの動きの対応がねじれて見えるとの指摘があり、
    //   Wavetable側の素直な等比対数スケールに合わせて解消する。
    // ==========================================================================
    constexpr float kMinFreq = 20.0f;
    constexpr float kMaxFreq = 20000.0f;

    // 周波数 -> x (0..1 正規化)
    float freqToNormX(float freq) noexcept
    {
        freq = juce::jlimit(kMinFreq, kMaxFreq, freq);
        return std::log10(freq / kMinFreq) / 3.0f; // 20Hz..20kHz = 3 decades
    }

    // x (0..1 正規化) -> 周波数
    float normXToFreq(float t) noexcept
    {
        t = juce::jlimit(0.0f, 1.0f, t);
        return kMinFreq * std::pow(1000.0f, t);
    }
}

void FilterCurveComponent::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    // 背景
    g.fillAll(PicoColors::panel.withMultipliedAlpha(0.9f));

    // 枠線
    g.setColour(PicoColors::knobTrack);
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 4.0f, 1.0f);

    // Wavetable と同じ dB レンジ (24 .. -48dB, 計72dB幅)
    const float dbTop = 24.0f;
    const float dbBottom = -48.0f;

    // 1. dB 水平グリッド線
    const float y0dB = juce::jmap(0.0f, dbTop, dbBottom, 0.0f, h);
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawHorizontalLine((int)y0dB, 0.0f, w);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.setColour(PicoColors::textDim);
    g.drawText("0dB", 4, (int)y0dB - 10, 30, 10, juce::Justification::left);

    static const float dbGridLines[] = { 12.0f, -12.0f, -24.0f, -36.0f };
    for (float dbLine : dbGridLines)
    {
        float yDb = juce::jmap(dbLine, dbTop, dbBottom, 0.0f, h);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawHorizontalLine((int)yDb, 0.0f, w);
        g.setColour(PicoColors::textDim.withAlpha(0.6f));
        g.drawText(juce::String((int)dbLine) + "dB", 4, (int)yDb - 10, 35, 10, juce::Justification::left);
    }

    // 2. 周波数 垂直グリッド線 (Wavetableと同じ 100/500/1k/5k/10k)
    static const float freqs[] = { 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f };
    static const juce::String labels[] = { "100", "500", "1k", "5k", "10k" };

    for (int i = 0; i < 5; ++i)
    {
        float x = freqToNormX(freqs[i]) * w;
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawVerticalLine((int)x, 0.0f, h);
        g.setColour(PicoColors::textDim.withAlpha(0.7f));
        g.drawText(labels[i], (int)x + 2, (int)h - 13, 30, 10, juce::Justification::left);
    }

    if (!currentParams.enable)
    {
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.setColour(PicoColors::textDim.withAlpha(0.5f));
        g.drawText("FILTER BYPASSED", 0, 0, (int)w, (int)h, juce::Justification::centred);
        return;
    }

    // 3. レスポンスカーブ計算 & 描画
    juce::Path curvePath;
    const int numPoints = std::min((int)w, 512);

    for (int px = 0; px < numPoints; ++px)
    {
        const float normX = (float)px / (float)(numPoints - 1);
        const float freq = normXToFreq(normX);
        const float mag = PicoFilter::getMagnitudeForFrequency(freq, currentParams, sampleRate);

        const float db = juce::Decibels::gainToDecibels(mag, -120.0f);
        const float y = juce::jlimit(-10.0f, h + 10.0f, juce::jmap(db, dbTop, dbBottom, 0.0f, h));
        const float x = normX * w;

        if (px == 0) curvePath.startNewSubPath(x, y);
        else curvePath.lineTo(x, y);
    }

    // カーブ下部の半透明グラデーション塗りつぶし
    juce::Path fillPath = curvePath;
    fillPath.lineTo(w, h);
    fillPath.lineTo(0.0f, h);
    fillPath.closeSubPath();

    juce::ColourGradient grad(PicoColors::mint.withAlpha(0.35f), 0.0f, 0.0f,
                               PicoColors::mint.withAlpha(0.02f), 0.0f, h, false);
    g.setGradientFill(grad);
    g.fillPath(fillPath);

    // ネオンカーブ線描画
    g.setColour(PicoColors::mint);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
