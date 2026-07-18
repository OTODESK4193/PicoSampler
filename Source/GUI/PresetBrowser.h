// ==========================================
// File: PresetBrowser.h
// カテゴリ / 検索 / お気に入り機能付き プリセットブラウザ (Granularより移植)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"

class PresetBrowser : public juce::Component
{
public:
    PresetBrowser()
    {
        txtSearch.setTextToShowWhenEmpty("Search presets...", juce::Colours::grey);
        addAndMakeVisible(txtSearch);

        btnSave.setButtonText("SAVE");
        btnInit.setButtonText("INIT");
        btnClose.setButtonText("CLOSE");

        addAndMakeVisible(btnSave);
        addAndMakeVisible(btnInit);
        addAndMakeVisible(btnClose);

        lstCategories.setModel(&categoryListModel);
        lstPresets.setModel(&presetListModel);

        addAndMakeVisible(lstCategories);
        addAndMakeVisible(lstPresets);

        btnClose.onClick = [this] { setVisible(false); };
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(PicoColors::bgDk.withAlpha(0.95f));
        g.setColour(PicoColors::panel);
        g.fillRoundedRectangle(getLocalBounds().reduced(20).toFloat(), 8.0f);
        g.setColour(PicoColors::mint);
        g.drawRoundedRectangle(getLocalBounds().reduced(20).toFloat(), 8.0f, 2.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(30);
        auto topRow = area.removeFromTop(32);

        txtSearch.setBounds(topRow.removeFromLeft(200));
        btnSave.setBounds(topRow.removeFromRight(70));
        topRow.removeFromRight(10);
        btnInit.setBounds(topRow.removeFromRight(70));
        topRow.removeFromRight(10);
        btnClose.setBounds(topRow.removeFromRight(70));

        area.removeFromTop(15);
        lstCategories.setBounds(area.removeFromLeft(180));
        area.removeFromLeft(15);
        lstPresets.setBounds(area);
    }

    std::function<void()> onPresetSelected;

private:
    struct SimpleListModel : public juce::ListBoxModel
    {
        juce::StringArray items;
        int getNumRows() override { return items.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (selected) { g.setColour(PicoColors::mint.withAlpha(0.2f)); g.fillRect(0, 0, w, h); }
            g.setColour(selected ? PicoColors::mint : juce::Colours::white);
            g.drawText(items[row], 10, 0, w - 20, h, juce::Justification::centredLeft, true);
        }
    };

    juce::TextEditor txtSearch;
    juce::TextButton btnSave, btnInit, btnClose;
    juce::ListBox lstCategories, lstPresets;

    SimpleListModel categoryListModel, presetListModel;
};
