// ==========================================
// File: GlowToggle.h
// LED発光点灯トグルボタン (Granularより移植)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"

class GlowToggle : public juce::ToggleButton
{
public:
    GlowToggle(const juce::String& text = "") : juce::ToggleButton(text) {}

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        const float w = (float)getWidth();
        const float h = (float)getHeight();
        const bool activeState = getToggleState();

        // 1. 背景
        g.setColour(activeState ? PicoColors::mint.withAlpha(0.2f) : PicoColors::panel);
        g.fillRoundedRectangle(1.0f, 1.0f, w - 2.0f, h - 2.0f, 4.0f);

        // 2. 枠線
        g.setColour(activeState ? PicoColors::mint : PicoColors::knobTrack);
        g.drawRoundedRectangle(1.0f, 1.0f, w - 2.0f, h - 2.0f, 4.0f, 1.5f);

        // 3. LEDインジケーター
        const float ledRadius = 4.0f;
        const float ledX = 8.0f;
        const float ledY = h * 0.5f;

        if (activeState)
        {
            g.setColour(PicoColors::mint.withAlpha(0.4f));
            g.fillEllipse(ledX - ledRadius - 2.0f, ledY - ledRadius - 2.0f, (ledRadius + 2.0f) * 2.0f, (ledRadius + 2.0f) * 2.0f);
        }
        g.setColour(activeState ? PicoColors::mint : juce::Colours::grey);
        g.fillEllipse(ledX - ledRadius, ledY - ledRadius, ledRadius * 2.0f, ledRadius * 2.0f);

        // 4. テキスト
        g.setColour(activeState ? juce::Colours::white : juce::Colours::grey);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(getButtonText(), (int)(ledX + ledRadius + 6.0f), 0, (int)(w - (ledX + ledRadius + 8.0f)), (int)h,
                   juce::Justification::centredLeft, true);
    }
};
