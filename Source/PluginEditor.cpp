// ==========================================
// File: PluginEditor.cpp
// PicoSampler メインエディタ GUI 実装 (KeyRangeMap波形統合 ＆ D&D対応)
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

PicoSamplerAudioProcessorEditor::PicoSamplerAudioProcessorEditor(PicoSamplerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      mainPanel(p.getAPVTS()),
      slotPanel(p.getAPVTS(), p.getSamplerEngine()),
      arpPanel(p.getAPVTS()),
      modPanel(p.getAPVTS()),
      fxPanel(p.getAPVTS()),
      configPanel(p.getAPVTS())
{
    setLookAndFeel(&lookAndFeel);
    setSize(1080, 700);

    addAndMakeVisible(waveDisplay);

    for (auto* b : { &btnTabMain, &btnTabSlot, &btnTabArp, &btnTabMod, &btnTabFx, &btnTabConfig })
    {
        addAndMakeVisible(*b);
    }

    btnPresets.setColour(juce::TextButton::buttonColourId, PicoColors::knobTrack);
    btnPresets.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(btnPresets);

    btnTabMain.onClick   = [this] { setActiveTab(0); };
    btnTabSlot.onClick   = [this] { setActiveTab(1); };
    btnTabArp.onClick    = [this] { setActiveTab(2); };
    btnTabMod.onClick    = [this] { setActiveTab(3); };
    btnTabFx.onClick     = [this] { setActiveTab(4); };
    btnTabConfig.onClick = [this] { setActiveTab(5); };
    btnPresets.onClick   = [this] { presetBrowser.setVisible(!presetBrowser.isVisible()); };

    addAndMakeVisible(mainPanel);
    addChildComponent(slotPanel);
    addChildComponent(arpPanel);
    addChildComponent(modPanel);
    addChildComponent(fxPanel);
    addChildComponent(configPanel);

    presetBrowser.setVisible(false);
    addAndMakeVisible(presetBrowser);

    waveDisplay.setVisualizerData(&p.getVisualizerData());

    // 波形エリアへの D&D ファイルドロップ対応 (アクティブスロットにロード)
    waveDisplay.onFileDropped = [this](const juce::File& file) {
        const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
        audioProcessor.getSamplerEngine().getSlot(activeIdx).loadFromFile(file);
        slotPanel.updateSlotStates();
        repaint();
    };

    // 波形エリア KeyRangeMap マウス操作対応
    waveDisplay.onKeyRangeChanged = [this](int slotIdx, int low, int high) {
        if (auto* pLow = audioProcessor.getAPVTS().getParameter("slotLowNote_" + juce::String(slotIdx)))
            pLow->setValueNotifyingHost((float)low / 127.0f);
        if (auto* pHigh = audioProcessor.getAPVTS().getParameter("slotHighNote_" + juce::String(slotIdx)))
            pHigh->setValueNotifyingHost((float)high / 127.0f);
        slotPanel.updateSlotStates();
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
    slotPanel.setVisible(activeTab == 1);
    arpPanel.setVisible(activeTab == 2);
    modPanel.setVisible(activeTab == 3);
    fxPanel.setVisible(activeTab == 4);
    configPanel.setVisible(activeTab == 5);

    styleTabButton(btnTabMain,   activeTab == 0);
    styleTabButton(btnTabSlot,   activeTab == 1);
    styleTabButton(btnTabArp,    activeTab == 2);
    styleTabButton(btnTabMod,    activeTab == 3);
    styleTabButton(btnTabFx,     activeTab == 4);
    styleTabButton(btnTabConfig, activeTab == 5);

    repaint(0, 290, getWidth(), 45);
}

void PicoSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    // 1. ヘッダーロゴ (グラデーション)
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    juce::ColourGradient titleGrad(PicoColors::mint, 20.0f, 15.0f, PicoColors::pink, 280.0f, 35.0f, false);
    titleGrad.addColour(0.5, PicoColors::lavender);
    g.setGradientFill(titleGrad);
    g.drawText("P I C O  S A M P L E R", 20, 10, 280, 28, juce::Justification::centredLeft);

    // 2. HUD 情報 (アクティブスロットのファイル名 & ルートキー)
    const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
    const auto& slot = audioProcessor.getSamplerEngine().getSlot(activeIdx);
    const auto& meta = slot.getMetadata();

    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText("Slot " + juce::String(activeIdx + 1) + ": " + (slot.isReady() ? meta.fileName : "Empty"), 310, 10, 300, 14, juce::Justification::left, true);
    g.setColour(PicoColors::textDim);
    g.drawText("Root Key: " + (slot.isReady() ? juce::MidiMessage::getMidiNoteName(meta.rootKey, true, true, 4) : "-"), 310, 25, 300, 14, juce::Justification::left);

    // 3. アクティブタブの下線バー描画 (Granularスタイル)
    auto drawUnderline = [&g](const juce::TextButton& b, juce::Colour c)
    {
        const auto r = b.getBounds();
        g.setColour(c);
        g.fillRoundedRectangle((float)r.getX() + 6.0f, (float)r.getBottom() + 1.0f,
                               (float)r.getWidth() - 12.0f, 3.0f, 1.5f);
    };

    if (activeTab == 0) drawUnderline(btnTabMain, PicoColors::mint);
    if (activeTab == 1) drawUnderline(btnTabSlot, PicoColors::peach);
    if (activeTab == 2) drawUnderline(btnTabArp, PicoColors::lavender);
    if (activeTab == 3) drawUnderline(btnTabMod, PicoColors::babyBlue);
    if (activeTab == 4) drawUnderline(btnTabFx, PicoColors::pink);
    if (activeTab == 5) drawUnderline(btnTabConfig, PicoColors::sage);
}

void PicoSamplerAudioProcessorEditor::resized()
{
    // 波形表示 (波形 + 下部88鍵 KeyRangeMap 統合エリア)
    waveDisplay.setBounds(20, 48, 1040, 240);

    const int tabY = 296;
    const int tabW = 88;
    const int tabH = 26;

    btnTabMain.setBounds(20,               tabY, tabW, tabH);
    btnTabSlot.setBounds(20 + (tabW + 8),  tabY, tabW, tabH);
    btnTabArp.setBounds(20 + (tabW + 8)*2, tabY, tabW, tabH);
    btnTabMod.setBounds(20 + (tabW + 8)*3, tabY, tabW, tabH);
    btnTabFx.setBounds(20 + (tabW + 8)*4,  tabY, tabW, tabH);
    btnTabConfig.setBounds(20 + (tabW + 8)*5, tabY, tabW, tabH);

    btnPresets.setBounds(1080 - 100, 10, 80, 26);

    // パネル領域
    const auto panelBounds = juce::Rectangle<int>(0, 330, 1080, 370);
    mainPanel.setBounds(panelBounds);
    slotPanel.setBounds(panelBounds);
    arpPanel.setBounds(panelBounds);
    modPanel.setBounds(panelBounds);
    fxPanel.setBounds(panelBounds);
    configPanel.setBounds(panelBounds);

    presetBrowser.setBounds(0, 330, 1080, 370);
}

void PicoSamplerAudioProcessorEditor::timerCallback()
{
    const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
    waveDisplay.setSampleSlot(&audioProcessor.getSamplerEngine().getSlot(activeIdx));

    slotPanel.updateSlotStates();
    mainPanel.updateStates();

    waveDisplay.updateKeyRanges(slotPanel.getSlotRanges(),
                                slotPanel.getRootKeys(),
                                slotPanel.getSlotReadyStates(),
                                activeIdx);

    repaint(0, 0, 600, 48);
}
