// ==========================================
// File: WaveformDisplay.h
// ドットマトリクス 波形・ループマーカー描画コンポーネント
// ドラッグ可能マーカー & スペクトルグラデーション着色
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "../DSP/SampleSlot.h"
#include "../DSP/SampleVisualizerData.h"
#include "ColorPalette.h"

class WaveformDisplay : public juce::Component, public juce::FileDragAndDropTarget, public juce::Timer
{
public:
    WaveformDisplay();
    ~WaveformDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSampleSlot(SampleSlot* slot) { currentSlot = slot; repaint(); }
    void setVisualizerData(SampleVisualizerData* data) { visData = data; }

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    void timerCallback() override;

    std::function<void(float startRatio, float endRatio)> onSampleRangeChanged;
    std::function<void(float loopStartRatio, float loopLenRatio)> onLoopRangeChanged;
    std::function<void(const juce::File& file)> onFileDropped;

private:
    enum class DragTarget { None, SampleStart, SampleEnd, LoopStart, LoopEnd };

    DragTarget activeDrag = DragTarget::None;
    SampleSlot* currentSlot = nullptr;
    SampleVisualizerData* visData = nullptr;

    juce::Colour getSpectralColor(float brightness) const noexcept;
};
