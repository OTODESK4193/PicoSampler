// ==========================================
// File: ArpPanel.cpp
// ArpPanel 実装 (Swing ノブ追加 ＆ レイアウト最適化)
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

    auto styleCombo = [](juce::ComboBox& c) {
        c.setColour(juce::ComboBox::backgroundColourId, PicoColors::panel);
        c.setColour(juce::ComboBox::outlineColourId, PicoColors::knobTrack);
        c.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        c.setColour(juce::ComboBox::arrowColourId, PicoColors::mint);
    };

    styleCombo(comboPattern);
    styleCombo(comboRateSync);

    addAndMakeVisible(comboPattern);
    addAndMakeVisible(comboRateSync);

    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "arpPattern", comboPattern));
    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "arpRateSync", comboRateSync));

    static const juce::StringArray keys = { "Auto (Off)", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    comboKey.addItemList(keys, 1);
    comboScale.addItemList(ScaleQuantizer::getScaleNames(), 1);

    styleCombo(comboKey);
    styleCombo(comboScale);

    addAndMakeVisible(comboKey);
    addAndMakeVisible(comboScale);

    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "key", comboKey));
    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "scale", comboScale));

    knobOctaves.knob.setDoubleClickReturnValue(true, 1.0);
    knobRateFree.knob.setDoubleClickReturnValue(true, 4.0);
    knobGate.knob.setDoubleClickReturnValue(true, 0.8);
    knobOffset.knob.setDoubleClickReturnValue(true, 0.0);
    knobSwing.knob.setDoubleClickReturnValue(true, 0.0);

    addAndMakeVisible(knobOctaves);
    addAndMakeVisible(knobRateFree);
    addAndMakeVisible(knobGate);
    addAndMakeVisible(knobOffset);
    addAndMakeVisible(knobSwing);

    attachments.push_back(std::make_unique<Attachment>(vts, "arpOctaves", knobOctaves.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpRateFree", knobRateFree.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpGate", knobGate.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpOffset", knobOffset.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpSwing", knobSwing.knob));
}

void ArpPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    auto drawSectionHeader = [&g](const juce::String& name, int x, int y, int w, juce::Colour accent)
    {
        g.setColour(accent);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(name, x, y, w, 14, juce::Justification::left);
        g.setColour(accent.withAlpha(0.35f));
        g.fillRect(x, y + 16, w, 1);
    };

    drawSectionHeader("ARPEGGIATOR SETTINGS", 20, 10, 480, PicoColors::lavender);
    drawSectionHeader("SCALE QUANTIZER",     520, 10, 520, PicoColors::mint);
}

void ArpPanel::resized()
{
    btnEnable.setBounds(20,  34, 85, 26);
    btnLatch.setBounds(112, 34, 75, 26);
    btnSync.setBounds(194,  34, 65, 26);

    comboPattern.setBounds(266, 34, 110, 26);
    comboRateSync.setBounds(382, 34, 80, 26);

    const int knobY = 75;
    const int knobW = 60;
    const int knobH = 75;

    knobOctaves.setBounds(20,  knobY, knobW, knobH);
    knobRateFree.setBounds(95, knobY, knobW, knobH);
    knobGate.setBounds(170,    knobY, knobW, knobH);
    knobOffset.setBounds(245,  knobY, knobW, knobH);
    knobSwing.setBounds(320,   knobY, knobW, knobH);

    comboKey.setBounds(520, 34, 110, 26);
    comboScale.setBounds(638, 34, 180, 26);
}
