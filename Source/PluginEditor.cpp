// ==========================================
// File: PluginEditor.cpp
// PicoSampler メインエディタ GUI 実装 (ReAnalyzeボタン機能)
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

PicoSamplerAudioProcessorEditor::PicoSamplerAudioProcessorEditor(PicoSamplerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      mainPanel(p.getAPVTS()),
      arpPanel(p.getAPVTS()),
      modPanel(p.getAPVTS()),
      fxPanel(p),
      configPanel(p.getAPVTS())
{
    setLookAndFeel(&lookAndFeel);
    setSize(1080, 700);

    waveDisplay.setAPVTS(&p.getAPVTS());
    addAndMakeVisible(waveDisplay);
    addAndMakeVisible(keyRangeMap);

    for (auto* b : { &btnTabMain, &btnTabArp, &btnTabMod, &btnTabFx, &btnTabConfig })
    {
        addAndMakeVisible(*b);
    }

    btnReAnalyze.setColour(juce::TextButton::buttonColourId, PicoColors::knobTrack);
    btnReAnalyze.setColour(juce::TextButton::textColourOffId, PicoColors::mint);
    addAndMakeVisible(btnReAnalyze);

    btnPresets.setColour(juce::TextButton::buttonColourId, PicoColors::knobTrack);
    btnPresets.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(btnPresets);

    btnTabMain.onClick   = [this] { setActiveTab(0); };
    btnTabArp.onClick    = [this] { setActiveTab(1); };
    btnTabMod.onClick    = [this] { setActiveTab(2); };
    btnTabFx.onClick     = [this] { setActiveTab(3); };
    btnTabConfig.onClick = [this] { setActiveTab(4); };

    btnReAnalyze.onClick = [this] {
        const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
        audioProcessor.reanalyzeSlot(activeIdx);
        repaint();
    };

    btnPresets.onClick   = [this] { presetBrowser.setVisible(!presetBrowser.isVisible()); };

    addAndMakeVisible(mainPanel);
    addChildComponent(arpPanel);
    addChildComponent(modPanel);
    addChildComponent(fxPanel);
    addChildComponent(configPanel);

    presetBrowser.setVisible(false);
    addAndMakeVisible(presetBrowser);

    waveDisplay.setVisualizerData(&p.getVisualizerData());

    // 波形エリア D&D
    waveDisplay.onFileDropped = [this](const juce::File& file) {
        const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
        audioProcessor.getSamplerEngine().getSlot(activeIdx).loadFromFile(file);
        repaint();
    };

    // KeyRangeMap 操作ハンドラ
    keyRangeMap.onKeyRangeChanged = [this](int slotIdx, int low, int high) {
        if (auto* pLow = audioProcessor.getAPVTS().getParameter("slotLowNote_" + juce::String(slotIdx)))
            pLow->setValueNotifyingHost((float)low / 127.0f);
        if (auto* pHigh = audioProcessor.getAPVTS().getParameter("slotHighNote_" + juce::String(slotIdx)))
            pHigh->setValueNotifyingHost((float)high / 127.0f);
        repaint();
    };

    setActiveTab(0);
    startTimerHz(10);
}

PicoSamplerAudioProcessorEditor::~PicoSamplerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PicoSamplerAudioProcessorEditor::styleTabButton(juce::TextButton& b, bool active)
{
    b.setColour(juce::TextButton::buttonColourId,
                active ? PicoColors::panel : juce::Colours::transparentBlack);
    b.setColour(juce::TextButton::textColourOffId,
                active ? juce::Colours::white : PicoColors::textDim);
}

void PicoSamplerAudioProcessorEditor::setActiveTab(int tabIndex)
{
    activeTab = tabIndex;
    if (presetBrowser.isVisible()) presetBrowser.setVisible(false);

    mainPanel.setVisible(activeTab == 0);
    arpPanel.setVisible(activeTab == 1);
    modPanel.setVisible(activeTab == 2);
    fxPanel.setVisible(activeTab == 3);
    configPanel.setVisible(activeTab == 4);

    styleTabButton(btnTabMain,   activeTab == 0);
    styleTabButton(btnTabArp,    activeTab == 1);
    styleTabButton(btnTabMod,    activeTab == 2);
    styleTabButton(btnTabFx,     activeTab == 3);
    styleTabButton(btnTabConfig, activeTab == 4);

    repaint(0, 310, getWidth(), 45);
}

void PicoSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    // 1. ヘッダーロゴ
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    juce::ColourGradient titleGrad(PicoColors::mint, 20.0f, 15.0f, PicoColors::pink, 280.0f, 35.0f, false);
    titleGrad.addColour(0.5, PicoColors::lavender);
    g.setGradientFill(titleGrad);
    g.drawText("P I C O  S A M P L E R", 20, 10, 280, 28, juce::Justification::centredLeft);

    // 2. HUD 情報
    const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
    const auto& slot = audioProcessor.getSamplerEngine().getSlot(activeIdx);
    const auto& meta = slot.getMetadata();

    const int lowNote = (int)audioProcessor.getAPVTS().getRawParameterValue("slotLowNote_" + juce::String(activeIdx))->load();
    const int highNote = (int)audioProcessor.getAPVTS().getRawParameterValue("slotHighNote_" + juce::String(activeIdx))->load();

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("Slot " + juce::String(activeIdx + 1) + ": " + (slot.isReady() ? meta.fileName : "Empty"), 310, 8, 380, 15, juce::Justification::left, true);

    g.setColour(PicoColors::mint);
    g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
    const juce::String rootStr = slot.isReady() ? juce::MidiMessage::getMidiNoteName(meta.rootKey, true, true, 4) : "-";
    const juce::String rangeStr = juce::MidiMessage::getMidiNoteName(lowNote, true, true, 4) + " ~ " + juce::MidiMessage::getMidiNoteName(highNote, true, true, 4);

    g.drawText("Root Key: " + rootStr + "  |  Key Range: " + rangeStr, 310, 25, 380, 15, juce::Justification::left);

    // 3. アクティブタブの下線バー描画
    auto drawUnderline = [&g](const juce::TextButton& b, juce::Colour c)
    {
        const auto r = b.getBounds();
        g.setColour(c);
        g.fillRoundedRectangle((float)r.getX() + 6.0f, (float)r.getBottom() + 1.0f,
                               (float)r.getWidth() - 12.0f, 3.0f, 1.5f);
    };

    if (activeTab == 0) drawUnderline(btnTabMain, PicoColors::mint);
    if (activeTab == 1) drawUnderline(btnTabArp, PicoColors::lavender);
    if (activeTab == 2) drawUnderline(btnTabMod, PicoColors::babyBlue);
    if (activeTab == 3) drawUnderline(btnTabFx, PicoColors::pink);
    if (activeTab == 4) drawUnderline(btnTabConfig, PicoColors::sage);
}

