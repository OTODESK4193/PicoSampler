// ==========================================
// File: ModPanel.h
// MODタブ: ソース段 (LFO / Env サブタブ ※Macro除外) + 16スロットマトリクス (Page1/Page2)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "ColorPalette.h"

class ModPanel : public juce::Component
{
public:
    explicit ModPanel(PicoSamplerAudioProcessor& processor);
    ~ModPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // LFO Rate ノブ (LFO→LFO クロスモジュレーションの変調レンジ表示用に外部公開)
    ValueKnob& getLfoRateKnob(int idx) { return lfoRateKnob[(size_t)juce::jlimit(0, ModMatrix::kNumLfos - 1, idx)]; }

private:
    void setupKnob(ValueKnob& s, const juce::String& paramID, juce::Colour accent);
    void setupCombo(juce::ComboBox& c, const juce::String& paramID);
    void setSourceTab(int t);          // 0=LFO, 1=Env
    void setMatrixPage(int p);         // 0=slot1-8, 1=slot9-16
    void styleTab(juce::TextButton& b, bool active);

    PicoSamplerAudioProcessor* procPtr = nullptr;
    juce::AudioProcessorValueTreeState& vts;

    // --- ソース・サブタブ (Macroなし) ---
    juce::TextButton lfoTabBtn { "LFO" };
    juce::TextButton envTabBtn { "ENV" };
    int activeSrcTab = 0;

    // --- LFO ×4 ---
    std::array<juce::ComboBox, ModMatrix::kNumLfos> lfoWaveBox;
    std::array<std::unique_ptr<GlowToggle>, ModMatrix::kNumLfos> lfoSyncButton;
    std::array<ValueKnob, ModMatrix::kNumLfos> lfoRateKnob;
    std::array<juce::ComboBox, ModMatrix::kNumLfos> lfoSyncRateBox;

    // --- ENV ×3 (ADSR) ---
    std::array<ValueKnob, ModMatrix::kNumEnvs> envAttackKnob, envDecayKnob, envSustainKnob, envReleaseKnob;
    std::array<std::unique_ptr<GlowToggle>, ModMatrix::kNumEnvs> envLoopButton;

    // --- マトリクス16行 (2ページ) ---
    juce::TextButton page1Btn { "PAGE 1 (1-8)" };
    juce::TextButton page2Btn { "PAGE 2 (9-16)" };
    int matrixPage = 0;
    std::array<juce::Label, ModMatrix::kNumSlots> rowLabel;
    std::array<juce::ComboBox, ModMatrix::kNumSlots> srcBox, dstBox;
    std::array<ValueKnob, ModMatrix::kNumSlots> amtSlider;
    std::array<std::unique_ptr<GlowToggle>, ModMatrix::kNumSlots> uniButton;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>   sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>>   buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModPanel)
};
