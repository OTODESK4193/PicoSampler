// ==========================================
// File: ArpPanel.cpp
// ArpPanel 実装 (Repeat/Accentノブ, PicoFilter, リアルタイムレスポンスカーブ統合)
// ==========================================
#include "ArpPanel.h"
#include "../DSP/Arpeggiator.h"
#include "../DSP/ScaleQuantizer.h"

ArpPanel::ArpPanel(juce::AudioProcessorValueTreeState& apvts) : vts(apvts)
{
    // --- 1. ARP アタッチメント ---
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
    knobRateFree.knob.setDoubleClickReturnValue(true, 8.0);
    knobGate.knob.setDoubleClickReturnValue(true, 0.8);
    knobOffset.knob.setDoubleClickReturnValue(true, 0.0);
    knobSwing.knob.setDoubleClickReturnValue(true, 0.0);
    knobRepeat.knob.setDoubleClickReturnValue(true, 1.0);
    knobAccent.knob.setDoubleClickReturnValue(true, 0.0);

    addAndMakeVisible(knobOctaves);
    addAndMakeVisible(knobRateFree);
    addAndMakeVisible(knobGate);
    addAndMakeVisible(knobOffset);
    addAndMakeVisible(knobSwing);
    addAndMakeVisible(knobRepeat);
    addAndMakeVisible(knobAccent);

    attachments.push_back(std::make_unique<Attachment>(vts, "arpOctaves", knobOctaves.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpRateFree", knobRateFree.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpGate", knobGate.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpOffset", knobOffset.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpSwing", knobSwing.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpRepeat", knobRepeat.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "arpAccent", knobAccent.knob));

    // --- 2. FILTER アタッチメント ---
    addAndMakeVisible(btnFilterEnable);
    buttonAttachments.push_back(std::make_unique<ButtonAttach>(vts, "fltEnable", btnFilterEnable));

    comboFilterModel.addItemList({ "Clean SVF", "Vowel Formant", "Comb Filter" }, 1);
    comboFilterType.addItemList({ "LowPass", "BandPass", "HighPass", "Notch" }, 1);
    comboFilterSlope.addItemList({ "12dB/oct", "24dB/oct" }, 1);

    styleCombo(comboFilterModel);
    styleCombo(comboFilterType);
    styleCombo(comboFilterSlope);

    addAndMakeVisible(comboFilterModel);
    addAndMakeVisible(comboFilterType);
    addAndMakeVisible(comboFilterSlope);

    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "fltModel", comboFilterModel));
    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "fltType", comboFilterType));
    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "fltSlope", comboFilterSlope));

    comboFilterModel.onChange = [this] { updateFilterUIState(); };

    knobFilterCutoff.knob.setDoubleClickReturnValue(true, 2000.0);
    knobFilterRes.knob.setDoubleClickReturnValue(true, 0.707);
    knobFilterFormant.knob.setDoubleClickReturnValue(true, 0.0);
    knobFilterCombMix.knob.setDoubleClickReturnValue(true, 0.5);

    addAndMakeVisible(knobFilterCutoff);
    addAndMakeVisible(knobFilterRes);
    addAndMakeVisible(knobFilterFormant);
    addAndMakeVisible(knobFilterCombMix);

    attachments.push_back(std::make_unique<Attachment>(vts, "fltCutoff", knobFilterCutoff.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "fltRes", knobFilterRes.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "fltFormant", knobFilterFormant.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "fltCombMix", knobFilterCombMix.knob));

    addAndMakeVisible(filterCurveComp);
    updateFilterUIState();
}

