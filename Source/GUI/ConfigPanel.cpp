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
    addAndMakeVisible(knobFadeIn);
    addAndMakeVisible(knobFadeOut);

    materialAttach   = std::make_unique<ChoiceAttach>(vts, "analysisEngine", choiceMaterial.combo);
    filterAttach     = std::make_unique<ChoiceAttach>(vts, "filterSlope", choiceFilter.combo);
    themeAttach      = std::make_unique<ChoiceAttach>(vts, "colorTheme", choiceTheme.combo);
    polyAttach       = std::make_unique<ChoiceAttach>(vts, "polyphony", choicePoly.combo);
    stretchAttach    = std::make_unique<ChoiceAttach>(vts, "stretchMode", choiceStretch.combo);
    
    limReleaseAttach = std::make_unique<SliderAttach>(vts, "limRelease", knobLimRelease.knob);
    sliceSensAttach  = std::make_unique<SliderAttach>(vts, "sliceSensitivity", knobSliceSens.knob);
    portaAttach      = std::make_unique<ButtonAttach>(vts, "portaEnable", btnPorta);
    portaTimeAttach  = std::make_unique<SliderAttach>(vts, "portaTime", knobPortaTime.knob);
    fadeInAttach     = std::make_unique<SliderAttach>(vts, "edgeFadeIn", knobFadeIn.knob);
    fadeOutAttach    = std::make_unique<SliderAttach>(vts, "edgeFadeOut", knobFadeOut.knob);

    knobLimRelease.knob.setDoubleClickReturnValue(true, 50.0);
    knobSliceSens.knob.setDoubleClickReturnValue(true, 0.5);
    knobPortaTime.knob.setDoubleClickReturnValue(true, 0.1);
    knobFadeIn.knob.setDoubleClickReturnValue(true, 2.0);
    knobFadeOut.knob.setDoubleClickReturnValue(true, 3.0);

    // ms 表記。0 は「フェード無し = ハードカット」であることが一目で分かるようにする。
    auto msText = [](double v) -> juce::String
    {
        if (v < 0.005) return "Off";
        return juce::String(v, v < 10.0 ? 2 : 1) + " ms";
    };
    knobFadeIn.knob.textFromValueFunction  = msText;
    knobFadeOut.knob.textFromValueFunction = msText;
    knobFadeIn.knob.updateText();
    knobFadeOut.knob.updateText();
}

void ConfigPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    // MainPanel と同じ見出しスタイル (色付き下線) で領域を区切る
    auto drawSectionHeader = [&g](const juce::String& name, int x, int y, int w, juce::Colour accent)
    {
        g.setColour(accent);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(name, x, y, w, 14, juce::Justification::left);
        g.setColour(accent.withAlpha(0.35f));
        g.fillRect(x, y + 16, w, 1);
    };

    auto drawCaption = [&g](const juce::String& text, int x, int y, int w)
    {
        g.setColour(PicoColors::textDim.withAlpha(0.7f));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(text, x, y, w, 12, juce::Justification::left);
    };

    // 上段
    drawSectionHeader("ANALYSIS",   kColA, kRow1Head, kColW, PicoColors::mint);
    drawSectionHeader("SLICING",    kColB, kRow1Head, kColW, PicoColors::lavender);
    drawSectionHeader("APPEARANCE", kColC, kRow1Head, kColW, PicoColors::babyBlue);

    // 下段
    drawSectionHeader("SAMPLE EDGE", kColA, kRow2Head, kColW, PicoColors::peach);
    drawSectionHeader("PERFORMANCE", kColB, kRow2Head, kColW, PicoColors::pink);
    drawSectionHeader("OUTPUT",      kColC, kRow2Head, kColW, PicoColors::mint);

    drawCaption("Start / End marker de-click", kColA, kRow2Y + kKnobH + 4, kColW);
    drawCaption("Limiter recovery time",       kColC, kRow2Y + kKnobH + 4, kColW);

    // ------------------------------------------------------------------
    // サードパーティ表記 (ANALYSIS エリア直下)
    //
    // Signalsmith Stretch / Signalsmith Linear は MIT ライセンス。
    // MIT は「著作権表示と許諾表示を配布物に含めること」を要求するため、
    // ここでの画面表示に加えて、配布パッケージには LICENSE 全文を必ず同梱すること。
    //   Source/third_party/signalsmith-stretch/LICENSE-stretch.txt
    //   Source/third_party/signalsmith-stretch/signalsmith-linear/LICENSE-linear.txt
    // ------------------------------------------------------------------
    {
        const int x = kColA;
        int y = kRow1Y + kItemH + 14;

        g.setColour(juce::Colours::white);

        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("Time-stretching powered by Signalsmith Stretch",
                   x, y, kColW, 13, juce::Justification::left);
        y += 14;

        g.setFont(juce::FontOptions(10.0f));
        g.drawText(juce::CharPointer_UTF8("Copyright \xc2\xa9 2022 Geraint Luff / Signalsmith Audio Ltd."),
                   x, y, kColW, 12, juce::Justification::left);
        y += 12;

        g.drawText(juce::CharPointer_UTF8("Signalsmith Linear \xc2\xa9 2025 Signalsmith Audio"),
                   x, y, kColW, 12, juce::Justification::left);
        y += 12;

        g.drawText("Used under the MIT License. See LICENSE files included with this product.",
                   x, y, kColW + 60, 12, juce::Justification::left);
    }
}

void ConfigPanel::resized()
{
    // --- 上段 ---
    // ANALYSIS
    choiceMaterial.setBounds(kColA, kRow1Y, kItemW, kItemH);
    choiceStretch.setBounds (kColA + kItemW + 16, kRow1Y, kItemW, kItemH);

    // SLICING
    knobSliceSens.setBounds(kColB, kRow1Y, kKnobW, kKnobH);
    choiceFilter.setBounds (kColB + kKnobW + 24, kRow1Y, kItemW, kItemH);

    // APPEARANCE
    choiceTheme.setBounds(kColC, kRow1Y, kItemW, kItemH);

    // --- 下段 ---
    // SAMPLE EDGE
    knobFadeIn.setBounds (kColA, kRow2Y, kKnobW, kKnobH);
    knobFadeOut.setBounds(kColA + kKnobW + 16, kRow2Y, kKnobW, kKnobH);

    // PERFORMANCE (Polyphony はここが自然なのでこの区画に置く)
    choicePoly.setBounds   (kColB, kRow2Y, kItemW, kItemH);
    btnPorta.setBounds     (kColB, kRow2Y + kItemH + 8, kItemW, 24);
    knobPortaTime.setBounds(kColB + kItemW + 24, kRow2Y, kKnobW, kKnobH);

    // OUTPUT
    knobLimRelease.setBounds(kColC, kRow2Y, kKnobW, kKnobH);
}
