// ==========================================
// File: ArpPanel.h
// アルペジエイター ＆ スケール量子化 ＆ フィルター(CleanSVF/Vowel/Comb) パネル
// (Filter Curve 表示統合完全移植)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ArcDial.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "FilterCurveComponent.h"
#include "../DSP/ModMatrix.h"

class ArpPanel : public juce::Component
{
public:
    ArpPanel(juce::AudioProcessorValueTreeState& apvts);
    ~ArpPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateFilterCurveDisplay() noexcept;
    void updateFilterUIState() noexcept;

    // カーブ表示にモジュレーション量を反映させるため ModMatrix を参照する
    void setModMatrix(const ModMatrix* mod) noexcept { modMatrix = mod; }

    juce::Slider& getOctavesKnob() { return knobOctaves.knob; }
    juce::Slider& getRateKnob()    { return knobRateFree.knob; }
    juce::Slider& getGateKnob()    { return knobGate.knob; }
    juce::Slider& getOffsetKnob()  { return knobOffset.knob; }
    juce::Slider& getSwingKnob()   { return knobSwing.knob; }
    juce::Slider& getRepeatKnob()  { return knobRepeat.knob; }
    juce::Slider& getAccentKnob()  { return knobAccent.knob; }

    juce::Slider& getCutoffKnob()  { return knobFilterCutoff.knob; }
    juce::Slider& getResoKnob()    { return knobFilterRes.knob; }
    juce::Slider& getFormantKnob() { return knobFilterFormant.knob; }
    juce::Slider& getCombMixKnob() { return knobFilterCombMix.knob; }

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

    // --- ARP ---
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
    LabeledKnob knobSwing    { "Swing",    PicoColors::mint };
    LabeledKnob knobRepeat   { "Repeat",   PicoColors::mint };
    LabeledKnob knobAccent   { "Accent",   PicoColors::pink };

    // --- FILTER ---
    GlowToggle btnFilterEnable { "FILTER ON" };

    juce::ComboBox comboFilterModel;
    juce::ComboBox comboFilterType;
    juce::ComboBox comboFilterSlope;

    LabeledKnob knobFilterCutoff  { "Cutoff",  PicoColors::mint };
    LabeledKnob knobFilterRes     { "Reso",    PicoColors::mint };
    LabeledKnob knobFilterFormant { "Formant", PicoColors::peach };
    LabeledKnob knobFilterCombMix { "Comb Mix",PicoColors::peach };

    // --- FILTER ENVELOPE ---
    LabeledKnob knobFltEnvA   { "Attack",  PicoColors::babyBlue };
    LabeledKnob knobFltEnvD   { "Decay",   PicoColors::babyBlue };
    LabeledKnob knobFltEnvS   { "Sustain", PicoColors::babyBlue };
    LabeledKnob knobFltEnvR   { "Release", PicoColors::babyBlue };
    LabeledKnob knobFltEnvAmt { "Amt",     PicoColors::pink };

    FilterCurveComponent filterCurveComp;
    const ModMatrix* modMatrix = nullptr;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<Attachment>> attachments;
    std::vector<std::unique_ptr<ComboAttach>> comboAttachments;
    std::vector<std::unique_ptr<ButtonAttach>> buttonAttachments;
};
