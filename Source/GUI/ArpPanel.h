// ==========================================
// File: ArpPanel.h
// アルペジエイター ＆ スケール量子化設定パネル (見切れなしラベル付きレイアウト)
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
    struct LabeledKnob : public juce::Component
    {
        ValueKnob knob;
        juce::Label label;

        LabeledKnob(const juce::String& name, juce::Colour accent = PicoColors::lavender)
        {
            knob.setColour(juce::Slider::rotarySliderFillColourId, accent);
            label.setText(name, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            label.setColour(juce::Label::textColourId, PicoColors::textDim);
            addAndMakeVisible(knob);
            addAndMakeVisible(label);
        }

        void resized() override
        {
            auto area = getLocalBounds();
            label.setBounds(area.removeFromBottom(16));
            knob.setBounds(area);
        }
    };

    juce::AudioProcessorValueTreeState& vts;

    GlowToggle btnEnable { "ARP ON" };
    GlowToggle btnLatch  { "LATCH" };
    GlowToggle btnSync   { "SYNC" };

    juce::ComboBox comboPattern;
    juce::ComboBox comboRateSync;
    juce::ComboBox comboKey;
    juce::ComboBox comboScale;

    LabeledKnob knobOctaves  { "Octaves", PicoColors::lavender };
    LabeledKnob knobRateFree { "Rate (Hz)", PicoColors::lavender };
    LabeledKnob knobGate     { "Gate",     PicoColors::lavender };
    LabeledKnob knobOffset   { "Offset",   PicoColors::lavender };

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<Attachment>> attachments;
    std::vector<std::unique_ptr<ComboAttach>> comboAttachments;
    std::vector<std::unique_ptr<ButtonAttach>> buttonAttachments;
};
