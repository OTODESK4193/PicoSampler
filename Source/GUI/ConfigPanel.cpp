// ==========================================
// File: ConfigPanel.cpp
// ConfigPanel 実装
// ==========================================
#include "ConfigPanel.h"

ConfigPanel::ConfigPanel(juce::AudioProcessorValueTreeState& apvts) : vts(apvts)
{
    comboMaterial.addItemList({ "Auto", "Crisp (Drums)", "Smooth (Loops)", "Formant (Bass/Vocals)" }, 1);
    comboFilterSlope.addItemList({ "12 dB / oct", "24 dB / oct" }, 1);
    comboTheme.addItemList({ "Midnight", "Sakura", "Ocean", "Forest", "Sunset", "Mono" }, 1);

    addAndMakeVisible(comboMaterial);
    addAndMakeVisible(comboFilterSlope);
    addAndMakeVisible(comboTheme);
    addAndMakeVisible(knobPolyphony);

    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "materialMode", comboMaterial));
    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "filterSlope", comboFilterSlope));
    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "colorTheme", comboTheme));
    attachments.push_back(std::make_unique<Attachment>(vts, "poly", knobPolyphony));
}

void ConfigPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("SYSTEM & CONFIGURATION", 30, 20, 300, 24, juce::Justification::left);

    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("Material Detection Mode:", 30, 60, 180, 24, juce::Justification::left);
    g.drawText("Filter Slope:", 30, 105, 180, 24, juce::Justification::left);
    g.drawText("Color Theme:", 30, 150, 180, 24, juce::Justification::left);
    g.drawText("Max Polyphony:", 30, 195, 180, 24, juce::Justification::left);
}

void ConfigPanel::resized()
{
    comboMaterial.setBounds(220, 58, 160, 26);
    comboFilterSlope.setBounds(220, 103, 160, 26);
    comboTheme.setBounds(220, 148, 160, 26);
    knobPolyphony.setBounds(220, 190, 50, 50);
}
