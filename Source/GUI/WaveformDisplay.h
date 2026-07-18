// ==========================================
// File: WaveformDisplay.h
// 波形表示コンポーネント (純粋波形表示)
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
    void resized() override {}

    void setSampleSlot(const SampleSlot* slot) { currentSlot = slot; }
    void setVisualizerData(SampleVisualizerData* data) { visualizerData = data; }
    void setActiveSlotIndex(int idx) { activeSlot = idx; }

    std::function<void(const juce::File& file)> onFileDropped;
    std::function<void(float startRatio, float endRatio)> onSampleRangeChanged;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    juce::Colour getSpectralColor(float brightness) const noexcept;

    const SampleSlot* currentSlot = nullptr;
    SampleVisualizerData* visualizerData = nullptr;
    int activeSlot = 0;

    enum class DragTarget { None, SampleStart, SampleEnd };
    DragTarget activeDrag = DragTarget::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
