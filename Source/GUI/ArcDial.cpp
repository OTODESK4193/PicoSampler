#include "ArcDial.h"

ArcDialLookAndFeel::ArcDialLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, PicoColors::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    // ARPタブと同等のコンボボックス形状・カラー全統一
    setColour(juce::ComboBox::backgroundColourId, PicoColors::panel);
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
    setColour(juce::ComboBox::outlineColourId, PicoColors::knobTrack);
    setColour(juce::ComboBox::arrowColourId, PicoColors::mint);

    setColour(juce::PopupMenu::backgroundColourId, PicoColors::panel);
    setColour(juce::PopupMenu::textColourId, PicoColors::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, PicoColors::lavender.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, PicoColors::text);
}

void ArcDialLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                          juce::Slider& slider)
{
    const float radius = (float)std::min(width, height) * 0.5f - 4.0f;
    const float centreX = (float)x + (float)width * 0.5f;
    const float centreY = (float)y + (float)height * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // 1. 背景トラック
    juce::Path trackPath;
    trackPath.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(PicoColors::knobTrack);
    g.strokePath(trackPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2. MOD変調レンジ帯 (プロパティから取得)
    const auto& props = slider.getProperties();
    const bool modActive = props.getWithDefault("mod_active", false);

    if (modActive && props.contains("mod_min") && props.contains("mod_max"))
    {
        const float modMin = (float)props["mod_min"];
        const float modMax = (float)props["mod_max"];
        const float aMin = rotaryStartAngle + juce::jlimit(0.0f, 1.0f, modMin) * (rotaryEndAngle - rotaryStartAngle);
        const float aMax = rotaryStartAngle + juce::jlimit(0.0f, 1.0f, modMax) * (rotaryEndAngle - rotaryStartAngle);

        if (std::abs(aMax - aMin) > 0.001f)
        {
            juce::Path modArc;
            modArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, aMin, aMax, true);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.strokePath(modArc, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // 3. 値アーク
    juce::Path valuePath;
    valuePath.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(PicoColors::mint);
    g.strokePath(valuePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 4. ポインター
    const float pointerLen = radius * 0.6f;
    juce::Path p;
    p.addRoundedRectangle(-2.0f, -radius, 4.0f, pointerLen, 2.0f);
    g.setColour(juce::Colours::white);
    g.fillPath(p, juce::AffineTransform::rotation(angle).translated(centreX, centreY));

    // 5. ライブ変調ドット
    if (modActive && props.contains("mod_live"))
    {
        const float liveVal = (float)props["mod_live"];
        const float aAct = rotaryStartAngle + juce::jlimit(0.0f, 1.0f, liveVal) * (rotaryEndAngle - rotaryStartAngle);
        const float dotX = centreX + radius * std::sin(aAct);
        const float dotY = centreY - radius * std::cos(aAct);

        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.fillEllipse(dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(dotX - 2.5f, dotY - 2.5f, 5.0f, 5.0f);
    }
}

void ArcDialLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const float boundsWidth = (float)button.getWidth();
    const float boundsHeight = (float)button.getHeight();

    const bool isOn = button.getToggleState();
    g.setColour(isOn ? PicoColors::mint : PicoColors::knobTrack);
    g.fillRoundedRectangle(2.0f, 2.0f, boundsWidth - 4.0f, boundsHeight - 4.0f, 4.0f);

    g.setColour(isOn ? juce::Colours::black : juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, true);
}
