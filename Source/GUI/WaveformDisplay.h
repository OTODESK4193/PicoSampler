// ==========================================
// File: WaveformDisplay.h
// 波形表示 ＆ KeyRange (88鍵) 表示・操作対応コンポーネント
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "../DSP/SampleSlot.h"
#include "../DSP/SampleVisualizerData.h"
#include "ColorPalette.h"

class WaveformDisplay : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    WaveformDisplay();
    ~WaveformDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSampleSlot(const SampleSlot* slot) { currentSlot = slot; }
    void setVisualizerData(SampleVisualizerData* data) { visualizerData = data; }

    void updateKeyRanges(const std::array<std::pair<int, int>, 8>& ranges,
                         const std::array<int, 8>& roots,
                         const std::array<bool, 8>& readyStates,
                         int activeSlotIdx)
    {
        slotRanges = ranges;
        rootKeys = roots;
        slotReady = readyStates;
        activeSlot = activeSlotIdx;
    }

    std::function<void(const juce::File& file)> onFileDropped;
    std::function<void(float startRatio, float endRatio)> onSampleRangeChanged;
    std::function<void(int slotIdx, int lowNote, int highNote)> onKeyRangeChanged;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    juce::Colour getSpectralColor(float brightness) const noexcept;

    float noteToX(int midiNote, float w) const noexcept;
    int xToNote(float x, float w) const noexcept;

    const SampleSlot* currentSlot = nullptr;
    SampleVisualizerData* visualizerData = nullptr;

    std::array<std::pair<int, int>, 8> slotRanges {};
    std::array<int, 8> rootKeys {};
    std::array<bool, 8> slotReady {};
    int activeSlot = 0;

    enum class DragTarget { None, SampleStart, SampleEnd, KeyRangeLow, KeyRangeHigh };
    DragTarget activeDrag = DragTarget::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
