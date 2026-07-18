// ==========================================
// File: ArcDial.h
// カスタム ArcDial LookAndFeel (Granularより移植)
// MOD変調アーク帯 ＆ ライブ変調ドット描画対応
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"

class ArcDialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ArcDialLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
