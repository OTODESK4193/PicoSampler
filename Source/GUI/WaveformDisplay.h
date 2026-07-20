// ==========================================
// File: WaveformDisplay.h
// 波形表示コンポーネント (ZeroCrossingスナップ定義追加)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "../DSP/ModMatrix.h"
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
    void resized() override {}

    void setSampleSlot(const SampleSlot* slot) { currentSlot = slot; }
    void setVisualizerData(SampleVisualizerData* data) { visualizerData = data; }
    void setActiveSlotIndex(int idx) { activeSlot = idx; }
    void setAPVTS(juce::AudioProcessorValueTreeState* state) { vts = state; }
    void setModMatrix(const ModMatrix* mod) { modMatrix = mod; }

    std::function<void(const juce::File& file)> onFileDropped;
    std::function<void(int slotIdx)> onClearSlotRequested;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    float findZeroCrossingRatio(float targetRatio) const noexcept;

private:
    void timerCallback() override;
    juce::Colour getSpectralColor(float brightness) const noexcept;

    const SampleSlot* currentSlot = nullptr;
    SampleVisualizerData* visualizerData = nullptr;
    juce::AudioProcessorValueTreeState* vts = nullptr;
    const ModMatrix* modMatrix = nullptr;
    int activeSlot = 0;

    enum class DragTarget { None, SampleStart, SampleEnd, LoopStart, LoopEnd, Scrollbar };
    DragTarget activeDrag = DragTarget::None;

    float zoomLevel = 1.0f;
    float viewStartRatio = 0.0f;
    float scrollDragStartRatio = 0.0f;
    
    int dragStartX = 0;
    float dragStartValue = 0.0f;

    float animPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