void ArpPanel::updateFilterCurveDisplay() noexcept
{
    PicoFilter::Params fp;
    auto fetchBool = [this](const juce::String& id) {
        if (auto* p = vts.getRawParameterValue(id)) return p->load() > 0.5f;
        return false;
    };
    auto fetchFloat = [this](const juce::String& id, float def) {
        if (auto* p = vts.getRawParameterValue(id)) return p->load();
        return def;
    };

    fp.enable  = fetchBool("fltEnable");
    fp.model   = (int)fetchFloat("fltModel", 0.0f);
    fp.cutoff  = fetchFloat("fltCutoff", 2000.0f);
    fp.res     = fetchFloat("fltRes", 0.707f);
    fp.type    = (int)fetchFloat("fltType", 0.0f);
    fp.slope   = (int)fetchFloat("fltSlope", 0.0f);
    fp.formant = fetchFloat("fltFormant", 0.0f);
    fp.combMix = fetchFloat("fltCombMix", 0.5f);

    filterCurveComp.updateFilterState(fp);
}

void ArpPanel::updateFilterUIState() noexcept
{
    updateFilterCurveDisplay();

    auto* pModel = vts.getRawParameterValue("fltModel");
    if (pModel == nullptr) return;
    const int modelIdx = (int)pModel->load();

    if (modelIdx == 0) // Clean SVF
    {
        comboFilterType.setEnabled(true);
        comboFilterSlope.setEnabled(true);
        knobFilterCutoff.setEnabled(true);
        knobFilterRes.setEnabled(true);
        knobFilterFormant.setEnabled(false);
        knobFilterCombMix.setEnabled(false);
    }
    else if (modelIdx == 1) // Vowel Formant
    {
        comboFilterType.setEnabled(false);
        comboFilterSlope.setEnabled(false);
        knobFilterCutoff.setEnabled(true);
        knobFilterRes.setEnabled(true);
        knobFilterFormant.setEnabled(true);
        knobFilterCombMix.setEnabled(false);
    }
    else if (modelIdx == 2) // Comb Filter
    {
        comboFilterType.setEnabled(false);
        comboFilterSlope.setEnabled(false);
        knobFilterCutoff.setEnabled(true);
        knobFilterRes.setEnabled(true);
        knobFilterFormant.setEnabled(false);
        knobFilterCombMix.setEnabled(true);
    }
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

    drawSectionHeader("ARPEGGIATOR SETTINGS", 20, 8, 480, PicoColors::lavender);
    drawSectionHeader("SCALE QUANTIZER",     520, 8, 300, PicoColors::mint);
    drawSectionHeader("MAIN FILTER (CleanSVF / Vowel / Comb)", 20, 148, 1040, PicoColors::mint);
}

void ArpPanel::resized()
{
    btnEnable.setBounds(20,  30, 85, 26);
    btnLatch.setBounds(112, 30, 75, 26);
    btnSync.setBounds(194,  30, 65, 26);

    comboPattern.setBounds(266, 30, 110, 26);
    comboRateSync.setBounds(382, 30, 80, 26);

    const int knobY = 66;
    const int knobW = 55;
    const int knobH = 75;

    knobOctaves.setBounds(20,  knobY, knobW, knobH);
    knobRateFree.setBounds(82, knobY, knobW, knobH);
    knobGate.setBounds(144,    knobY, knobW, knobH);
    knobOffset.setBounds(206,  knobY, knobW, knobH);
    knobSwing.setBounds(268,   knobY, knobW, knobH);
    knobRepeat.setBounds(330,  knobY, knobW, knobH);
    knobAccent.setBounds(392,  knobY, knobW, knobH);

    comboKey.setBounds(520, 30, 110, 26);
    comboScale.setBounds(638, 30, 180, 26);

    // --- FILTER セクション ---
    btnFilterEnable.setBounds(20, 172, 90, 26);
    comboFilterModel.setBounds(118, 172, 115, 26);
    comboFilterType.setBounds(241, 172, 110, 26);
    comboFilterSlope.setBounds(359, 172, 90, 26);

    const int fltKnobY = 206;
    knobFilterCutoff.setBounds(20,  fltKnobY, 58, knobH);
    knobFilterRes.setBounds(84,  fltKnobY, 58, knobH);
    knobFilterFormant.setBounds(148, fltKnobY, 58, knobH);
    knobFilterCombMix.setBounds(212, fltKnobY, 58, knobH);

    filterCurveComp.setBounds(460, 172, 600, 110);
}