void PicoSamplerAudioProcessorEditor::resized()
{
    // 1. 波形表示 (高さ 140px)
    waveDisplay.setBounds(20, 44, 1040, 140);

    // 2. 倍幅 KeyRangeMap コンポーネント (高さ 122px)
    keyRangeMap.setBounds(20, 190, 1040, 122);

    // 3. タブボタン群 (Y=322)
    const int tabY = 322;
    const int tabH = 26;

    btnTabMain.setBounds(20,  tabY, 70,  tabH);
    btnTabArp.setBounds(98,   tabY, 115, tabH);
    btnTabMod.setBounds(221,  tabY, 70,  tabH);
    btnTabFx.setBounds(299,   tabY, 70,  tabH);
    btnTabConfig.setBounds(377, tabY, 80,  tabH);

    btnReAnalyze.setBounds(1080 - 215, 10, 95, 26);
    btnPresets.setBounds(1080 - 110, 10, 90, 26);

    // 4. パネル領域 (Y=356)
    const auto panelBounds = juce::Rectangle<int>(0, 356, 1080, 344);
    mainPanel.setBounds(panelBounds);
    arpPanel.setBounds(panelBounds);
    modPanel.setBounds(panelBounds);
    fxPanel.setBounds(panelBounds);
    configPanel.setBounds(panelBounds);

    presetBrowser.setBounds(0, 356, 1080, 344);
}

void PicoSamplerAudioProcessorEditor::timerCallback()
{
    auto* pActive = audioProcessor.getAPVTS().getRawParameterValue("activeSlot");
    if (pActive == nullptr) return;

    const int activeIdx = (int)pActive->load();
    waveDisplay.setSampleSlot(&audioProcessor.getSamplerEngine().getSlot(activeIdx));
    waveDisplay.setActiveSlotIndex(activeIdx);

    mainPanel.updateStates();

    std::array<std::pair<int, int>, 8> ranges {};
    std::array<int, 8> roots {};
    std::array<bool, 8> readyStates {};

    for (int i = 0; i < 8; ++i)
    {
        const juce::String s = juce::String(i);
        auto* pLow  = audioProcessor.getAPVTS().getRawParameterValue("slotLowNote_" + s);
        auto* pHigh = audioProcessor.getAPVTS().getRawParameterValue("slotHighNote_" + s);

        const int low  = pLow  ? (int)pLow->load()  : 0;
        const int high = pHigh ? (int)pHigh->load() : 127;
        ranges[(size_t)i] = { low, high };

        const auto& slot = audioProcessor.getSamplerEngine().getSlot(i);
        roots[(size_t)i] = slot.getMetadata().rootKey;
        readyStates[(size_t)i] = slot.isReady();
    }

    keyRangeMap.updateKeyRanges(ranges, roots, readyStates, activeIdx);
    keyRangeMap.setPlayingNotes(audioProcessor.getSamplerEngine().getPlayingNotes());
    arpPanel.updateFilterUIState();
    repaint(0, 0, 700, 44);
}
