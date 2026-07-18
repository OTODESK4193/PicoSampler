// ==========================================
// File: ArpPanel.cpp
// ArpPanel 実装
// ==========================================
#include "ArpPanel.h"
#include "../DSP/Arpeggiator.h"
#include "../DSP/ScaleQuantizer.h"

ArpPanel::ArpPanel(juce::AudioProcessorValueTreeState& apvts) : vts(apvts)
{
    addAndMakeVisible(btnEnable);
    addAndMakeVisible(btnLatch);
    addAndMakeVisible(btnSync);

    buttonAttachments.push_back(std::make_unique<ButtonAttach>(vts, "arpEnable", btnEnable));
    buttonAttachments.push_back(std::make_unique<ButtonAttach>(vts, "arpLatch", btnLatch));
    buttonAttachments.push_back(std::make_unique<ButtonAttach>(vts, "arpSync", btnSync));

    comboPattern.addItemList(Arpeggiator::getPatternNames(), 1);
    comboRateSync.addItemList(Arpeggiator::getSyncRateNames(), 1);
    addAndMakeVisible(comboPattern);
    addAndMakeVisible(comboRateSync);

    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "arpPattern", comboPattern));
    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "arpRateSync", comboRateSync));

    static const juce::StringArray keys = { "Auto (Off)", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    comboKey.addItemList(keys, 1);
    comboScale.addItemList(ScaleQuantizer::getScaleNames(), 1);
    addAndMakeVisible(comboKey);
    addAndMakeVisible(comboScale);

    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "key", comboKey));
    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "scale", comboScale));

    addAndMakeVisible(knobOctaves);
    addAndMakeVisible(knobRateFree);
    addAndMakeVisible(knobGate);

    attachments.push_back(std::make_unique<Attachment>(vts, "arpOctaves", knobOctaves));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpRateFree", knobRateFree));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpGate", knobGate));
}

void ArpPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("ARPEGGIATOR", 30, 20, 200, 24, juce::Justification::left);
    g.drawText("SCALE QUANTIZE", 450, 20, 200, 24, juce::Justification::left);
}

void ArpPanel::resized()
{
    btnEnable.setBounds(30, 60, 110, 30);
    btnLatch.setBounds(150, 60, 90, 30);
    btnSync.setBounds(250, 60, 80, 30);

    comboPattern.setBounds(30, 110, 150, 28);
    comboRateSync.setBounds(190, 110, 140, 28);

    knobOctaves.setBounds(30, 160, 55, 55);
    knobRateFree.setBounds(95, 160, 55, 55);
    knobGate.setBounds(160, 160, 55, 55);

    comboKey.setBounds(450, 60, 120, 28);
    comboScale.setBounds(580, 60, 180, 28);
}
