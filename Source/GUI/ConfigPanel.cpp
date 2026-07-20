// ==========================================
// File: ConfigPanel.cpp
// ConfigPanel 実装 (Global Settings パラメータの完全バインド & スタイリング)
// ==========================================
#include "ConfigPanel.h"

ConfigPanel::ConfigPanel(juce::AudioProcessorValueTreeState& apvts) : vts(apvts)
{
    choiceMaterial.combo.addItemList({ "Auto", "Crisp", "Smooth", "Formant" }, 1);
    choiceFilter.combo.addItemList({ "12dB/oct", "24dB/oct" }, 1);
    choiceTheme.combo.addItemList({ "Midnight", "Sakura", "Ocean", "Forest", "Sunset", "Mono" }, 1);
    choicePoly.combo.addItemList({ "1 Voice", "2 Voices", "4 Voices", "8 Voices", "16 Voices", "32 Voices" }, 1);
    choiceStretch.combo.addItemList({ "Beat", "Tone", "Texture", "Complex" }, 1);

    auto styleCombo = [](juce::ComboBox& c) {
        c.setColour(juce::ComboBox::backgroundColourId, PicoColors::panel);
        c.setColour(juce::ComboBox::outlineColourId, PicoColors::knobTrack);
        c.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        c.setColour(juce::ComboBox::arrowColourId, PicoColors::mint);
    };

    styleCombo(choiceMaterial.combo);
    styleCombo(choiceFilter.combo);
    styleCombo(choiceTheme.combo);
    styleCombo(choicePoly.combo);
    styleCombo(choiceStretch.combo);

    addAndMakeVisible(choiceMaterial);
    addAndMakeVisible(choiceFilter);
    addAndMakeVisible(choiceTheme);
    addAndMakeVisible(choicePoly);
    addAndMakeVisible(choiceStretch);
    
    btnPorta.setColour(juce::ToggleButton::textColourId, PicoColors::textDim);
    btnPorta.setColour(juce::ToggleButton::tickColourId, PicoColors::pink);
    addAndMakeVisible(btnPorta);

    addAndMakeVisible(knobLimRelease);
    addAndMakeVisible(knobSliceSens);
    addAndMakeVisible(knobPortaTime);

    materialAttach   = std::make_unique<ChoiceAttach>(vts, "analysisEngine", choiceMaterial.combo);
    filterAttach     = std::make_unique<ChoiceAttach>(vts, "filterSlope", choiceFilter.combo);
    themeAttach      = std::make_unique<ChoiceAttach>(vts, "colorTheme", choiceTheme.combo);
    polyAttach       = std::make_unique<ChoiceAttach>(vts, "polyphony", choicePoly.combo);
    stretchAttach    = std::make_unique<ChoiceAttach>(vts, "stretchMode", choiceStretch.combo);
    
    limReleaseAttach = std::make_unique<SliderAttach>(vts, "limRelease", knobLimRelease.knob);
    sliceSensAttach  = std::make_unique<SliderAttach>(vts, "sliceSensitivity", knobSliceSens.knob);
    portaAttach      = std::make_unique<ButtonAttach>(vts, "portaEnable", btnPorta);
    portaTimeAttach  = std::make_unique<SliderAttach>(vts, "portaTime", knobPortaTime.knob);

    knobLimRelease.knob.setDoubleClickReturnValue(true, 50.0);
    knobSliceSens.knob.setDoubleClickReturnValue(true, 0.5);
    knobPortaTime.knob.setDoubleClickReturnValue(true, 0.1);
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

    choiceStretch.setBounds(startX + 180, startY + 0 * 60, itemW, itemH);
    btnPorta.setBounds(startX + 180, startY + 1 * 60 + 16, itemW, 24);

    knobLimRelease.setBounds(startX + 360, startY + 10, 64, 82);
    knobSliceSens.setBounds(startX + 440, startY + 10, 64, 82);
    knobPortaTime.setBounds(startX + 520, startY + 10, 64, 82);
}
