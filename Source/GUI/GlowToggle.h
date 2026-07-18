// ==========================================
// File: GlowToggle.h
// LED発光点灯トグルボタン (Granularより移植, 警告解消済み)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"

class GlowToggle : public juce::ToggleButton
{
public:
    GlowToggle(const juce::String& text = "", juce::Colour color = PicoColors::mint)
        : juce::ToggleButton(text), accentColor(color) {}

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        const float w = (float)getWidth();
        const float h = (float)getHeight();
        const bool activeState = getToggleState();

        // 1. 背景
        g.setColour(activeState ? accentColor.withAlpha(0.2f) : PicoColors::panel);
        g.fillRoundedRectangle(1.0f, 1.0f, w - 2.0f, h - 2.0f, 4.0f);

        // 2. 枠線
        g.setColour(activeState ? accentColor : PicoColors::knobTrack);
        g.drawRoundedRectangle(1.0f, 1.0f, w - 2.0f, h - 2.0f, 4.0f, 1.5f);

        // 3. LEDインジケーター
        const float ledRadius = 4.0f;
        const float ledX = 8.0f;
        const float ledY = h * 0.5f;

        if (activeState)
        {
            g.setColour(accentColor.withAlpha(0.4f));
            g.fillEllipse(ledX - ledRadius - 2.0f, ledY - ledRadius - 2.0f, (ledRadius + 2.0f) * 2.0f, (ledRadius + 2.0f) * 2.0f);
        }
        g.setColour(activeState ? accentColor : PicoColors::textDim);
        g.fillEllipse(ledX - ledRadius, ledY - ledRadius, ledRadius * 2.0f, ledRadius * 2.0f);

        // 4. テキスト
        g.setColour(activeState ? juce::Colours::white : PicoColors::textDim);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText(getButtonText(), 18, 0, (int)w - 20, (int)h, juce::Justification::centredLeft, true);
    }

private:
    juce::Colour accentColor = PicoColors::mint;
};
