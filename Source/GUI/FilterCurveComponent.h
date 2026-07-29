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

    void updateFilterState(const PicoFilter::Params& params, double sr) noexcept
    {
        // タイマーから毎フレーム呼ばれるため、実際に変化した時だけ再描画する。
        // (モジュレーション中は毎回変わるので結果的にヌルヌル動く)
        sampleRate = sr > 1000.0 ? sr : 44100.0;
        if (!hasChanged(params)) return;
        currentParams = params;
        repaint();
    }

    void paint(juce::Graphics& g) override;

private:
    bool hasChanged(const PicoFilter::Params& p) const noexcept
    {
        auto diff = [](float a, float b, float eps) { return std::abs(a - b) > eps; };
        return p.enable  != currentParams.enable
            || p.type    != currentParams.type
            || p.slope24 != currentParams.slope24
            || diff(p.cutoff,  currentParams.cutoff,  0.5f)
            || diff(p.res,     currentParams.res,     0.001f)
            || diff(p.formant, currentParams.formant, 0.001f)
            || diff(p.combMix, currentParams.combMix, 0.001f);
    }

    PicoFilter::Params currentParams;
    double sampleRate = 44100.0;

    std::array<float, 512> rawMag {};
};
