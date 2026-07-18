// ==========================================
// File: ValueKnob.h
// 右クリックで数値直接入力ポップアップ対応ノブ
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"

class ValueKnob : public juce::Slider
{
public:
    ValueKnob()
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showDirectInputEditor();
            return;
        }
        juce::Slider::mouseDown(e);
    }

private:
    void showDirectInputEditor()
    {
        auto* editor = new juce::TextEditor();
        editor->setSize(70, 24);
        editor->setText(juce::String(getValue(), 2));
        editor->setSelectAllWhenFocused(true);
        editor->setInputRestrictions(10, "0123456789.-");

        editor->onReturnKey = [this, editor]()
        {
            setValue(editor->getText().getDoubleValue(), juce::sendNotificationSync);
            if (auto* box = editor->findParentComponentOfClass<juce::CallOutBox>())
                box->exitModalState(0);
        };

        juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(editor), getScreenBounds(), nullptr);
    }
};
