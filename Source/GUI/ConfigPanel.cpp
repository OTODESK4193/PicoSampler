// ==========================================
// File: ConfigPanel.cpp
// ConfigPanel 実装 (Limiter Release ノブ対応)
// ==========================================
#include "ConfigPanel.h"

ConfigPanel::ConfigPanel(juce::AudioProcessorValueTreeState& apvts) : vts(apvts)
{
    choiceMaterial.combo.addItemList({ "Auto", "Crisp", "Smooth", "Formant" }, 1);
    choiceFilter.combo.addItemList({ "12dB/oct", "24dB/oct" }, 1);
    choiceTheme.combo.addItemList({ "Midnight", "Sakura", "Ocean", "Forest", "Sunset", "Mono" }, 1);
    choicePoly.combo.addItemList({ "1 Voice", "2 Voices", "4 Voices", "8 Voices", "16 Voices", "32 Voices" }, 1);

    addAndMakeVisible(choiceMaterial);
    addAndMakeVisible(choiceFilter);
    addAndMakeVisible(choiceTheme);
    addAndMakeVisible(choicePoly);
    addAndMakeVisible(knobLimRelease);

    materialAttach   = std::make_unique<ChoiceAttach>(vts, "materialMode", choiceMaterial.combo);
    filterAttach     = std::make_unique<ChoiceAttach>(vts, "filterSlope", choiceFilter.combo);
    themeAttach      = std::make_unique<ChoiceAttach>(vts, "colorTheme", choiceTheme.combo);
    polyAttach       = std::make_unique<ChoiceAttach>(vts, "poly", choicePoly.combo);
    limReleaseAttach = std::make_unique<SliderAttach>(vts, "limRelease", knobLimRelease.knob);
}

void ConfigPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    g.setColour(PicoColors::mint);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("GLOBAL SETTINGS", 20, 15, 300, 20, juce::Justification::left);
    g.setColour(PicoColors::knobTrack);
    g.drawHorizontalLine(40, 20.0f, (float)getWidth() - 20.0f);
}

void ConfigPanel::resized()
{
    const int startX = 40;
    const int startY = 60;
    const int itemW  = 160;
    const int itemH  = 46;

    choiceMaterial.setBounds(startX, startY + 0 * 60, itemW, itemH);
    choiceFilter.setBounds(startX,   startY + 1 * 60, itemW, itemH);
    choiceTheme.setBounds(startX,    startY + 2 * 60, itemW, itemH);
    choicePoly.setBounds(startX,     startY + 3 * 60, itemW, itemH);

    knobLimRelease.setBounds(startX + 240, startY + 10, 64, 82);
}
