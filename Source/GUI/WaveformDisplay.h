// ==========================================
// File: WaveformDisplay.h
// 波形表示コンポーネント (解析演出 ＆ LoopEnd / 4点ドラッグ連動対応)
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
    void setAPVTS(juce::AudioProcessorValueTreeState* state) { vts = state; }

    std::function<void(const juce::File& file)> onFileDropped;

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
    juce::AudioProcessorValueTreeState* vts = nullptr;
    int activeSlot = 0;

    enum class DragTarget { None, SampleStart, SampleEnd, LoopStart, LoopEnd };
    DragTarget activeDrag = DragTarget::None;

    float animPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
