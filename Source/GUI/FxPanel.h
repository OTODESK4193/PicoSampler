// ==========================================
// File: FxPanel.h
// FXタブ: 5スロットカード（D&Dで適用順を並べ替え）
//        + スロット選択で下部に選択FXの詳細パラメータが出現 (Granular完全移植)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "ValueKnob.h"
#include "ColorPalette.h"

// ------------------------------------------
// 1スロット分のカード
// ------------------------------------------
class FxSlotCard : public juce::Component,
                   public juce::DragAndDropTarget
{
public:
    FxSlotCard(PicoSamplerAudioProcessor& processor, int slotIndex,
               std::function<void(int, int)> onSwapCallback,
               std::function<void(int)> onSelectCallback,
               std::function<void()> onTypeChangedCallback);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSelected(bool shouldBeSelected)
    {
        if (selected != shouldBeSelected) { selected = shouldBeSelected; repaint(); }
    }

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    // --- DragAndDropTarget ---
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit(const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped(const SourceDetails& details) override;

    ValueKnob& getAmountKnob() { return amountKnob; }

private:
    PicoSamplerAudioProcessor& proc;
    const int slot; // 0-based
    std::function<void(int, int)> onSwap;
    std::function<void(int)> onSelect;
    std::function<void()> onTypeChanged;

    juce::ComboBox typeBox;
    ValueKnob amountKnob;
    juce::Label amountLabel;

    bool dragOver = false;
    bool selected = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxSlotCard)
};

// ------------------------------------------
// FXタブ本体
// ------------------------------------------
class FxPanel : public juce::Component,
                public juce::DragAndDropContainer
{
public:
    explicit FxPanel(PicoSamplerAudioProcessor& processor);
    ~FxPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    ValueKnob* getSlotAmountKnob(int slotIdx)
    {
        if (slotIdx >= 0 && slotIdx < (int)cards.size() && cards[(size_t)slotIdx])
            return &cards[(size_t)slotIdx]->getAmountKnob();
        return nullptr;
    }

    int getSelectedSlot() const noexcept { return selectedSlot; }
    const std::vector<std::unique_ptr<ValueKnob>>& getDetailKnobs() const { return detailKnobs; }

private:
    void swapSlots(int a, int b);
    void selectSlot(int slot);
    void rebuildDetails();
    void layoutDetails();
    int  getSlotType(int slot) const;

    PicoSamplerAudioProcessor& proc;

    std::array<std::unique_ptr<FxSlotCard>, FxChain::kNumSlots> cards;
    int selectedSlot = 0;

    // --- 動的な詳細コントロール ---
    std::unique_ptr<juce::ComboBox> detailCombo;
    std::unique_ptr<juce::Label>    detailComboLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> detailComboAttachment;

    std::vector<std::unique_ptr<ValueKnob>>   detailKnobs;
    std::vector<std::unique_ptr<juce::Label>> detailKnobLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> detailKnobAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPanel)
};
