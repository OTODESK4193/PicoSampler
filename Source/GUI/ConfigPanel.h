// ==========================================
// File: ConfigPanel.h
// Config 設定パネル (Limiter Release ノブ追加)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ArcDial.h"
#include "ValueKnob.h"

class ConfigPanel : public juce::Component
{
public:
    ConfigPanel(juce::AudioProcessorValueTreeState& apvts);
    ~ConfigPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void bindEdgeSlot(int slotIdx);

    struct LabeledChoice : public juce::Component
    {
        juce::ComboBox combo;
        juce::Label label;

        LabeledChoice(const juce::String& name)
        {
            label.setText(name, juce::dontSendNotification);
            label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            label.setColour(juce::Label::textColourId, PicoColors::textDim);
            addAndMakeVisible(combo);
            addAndMakeVisible(label);
        }

        void resized() override
        {
            label.setBounds(0, 0, getWidth(), 16);
            combo.setBounds(0, 18, getWidth(), 24);
        }
    };

    struct LabeledKnob : public juce::Component
    {
        ValueKnob knob;
        juce::Label label;

        LabeledKnob(const juce::String& name, juce::Colour accent = PicoColors::mint)
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
            knob.setBounds(0, 0, getWidth(), getWidth());
            label.setBounds(0, getWidth() + 2, getWidth(), 16);
        }
    };

    // ---- レイアウト定数 (パネル実寸 1080 x 344 に収める) ----
    static constexpr int kColA = 20;
    static constexpr int kColB = 390;
    static constexpr int kColC = 760;
    static constexpr int kColW = 340;

    static constexpr int kRow1Head = 12;
    static constexpr int kRow1Y    = 38;
    static constexpr int kRow2Head = 152;
    static constexpr int kRow2Y    = 178;
    // SAMPLE EDGE 列のみ、上に S1..S8 切替ボタンを乗せる分ノブを下げる
    static constexpr int kEdgeKnobY = kRow2Y + 22;

    static constexpr int kItemW = 160;
    static constexpr int kItemH = 46;
    static constexpr int kKnobW = 64;
    static constexpr int kKnobH = 82;

    juce::AudioProcessorValueTreeState& vts;

    LabeledChoice choiceMaterial { "Material Mode" };
    LabeledChoice choiceFilter   { "Filter Slope" };
    LabeledChoice choiceTheme    { "Color Theme" };
    LabeledChoice choicePoly     { "Polyphony" };
    LabeledChoice choiceStretch  { "Stretch Mode" };

    LabeledKnob knobLimRelease   { "Lim Release", PicoColors::mint };
    LabeledKnob knobSliceSens    { "Slice Sens", PicoColors::lavender };
    LabeledKnob knobFadeIn       { "Fade In", PicoColors::peach };
    LabeledKnob knobFadeOut      { "Fade Out", PicoColors::peach };

    // Sample Edge (Fade In/Out) はスロット毎の設定。S1..S8 で対象を切替える。
    std::array<juce::TextButton, 8> btnEdgeSlots;
    int currentEdgeSlot = 0;

    juce::ToggleButton btnPorta  { "Portamento" };
    LabeledKnob knobPortaTime    { "Glide Time", PicoColors::pink };

    using ChoiceAttach = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ChoiceAttach> materialAttach;
    std::unique_ptr<ChoiceAttach> filterAttach;
    std::unique_ptr<ChoiceAttach> themeAttach;
    std::unique_ptr<ChoiceAttach> polyAttach;
    std::unique_ptr<ChoiceAttach> stretchAttach;
    
    std::unique_ptr<SliderAttach> limReleaseAttach;
    std::unique_ptr<SliderAttach> sliceSensAttach;
    // Fade In/Out はスロット切替のたびに対象パラメータへ付け替える (MainPanelと同じ方式)
    std::unique_ptr<SliderAttach> fadeInAttach;
    std::unique_ptr<SliderAttach> fadeOutAttach;
    
    std::unique_ptr<ButtonAttach> portaAttach;
    std::unique_ptr<SliderAttach> portaTimeAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfigPanel)
};
