// ==========================================
// File: ArpPanel.h
// アルペジエイター ＆ スケール量子化設定パネル (Granularより移植)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ArcDial.h"
#include "ValueKnob.h"
#include "GlowToggle.h"

class ArpPanel : public juce::Component
{
public:
    ArpPanel(juce::AudioProcessorValueTreeState& apvts);
    ~ArpPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& vts;

    GlowToggle btnEnable { "ARP ENABLE" };
    GlowToggle btnLatch  { "LATCH" };
    GlowToggle btnSync   { "SYNC" };

    juce::ComboBox comboPattern;
    juce::ComboBox comboRateSync;
    juce::ComboBox comboKey;
    juce::ComboBox comboScale;

    ValueKnob knobOctaves, knobRateFree, knobGate;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<Attachment>> attachments;
    std::vector<std::unique_ptr<ComboAttach>> comboAttachments;
    std::vector<std::unique_ptr<ButtonAttach>> buttonAttachments;
};
