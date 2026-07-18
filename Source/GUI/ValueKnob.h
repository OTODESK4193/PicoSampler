// ==========================================
// File: ValueKnob.h
// 右クリックで数値直接入力 / ダブルクリックでデフォルト値リセット対応ノブ
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
        setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
        setTextBoxIsEditable(false);
        setColour(juce::Slider::textBoxTextColourId, PicoColors::textDim);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    juce::String getTextFromValue(double val) override
    {
        if (textFromValueFunction != nullptr)
            return textFromValueFunction(val);
        return juce::Slider::getTextFromValue(val);
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

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        // ダブルクリックでデフォルト値（または中央値）に復帰
        juce::Slider::mouseDoubleClick(e);
    }

private:
    void showDirectInputEditor()
    {
        auto* editor = new juce::TextEditor();
        editor->setSize(70, 24);
        editor->setText(getTextFromValue(getValue()));
        editor->setSelectAllWhenFocused(true);

        juce::Component::SafePointer<ValueKnob> safeThis(this);
        editor->onReturnKey = [safeThis, editor]()
        {
            if (safeThis != nullptr)
            {
                if (safeThis->valueFromTextFunction != nullptr)
                {
                    safeThis->setValue(safeThis->valueFromTextFunction(editor->getText()), juce::sendNotificationSync);
                }
                else
                {
                    safeThis->setValue(editor->getText().getDoubleValue(), juce::sendNotificationSync);
                }
            }

            if (auto* box = editor->findParentComponentOfClass<juce::CallOutBox>())
                box->exitModalState(0);
        };

        juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(editor), getScreenBounds(), nullptr);
    }
};
