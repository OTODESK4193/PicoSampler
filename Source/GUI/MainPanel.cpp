// ==========================================
// File: MainPanel.cpp
// MainPanel レイアウト＆APVTSアタッチメント
// ==========================================
#include "MainPanel.h"

MainPanel::MainPanel(juce::AudioProcessorValueTreeState& apvts) : vts(apvts)
{
    addAndMakeVisible(waveformDisplay);

    // Mode Buttons
    btnSingle.setClickingTogglesState(true);
    btnLayer.setClickingTogglesState(true);
    btnRandom.setClickingTogglesState(true);

    btnSingle.setRadioGroupId(101);
    btnLayer.setRadioGroupId(101);
    btnRandom.setRadioGroupId(101);

    btnSingle.onClick = [this] { if (auto* p = vts.getParameter("samplerMode")) p->setValueNotifyingHost(0.0f); };
    btnLayer.onClick  = [this] { if (auto* p = vts.getParameter("samplerMode")) p->setValueNotifyingHost(0.5f); };
    btnRandom.onClick = [this] { if (auto* p = vts.getParameter("samplerMode")) p->setValueNotifyingHost(1.0f); };

    addAndMakeVisible(btnSingle);
    addAndMakeVisible(btnLayer);
    addAndMakeVisible(btnRandom);

    // Knobs Add & Attachment
    auto setupKnob = [this](ValueKnob& knob, const juce::String& paramId) {
        addAndMakeVisible(knob);
        attachments.push_back(std::make_unique<Attachment>(vts, paramId, knob));
    };

    setupKnob(knobAttack, "attack_0");
    setupKnob(knobDecay, "decay_0");
    setupKnob(knobSustain, "sustain_0");
    setupKnob(knobRelease, "release_0");

    setupKnob(knobOctave, "octave_0");
    setupKnob(knobSemitone, "pitchSt_0");
    setupKnob(knobFineTune, "fineTune_0");

    setupKnob(knobSampleStart, "sampleStart_0");
    setupKnob(knobSampleEnd, "sampleEnd_0");
    setupKnob(knobLoopStart, "loopStart_0");
    setupKnob(knobLoopLength, "loopLength_0");
    setupKnob(knobCrossfade, "crossfade_0");

    setupKnob(knobMasterHpf, "masterHPF");
    setupKnob(knobMasterLpf, "masterLPF");
    setupKnob(knobOutGain, "outGain");
    setupKnob(knobCeiling, "ceiling");

    btnSingle.setToggleState(true, juce::dontSendNotification);
}

void MainPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("MODE", 20, 185, 100, 20, juce::Justification::left);
    g.drawText("SAMPLE / LOOP", 20, 245, 150, 20, juce::Justification::left);
    g.drawText("PITCH", 450, 245, 100, 20, juce::Justification::left);
    g.drawText("ENVELOPE", 650, 245, 100, 20, juce::Justification::left);
    g.drawText("MASTER", 880, 245, 100, 20, juce::Justification::left);
}

void MainPanel::resized()
{
    waveformDisplay.setBounds(20, 15, 1040, 160);

    btnSingle.setBounds(20, 205, 80, 30);
    btnLayer.setBounds(105, 205, 80, 30);
    btnRandom.setBounds(190, 205, 80, 30);

    // Sample Row
    knobSampleStart.setBounds(20, 270, 55, 55);
    knobSampleEnd.setBounds(85, 270, 55, 55);
    knobLoopStart.setBounds(150, 270, 55, 55);
    knobLoopLength.setBounds(215, 270, 55, 55);
    knobCrossfade.setBounds(280, 270, 55, 55);

    // Pitch Row
    knobOctave.setBounds(450, 270, 55, 55);
    knobSemitone.setBounds(515, 270, 55, 55);
    knobFineTune.setBounds(580, 270, 55, 55);

    // Env Row
    knobAttack.setBounds(650, 270, 55, 55);
    knobDecay.setBounds(715, 270, 55, 55);
    knobSustain.setBounds(780, 270, 55, 55);
    knobRelease.setBounds(845, 270, 55, 55);

    // Master
    knobMasterHpf.setBounds(880, 270, 55, 55);
    knobMasterLpf.setBounds(945, 270, 55, 55);
}
