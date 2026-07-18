// ==========================================
// File: ValueKnob.h
// 数値常時表示 + カスタムテキスト表示 + ダブルクリック直接入力対応ノブ
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

    void mouseDoubleClick(const juce::MouseEvent& /*e*/) override
    {
        showDirectInputEditor();
    }

private:
    void showDirectInputEditor()
    {
        auto* editor = new juce::TextEditor();
        editor->setSize(70, 24);
        editor->setText(getTextFromValue(getValue()));
        editor->setSelectAllWhenFocused(true);

        editor->onReturnKey = [this, editor]()
        {
            if (valueFromTextFunction != nullptr)
            {
                setValue(valueFromTextFunction(editor->getText()), juce::sendNotificationSync);
            }
            else
            {
                setValue(editor->getText().getDoubleValue(), juce::sendNotificationSync);
            }

            if (auto* box = editor->findParentComponentOfClass<juce::CallOutBox>())
                box->exitModalState(0);
        };

        juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(editor), getScreenBounds(), nullptr);
    }
};
