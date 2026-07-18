// ==========================================
// File: MainPanel.h
// メイン操作パネル (Sample / Pitch / Env / Mode / Master)
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

    WaveformDisplay& getWaveformDisplay() { return waveformDisplay; }

private:
    juce::AudioProcessorValueTreeState& vts;
    WaveformDisplay waveformDisplay;

    // Mode Toggle Buttons
    juce::TextButton btnSingle { "SINGLE" };
    juce::TextButton btnLayer  { "LAYER" };
    juce::TextButton btnRandom { "RANDOM" };

    // Knobs
    ValueKnob knobAttack, knobDecay, knobSustain, knobRelease;
    ValueKnob knobOctave, knobSemitone, knobFineTune;
    ValueKnob knobSampleStart, knobSampleEnd, knobLoopStart, knobLoopLength, knobCrossfade;
    ValueKnob knobMasterHpf, knobMasterLpf, knobOutGain, knobCeiling;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<Attachment>> attachments;
};
