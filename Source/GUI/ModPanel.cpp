// ==========================================
// File: ModPanel.cpp
// ModPanel 実装
// ==========================================
#include "ModPanel.h"
#include "../DSP/ModMatrix.h"

ModPanel::ModPanel(juce::AudioProcessorValueTreeState& apvts) : vts(apvts)
{
    addAndMakeVisible(btnPage1);
    addAndMakeVisible(btnPage2);

    btnPage1.onClick = [this] { activePage = 0; resized(); repaint(); };
    btnPage2.onClick = [this] { activePage = 1; resized(); repaint(); };

    auto srcNames = ModMatrix::getSourceNames();
    auto dstNames = ModMatrix::getDestNames();

    auto setupRow = [&](SlotRow& row, int idx) {
        row.slotIndex = idx;
        row.comboSrc.addItemList(srcNames, 1);
        row.comboDst.addItemList(dstNames, 1);
        addAndMakeVisible(row);

        const juce::String pPrefix = "mod" + juce::String(idx + 1);
        comboAttachments.push_back(std::make_unique<ComboAttach>(vts, pPrefix + "Src", row.comboSrc));
        comboAttachments.push_back(std::make_unique<ComboAttach>(vts, pPrefix + "Dst", row.comboDst));
        attachments.push_back(std::make_unique<Attachment>(vts, pPrefix + "Amt", row.sliderAmount));
        buttonAttachments.push_back(std::make_unique<ButtonAttach>(vts, pPrefix + "Uni", row.btnUni));
    };

    for (int i = 0; i < 8; ++i) setupRow(slotRowsPage1[(size_t)i], i);
    for (int i = 0; i < 8; ++i) setupRow(slotRowsPage2[(size_t)i], i + 8);
}

void ModPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("MODULATION MATRIX", 30, 20, 200, 24, juce::Justification::left);
}

void ModPanel::resized()
{
    btnPage1.setBounds(30, 50, 100, 26);
    btnPage2.setBounds(135, 50, 100, 26);

    for (int i = 0; i < 8; ++i)
    {
        const int y = 90 + i * 32;
        slotRowsPage1[(size_t)i].setBounds(30, y, 440, 28);
        slotRowsPage2[(size_t)i].setBounds(30, y, 440, 28);

        slotRowsPage1[(size_t)i].setVisible(activePage == 0);
        slotRowsPage2[(size_t)i].setVisible(activePage == 1);
    }
}
