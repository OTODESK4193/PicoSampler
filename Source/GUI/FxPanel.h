// ==========================================
// File: FxPanel.h
// 5スロット D&D並べ替え対応 FX パネル (Granularより移植)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ArcDial.h"
#include "ValueKnob.h"
#include "GlowToggle.h"

class FxPanel : public juce::Component, public juce::DragAndDropContainer
{
public:
    FxPanel(juce::AudioProcessorValueTreeState& apvts);
    ~FxPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct FxCardComponent : public juce::Component, public juce::DragAndDropTarget
    {
        int slotIndex = 0;
        juce::ComboBox comboType;
        ValueKnob knobAmount;
        std::function<void(int src, int dst)> onReorder;

        FxCardComponent()
        {
            addAndMakeVisible(comboType);
            addAndMakeVisible(knobAmount);
        }

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
        void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override;
    };

    juce::AudioProcessorValueTreeState& vts;
    std::array<std::unique_ptr<FxCardComponent>, 5> fxCards;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<Attachment>> attachments;
    std::vector<std::unique_ptr<ComboAttach>> comboAttachments;
};
