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

    comboFilterType.addItemList({ "LPF", "HPF", "BPF", "Notch", "Comb", "Ladder LPF", "Vowel", "Comb+", "Phaser" }, 1);
    comboFilterSlope.addItemList({ "12dB/oct", "24dB/oct" }, 1);

    styleCombo(comboFilterType);
    styleCombo(comboFilterSlope);

    addAndMakeVisible(comboFilterType);
    addAndMakeVisible(comboFilterSlope);

    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "fltType", comboFilterType));
    comboAttachments.push_back(std::make_unique<ComboAttach>(vts, "fltSlope", comboFilterSlope));

    comboFilterType.onChange = [this] { updateFilterUIState(); };

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

    // --- 3. FILTER ENVELOPE アタッチメント ---
    knobFltEnvA.knob.setDoubleClickReturnValue(true, 0.01);
    knobFltEnvD.knob.setDoubleClickReturnValue(true, 0.3);
    knobFltEnvS.knob.setDoubleClickReturnValue(true, 1.0);
    knobFltEnvR.knob.setDoubleClickReturnValue(true, 0.3);
    knobFltEnvAmt.knob.setDoubleClickReturnValue(true, 0.0);

    addAndMakeVisible(knobFltEnvA);
    addAndMakeVisible(knobFltEnvD);
    addAndMakeVisible(knobFltEnvS);
    addAndMakeVisible(knobFltEnvR);
    addAndMakeVisible(knobFltEnvAmt);

    attachments.push_back(std::make_unique<Attachment>(vts, "fltEnvAttack",  knobFltEnvA.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "fltEnvDecay",   knobFltEnvD.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "fltEnvSustain", knobFltEnvS.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "fltEnvRelease", knobFltEnvR.knob));
    attachments.push_back(std::make_unique<Attachment>(vts, "fltEnvAmt",     knobFltEnvAmt.knob));

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
    fp.type    = (int)fetchFloat("fltType", 0.0f);
    fp.slope24 = fetchFloat("fltSlope", 0.0f) > 0.5f;

    const float baseCutoff  = fetchFloat("fltCutoff", 2000.0f);
    const float baseRes     = fetchFloat("fltRes", 0.707f);
    const float baseFormant = fetchFloat("fltFormant", 0.0f);
    const float baseCombMix = fetchFloat("fltCombMix", 0.5f);
    const float envAmt      = fetchFloat("fltEnvAmt", 0.0f);

    // ------------------------------------------------------------------
    // モジュレーション反映
    // 計算式は PluginProcessor 側の DSP と必ず一致させること
    // (Cutoff は Filter Envelope ±4oct + ModMatrix ±4oct の指数変調、
    //  Reso は ModMatrix ±5 の線形加算)。ズレるとカーブ表示と実際の音が食い違う。
    // ------------------------------------------------------------------
    const float modCutoff  = modMatrix != nullptr ? modMatrix->get(ModMatrix::DstFltCutoff)  : 0.0f;
    const float modReso    = modMatrix != nullptr ? modMatrix->get(ModMatrix::DstFltReso)    : 0.0f;
    const float modFormant = modMatrix != nullptr ? modMatrix->get(ModMatrix::DstFltFormant) : 0.0f;
    const float modComb    = modMatrix != nullptr ? modMatrix->get(ModMatrix::DstFltCombMix) : 0.0f;

    const float octaveShift = liveFilterEnvValue * envAmt * 4.0f + modCutoff * 4.0f;
    fp.cutoff  = juce::jlimit(20.0f, 20000.0f, baseCutoff * std::pow(2.0f, octaveShift));
    fp.res     = juce::jlimit(0.1f, 10.0f, baseRes + modReso * 5.0f);
    fp.formant = juce::jlimit(0.0f, 1.0f, baseFormant + modFormant);
    fp.combMix = juce::jlimit(0.0f, 1.0f, baseCombMix + modComb);

    filterCurveComp.updateFilterState(fp, currentSampleRateForCurve);
}

void ArpPanel::updateFilterUIState() noexcept
{
    updateFilterCurveDisplay();

    auto* pType = vts.getRawParameterValue("fltType");
    if (pType == nullptr) return;
    const int typeIdx = (int)pType->load();

    // Wavetableプロジェクト DualFilterEngine::FilterType と同じ並び
    //   0=LPF 1=HPF 2=BPF 3=Notch 4=Comb 5=LadderLPF 6=Vowel 7=CombPlus 8=Phaser
    const bool isSlopeType   = (typeIdx == PicoFilter::LPF || typeIdx == PicoFilter::HPF
                              || typeIdx == PicoFilter::BPF || typeIdx == PicoFilter::Phaser);
    const bool isVowel       = (typeIdx == PicoFilter::Vowel);
    const bool usesMixKnob   = (typeIdx == PicoFilter::CombPlus || typeIdx == PicoFilter::Phaser);

    comboFilterSlope.setEnabled(isSlopeType);
    knobFilterCutoff.setEnabled(true);
    knobFilterRes.setEnabled(true);
    knobFilterFormant.setEnabled(isVowel);
    knobFilterCombMix.setEnabled(usesMixKnob);
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
    drawSectionHeader("MAIN FILTER (LPF/HPF/BPF/Notch/Comb/Ladder/Vowel/Comb+/Phaser)", 20, 148, 430, PicoColors::mint);
    drawSectionHeader("FILTER ENVELOPE", 470, 248, 300, PicoColors::babyBlue);
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

    // --- FILTER セクション (左: コンボ+カーブ / 右: ノブ類) ---
    // 左側: コンボボックス
    btnFilterEnable.setBounds(20,  172, 85, 26);
    comboFilterType.setBounds(111, 172, 130, 26);
    comboFilterSlope.setBounds(247, 172, 90, 26);

    // 左側: フィルターカーブ表示 (縦幅拡大)
    filterCurveComp.setBounds(20, 206, 420, 128);

    // 右側上段: フィルターパラメータノブ
    const int fltKnobY = 170;
    const int fltKnobW = 58;
    knobFilterCutoff.setBounds(470,   fltKnobY, fltKnobW, knobH);
    knobFilterRes.setBounds(536,      fltKnobY, fltKnobW, knobH);
    knobFilterFormant.setBounds(602,  fltKnobY, fltKnobW, knobH);
    knobFilterCombMix.setBounds(668,  fltKnobY, fltKnobW, knobH);

    // 右側下段: フィルターエンベロープノブ
    const int fltEnvY = 268;
    const int fltEnvW = 55;
    knobFltEnvA.setBounds(470,    fltEnvY, fltEnvW, knobH);
    knobFltEnvD.setBounds(530,    fltEnvY, fltEnvW, knobH);
    knobFltEnvS.setBounds(590,    fltEnvY, fltEnvW, knobH);
    knobFltEnvR.setBounds(650,    fltEnvY, fltEnvW, knobH);
    knobFltEnvAmt.setBounds(710,  fltEnvY, fltEnvW, knobH);
}
