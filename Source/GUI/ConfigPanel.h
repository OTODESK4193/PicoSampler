// ==========================================
// File: ConfigPanel.h
// システム＆環境設定パネル (Granularより移植)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ArcDial.h"
#include "ValueKnob.h"
#include "GlowToggle.h"

class ConfigPanel : public juce::Component
{
public:
    ConfigPanel(juce::AudioProcessorValueTreeState& apvts);
    ~ConfigPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& vts;

    juce::ComboBox comboMaterial;
    juce::ComboBox comboFilterSlope;
    juce::ComboBox comboTheme;
    ValueKnob knobPolyphony;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<Attachment>> attachments;
    std::vector<std::unique_ptr<ComboAttach>> comboAttachments;
};
