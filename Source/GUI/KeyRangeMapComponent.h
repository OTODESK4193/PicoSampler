// ==========================================
// File: KeyRangeMapComponent.h
// 8スロット倍幅音域レーン ＆ リアル88鍵盤コンポーネント (発音点灯表示対応)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include <array>

class KeyRangeMapComponent : public juce::Component
{
public:
    KeyRangeMapComponent() = default;
    ~KeyRangeMapComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override {}

    void updateKeyRanges(const std::array<std::pair<int, int>, 8>& ranges,
                         const std::array<int, 8>& roots,
                         const std::array<bool, 8>& readyStates,
                         int activeSlotIdx)
    {
        slotRanges = ranges;
        rootKeys = roots;
        slotReady = readyStates;
        activeSlot = activeSlotIdx;
        repaint();
    }

    void setPlayingNotes(const std::array<bool, 128>& notes)
    {
        playingNotes = notes;
        repaint();
    }

    std::function<void(int slotIdx, int lowNote, int highNote)> onKeyRangeChanged;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    float noteToX(int midiNote, float w) const noexcept;
    int xToNote(float x, float w) const noexcept;

    std::array<std::pair<int, int>, 8> slotRanges {};
    std::array<int, 8> rootKeys {};
    std::array<bool, 8> slotReady {};
    std::array<bool, 128> playingNotes {}; // 発音中ノート
    int activeSlot = 0;

    enum class DragTarget { None, LowNote, HighNote };
    DragTarget activeDrag = DragTarget::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyRangeMapComponent)
};
