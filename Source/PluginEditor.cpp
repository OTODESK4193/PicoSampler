// ==========================================
// File: PluginEditor.cpp
// PicoSampler メインエディタ GUI 実装
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

    addAndMakeVisible(btnTabMain);
    addAndMakeVisible(btnTabSlot);
    addAndMakeVisible(btnTabArp);
    addAndMakeVisible(btnTabMod);
    addAndMakeVisible(btnTabFx);
    addAndMakeVisible(btnTabConfig);
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

void PicoSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    // 1. ヘッダーロゴ ＆ HUD
    g.setColour(PicoColors::mint);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("PICO SAMPLER", 20, 10, 200, 30, juce::Justification::left);

    // 2. アクティブスロット HUD 情報
    const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
    const auto& slot = audioProcessor.getSamplerEngine().getSlot(activeIdx);
    const auto& meta = slot.getMetadata();

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText("File: " + (slot.isReady() ? meta.fileName : "Empty"), 200, 10, 250, 15, juce::Justification::left, true);
    g.drawText("Root Key: " + (slot.isReady() ? juce::MidiMessage::getMidiNoteName(meta.rootKey, true, true, 4) : "-"), 200, 25, 250, 15, juce::Justification::left);
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

void PicoSamplerAudioProcessorEditor::setActiveTab(int tabIndex)
{
    activeTab = tabIndex;
    mainPanel.setVisible(activeTab == 0);
    slotPanel.setVisible(activeTab == 1);
    arpPanel.setVisible(activeTab == 2);
    modPanel.setVisible(activeTab == 3);
    fxPanel.setVisible(activeTab == 4);
    configPanel.setVisible(activeTab == 5);
}

void PicoSamplerAudioProcessorEditor::timerCallback()
{
    const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
    mainPanel.getWaveformDisplay().setSampleSlot(&audioProcessor.getSamplerEngine().getSlot(activeIdx));
    repaint();
}
