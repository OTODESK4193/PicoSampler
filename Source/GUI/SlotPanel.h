// ==========================================
// File: SlotPanel.h
// 8スロット カード ＆ 88鍵ビジュアル パネル (グラフィック強化版)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "../DSP/SamplerEngine.h"
#include "ColorPalette.h"
#include "ValueKnob.h"

class SlotPanel : public juce::Component
{
public:
    SlotPanel(juce::AudioProcessorValueTreeState& apvts, SamplerEngine& engine);
    ~SlotPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateSlotStates();

private:
    struct SlotCardComponent : public juce::Component, public juce::FileDragAndDropTarget
    {
        int slotIndex = 0;
        bool isSelected = false;
        juce::String fileName = "Empty";
        int rootKey = 60;
        int lowNote = 0;
        int highNote = 127;
        std::function<void(int)> onSelect;
        std::function<void(int, const juce::File&)> onFileDropped;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        bool isInterestedInFileDrag(const juce::StringArray& files) override;
        void filesDropped(const juce::StringArray& files, int, int) override;
    };

    struct GraphicalKeyboardComponent : public juce::Component
    {
        std::array<std::pair<int, int>, 8> slotRanges {};
        std::array<int, 8> rootKeys {};
        std::array<bool, 8> slotEnabled {};
        int activeSlot = 0;
        std::function<void(int slotIdx, int low, int high)> onRangeChanged;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

    private:
        float noteToX(int midiNote) const noexcept;
        int xToNote(float x) const noexcept;

        int draggingSlot = -1;
        bool draggingLowEdge = true;
    };

    juce::AudioProcessorValueTreeState& vts;
    SamplerEngine& samplerEngine;

    std::array<std::unique_ptr<SlotCardComponent>, 8> slotCards;
    GraphicalKeyboardComponent keyboardVisualizer;

    std::array<std::unique_ptr<juce::Slider>, 8> lowNoteKnobs;
    std::array<std::unique_ptr<juce::Slider>, 8> highNoteKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 16> attachments;
};
