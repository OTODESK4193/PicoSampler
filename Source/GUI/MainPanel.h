// ==========================================
// File: MainPanel.h
// メイン操作パネル (2段目MASTER移動 ＆ REVERSE, LoopEnd, 英語ダイアログ)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ArcDial.h"
#include "ValueKnob.h"
#include "GlowToggle.h"

class MainPanel : public juce::Component
{
public:
    MainPanel(juce::AudioProcessorValueTreeState& apvts);
    ~MainPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateStates();
    void bindSlotParameters(int slotIdx);

private:
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

    juce::AudioProcessorValueTreeState& vts;

    // Slot 1-8 Selection Buttons
    std::array<juce::TextButton, 8> btnSlots;

    // Mode Toggle Buttons
    juce::TextButton btnSingle { "SINGLE" };
    juce::TextButton btnLayer  { "LAYER" };
    juce::TextButton btnRandom { "RANDOM" };

    // Loop, Stretch & Reverse Toggles
    GlowToggle btnLoop    { "LOOP" };
    GlowToggle btnStretch { "STRETCH" };
    GlowToggle btnReverse { "REVERSE" };

    // ADSR Link Toggle
    GlowToggle btnLinkEnv { "LINK" };
    bool isEnvLinked = false;

    // Labeled Knobs
    LabeledKnob knobSampleStart { "Start",   PicoColors::peach };
    LabeledKnob knobSampleEnd   { "End",     PicoColors::peach };
    LabeledKnob knobLoopStart   { "L-Start", PicoColors::peach };
    LabeledKnob knobLoopEnd     { "L-End",   PicoColors::peach };
    LabeledKnob knobCrossfade   { "X-Fade",  PicoColors::peach };

    LabeledKnob knobOctave    { "Octave", PicoColors::lavender };
    LabeledKnob knobSemitone  { "Semi",   PicoColors::lavender };
    LabeledKnob knobFineTune  { "Fine",   PicoColors::lavender };

    LabeledKnob knobAttack   { "Attack",  PicoColors::pink };
    LabeledKnob knobDecay    { "Decay",   PicoColors::pink };
    LabeledKnob knobSustain  { "Sustain", PicoColors::pink };
    LabeledKnob knobRelease  { "Release", PicoColors::pink };

    LabeledKnob knobMasterHpf { "HPF",  PicoColors::mint };
    LabeledKnob knobMasterLpf { "LPF",  PicoColors::mint };
    LabeledKnob knobOutGain   { "Gain", PicoColors::mint };

    int currentBoundSlot = -1;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<Attachment>> attachments;
    std::unique_ptr<ButtonAttach> loopAttach;
    std::unique_ptr<ButtonAttach> stretchAttach;
    std::unique_ptr<ButtonAttach> reverseAttach;

    std::unique_ptr<Attachment> outGainAttach;
    std::unique_ptr<Attachment> hpfAttach;
    std::unique_ptr<Attachment> lpfAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};
