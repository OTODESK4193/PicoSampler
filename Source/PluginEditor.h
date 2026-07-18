// ==========================================
// File: PluginEditor.h
// PicoSampler メインエディタ GUI 定義 (KeyRangeMapコンポーネont独立追加)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/ArcDial.h"
#include "GUI/WaveformDisplay.h"
#include "GUI/KeyRangeMapComponent.h"
#include "GUI/MainPanel.h"
#include "GUI/ArpPanel.h"
#include "GUI/ModPanel.h"
#include "GUI/FxPanel.h"
#include "GUI/ConfigPanel.h"
#include "GUI/PresetBrowser.h"

class PicoSamplerAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    PicoSamplerAudioProcessorEditor(PicoSamplerAudioProcessor&);
    ~PicoSamplerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void setActiveTab(int tabIndex);
    void styleTabButton(juce::TextButton& b, bool active);

    PicoSamplerAudioProcessor& audioProcessor;
    ArcDialLookAndFeel lookAndFeel;

    WaveformDisplay waveDisplay;
    KeyRangeMapComponent keyRangeMap;

    // Header Toolbar Buttons
    juce::TextButton btnTabMain   { "MAIN" };
    juce::TextButton btnTabArp    { "ARP" };
    juce::TextButton btnTabMod    { "MOD" };
    juce::TextButton btnTabFx     { "FX" };
    juce::TextButton btnTabConfig { "CONFIG" };
    juce::TextButton btnPresets   { "PRESETS" };

    // Panels
    MainPanel mainPanel;
    ArpPanel arpPanel;
    ModPanel modPanel;
    FxPanel fxPanel;
    ConfigPanel configPanel;
    PresetBrowser presetBrowser;

    int activeTab = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PicoSamplerAudioProcessorEditor)
};
