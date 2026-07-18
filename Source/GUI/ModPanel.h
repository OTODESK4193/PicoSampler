// ==========================================
// File: ModPanel.h
// モジュレーションマトリクス UI パネル (16スロット)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ArcDial.h"
#include "ValueKnob.h"
#include "GlowToggle.h"

class ModPanel : public juce::Component
{
public:
    ModPanel(juce::AudioProcessorValueTreeState& apvts);
    ~ModPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct SlotRow : public juce::Component
    {
        int slotIndex = 0;
        juce::ComboBox comboSrc;
        juce::ComboBox comboDst;
        juce::Slider sliderAmount;
        juce::ToggleButton btnUni { "UNI" };

        SlotRow()
        {
            sliderAmount.setSliderStyle(juce::Slider::LinearHorizontal);
            sliderAmount.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(comboSrc);
            addAndMakeVisible(comboDst);
            addAndMakeVisible(sliderAmount);
            addAndMakeVisible(btnUni);
        }

        void resized() override
        {
            comboSrc.setBounds(0, 2, 100, 24);
            comboDst.setBounds(105, 2, 120, 24);
            sliderAmount.setBounds(230, 2, 140, 24);
            btnUni.setBounds(375, 2, 50, 24);
        }
    };

    juce::AudioProcessorValueTreeState& vts;
    std::array<SlotRow, 8> slotRowsPage1;
    std::array<SlotRow, 8> slotRowsPage2;

    juce::TextButton btnPage1 { "PAGE 1 (1-8)" };
    juce::TextButton btnPage2 { "PAGE 2 (9-16)" };
    int activePage = 0;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<Attachment>> attachments;
    std::vector<std::unique_ptr<ComboAttach>> comboAttachments;
    std::vector<std::unique_ptr<ButtonAttach>> buttonAttachments;
};
