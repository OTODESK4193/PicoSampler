// ==========================================
// File: FilterCurveComponent.h
// リアルタイム フィルターレスポンスカーブ表示 (QuadMorphFilter完全移植)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "../DSP/PicoFilter.h"
#include "ColorPalette.h"

class FilterCurveComponent : public juce::Component
{
public:
    FilterCurveComponent() = default;
    ~FilterCurveComponent() override = default;

    void updateFilterState(const PicoFilter::Params& params) noexcept
    {
        currentParams = params;
        repaint();
    }

    void paint(juce::Graphics& g) override;

private:
    PicoFilter dummyFilter;
    PicoFilter::Params currentParams;

    std::array<float, 512> rawMag {};
};
