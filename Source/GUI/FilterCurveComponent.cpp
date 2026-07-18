// ==========================================
// File: FilterCurveComponent.cpp
// リアルタイム フィルターレスポンスカーブ表示実装 (QuadMorphFilter完全移植)
// ==========================================
#include "FilterCurveComponent.h"

void FilterCurveComponent::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    // 背景
    g.fillAll(PicoColors::panel.withMultipliedAlpha(0.9f));

    // 枠線
    g.setColour(PicoColors::knobTrack);
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 4.0f, 1.0f);

    const float dbTop = 40.0f;
    const float dbBottom = -80.0f;

    // 1. dB 水平グリッド線
    const float y0dB = juce::jmap(0.0f, dbTop, dbBottom, 0.0f, h);
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawHorizontalLine((int)y0dB, 0.0f, w);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.setColour(PicoColors::textDim);
    g.drawText("0dB", 4, (int)y0dB - 10, 30, 10, juce::Justification::left);

    static const float dbGridLines[] = { 20.0f, -20.0f, -40.0f, -60.0f };
    for (float dbLine : dbGridLines)
    {
        float yDb = juce::jmap(dbLine, dbTop, dbBottom, 0.0f, h);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawHorizontalLine((int)yDb, 0.0f, w);
        g.setColour(PicoColors::textDim.withAlpha(0.6f));
        g.drawText(juce::String((int)dbLine) + "dB", 4, (int)yDb - 10, 35, 10, juce::Justification::left);
    }

    // 2. 周波数 垂直グリッド線 (100Hz, 500Hz, 1kHz, 5kHz, 10kHz)
    static const float freqs[] = { 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f };
    static const juce::String labels[] = { "100Hz", "500Hz", "1kHz", "5kHz", "10kHz" };

    for (int i = 0; i < 5; ++i)
    {
        float x = w * std::log10(freqs[i] / 20.0f) / 3.0f; // 20Hz -> 20000Hz (3 decade)
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawVerticalLine((int)x, 0.0f, h);
        g.setColour(PicoColors::textDim.withAlpha(0.7f));
        g.drawText(labels[i], (int)x + 2, (int)h - 13, 40, 10, juce::Justification::left);
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
        const float normX = (float)px / (float)numPoints;
        const float freq = 20.0f * std::pow(1000.0f, normX);
        const float mag = dummyFilter.getMagnitudeForFrequency(freq, currentParams);

        const float db = juce::Decibels::gainToDecibels(mag, -120.0f);
        const float y = juce::jlimit(-10.0f, h + 10.0f, juce::jmap(db, dbTop, dbBottom, 0.0f, h));

        if (px == 0) curvePath.startNewSubPath(0.0f, y);
        else curvePath.lineTo((float)px * (w / (float)numPoints), y);
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
