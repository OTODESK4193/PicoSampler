// ==========================================
// File: MainPanel.h
// メイン操作パネル (Slot切替 / Mode選択 / Sample / Pitch / Env / Master)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ArcDial.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "WaveformDisplay.h"

class MainPanel : public juce::Component
{
public:
    MainPanel(juce::AudioProcessorValueTreeState& apvts);
    ~MainPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateStates();

    WaveformDisplay& getWaveformDisplay() { return waveformDisplay; }

private:
    struct LabeledKnob : public juce::Component
    {
        ValueKnob knob;
        juce::Label label;

        LabeledKnob(const juce::String& name)
        {
            label.setText(name, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
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
    WaveformDisplay waveformDisplay;

    // Slot Selection Buttons (1-8)
    std::array<juce::TextButton, 8> btnSlots;

    // Mode Toggle Buttons
    juce::TextButton btnSingle { "SINGLE" };
    juce::TextButton btnLayer  { "LAYER" };
    juce::TextButton btnRandom { "RANDOM" };

    // Labeled Knobs
    LabeledKnob knobSampleStart { "Start" };
    LabeledKnob knobSampleEnd   { "End" };
    LabeledKnob knobLoopStart   { "L-Start" };
    LabeledKnob knobLoopLength  { "L-Len" };
    LabeledKnob knobCrossfade   { "X-Fade" };

    LabeledKnob knobOctave    { "Octave" };
    LabeledKnob knobSemitone  { "Semi" };
    LabeledKnob knobFineTune  { "Fine" };

    LabeledKnob knobAttack   { "Attack" };
    LabeledKnob knobDecay    { "Decay" };
    LabeledKnob knobSustain  { "Sustain" };
    LabeledKnob knobRelease  { "Release" };

    LabeledKnob knobMasterHpf { "HPF" };
    LabeledKnob knobMasterLpf { "LPF" };
    LabeledKnob knobOutGain   { "Gain" };
    LabeledKnob knobCeiling   { "Ceil" };

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<Attachment>> attachments;
};
