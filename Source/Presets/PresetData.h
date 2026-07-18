// ==========================================
// File: PresetData.h
// ファクトリープリセット定義
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>

struct PresetItem
{
    juce::String name;
    juce::String category;
    juce::String author;
};

class PresetData
{
public:
    static std::vector<PresetItem> getFactoryPresets()
    {
        return {
            { "Default Init", "Basic", "OTODESK" },
            { "8-Slot Drum Kit", "Drums", "OTODESK" },
            { "Layered Pad Engine", "Pads", "OTODESK" },
            { "Randomized Keys", "Keys", "OTODESK" }
        };
    }
};
