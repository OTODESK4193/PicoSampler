// ==========================================
// File: PluginEditor.cpp
// PicoSampler メインエディタ GUI 実装 (Granularタブアンダーラインスタイル)
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

    mainPanel.getWaveformDisplay().setVisualizerData(&p.getVisualizerData());

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
                active ? juce::Colours::white : juce::Colours::grey);
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

    repaint(0, 0, getWidth(), 50);
}

void PicoSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    // 1. ヘッダーロゴ
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    juce::ColourGradient titleGrad(PicoColors::mint, 20.0f, 15.0f, PicoColors::pink, 250.0f, 35.0f, false);
    g.setGradientFill(titleGrad);
    g.drawText("P I C O  S A M P L E R", 20, 10, 260, 28, juce::Justification::centredLeft);

    // 2. HUD 情報 (ファイル名 & ルートキー)
    const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
    const auto& slot = audioProcessor.getSamplerEngine().getSlot(activeIdx);
    const auto& meta = slot.getMetadata();

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText("Slot " + juce::String(activeIdx + 1) + ": " + (slot.isReady() ? meta.fileName : "Empty"), 280, 10, 230, 14, juce::Justification::left, true);
    g.drawText("Root Key: " + (slot.isReady() ? juce::MidiMessage::getMidiNoteName(meta.rootKey, true, true, 4) : "-"), 280, 25, 230, 14, juce::Justification::left);

    // 3. アクティブタブの下線バー描画 (Granularスタイル)
    auto drawUnderline = [&g](const juce::TextButton& b, juce::Colour c)
    {
        const auto r = b.getBounds();
        g.setColour(c);
        g.fillRoundedRectangle((float)r.getX() + 6.0f, (float)r.getBottom() - 3.0f,
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
    const int tabW = 75;
    const int tabH = 28;
    const int startX = 520;

    btnTabMain.setBounds(startX, 10, tabW, tabH);
    btnTabSlot.setBounds(startX + tabW + 5, 10, tabW, tabH);
    btnTabArp.setBounds(startX + (tabW + 5) * 2, 10, tabW, tabH);
    btnTabMod.setBounds(startX + (tabW + 5) * 3, 10, tabW, tabH);
    btnTabFx.setBounds(startX + (tabW + 5) * 4, 10, tabW, tabH);
    btnTabConfig.setBounds(startX + (tabW + 5) * 5, 10, tabW, tabH);
    btnPresets.setBounds(1080 - 90, 10, 75, tabH);

    const auto contentBounds = juce::Rectangle<int>(0, 48, 1080, 652);
    mainPanel.setBounds(contentBounds);
    slotPanel.setBounds(contentBounds);
    arpPanel.setBounds(contentBounds);
    modPanel.setBounds(contentBounds);
    fxPanel.setBounds(contentBounds);
    configPanel.setBounds(contentBounds);

    presetBrowser.setBounds(getLocalBounds());
}

void PicoSamplerAudioProcessorEditor::timerCallback()
{
    const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
    mainPanel.getWaveformDisplay().setSampleSlot(&audioProcessor.getSamplerEngine().getSlot(activeIdx));
    slotPanel.updateSlotStates();
    mainPanel.updateStates();
    repaint(0, 0, 520, 48);
}
