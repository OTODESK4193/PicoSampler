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
        setMouseDragSensitivity(baseSensitivity);
    }

    // ------------------------------------------------------------------
    // ドラッグ解像度モード
    //   通常      : baseSensitivity      (既定の細かさ)
    //   Ctrl 押下 : coarseSensitivity    (一気に動かす。ざっくり狙う用)
    //   Shift 押下: fineSensitivity      (さらに追い込む用)
    // 感度は「フルレンジを動かすのに必要なピクセル数」なので、
    // 値が小さいほど速く動く。
    // ------------------------------------------------------------------
    void setBaseSensitivity(int s) noexcept
    {
        baseSensitivity = juce::jmax(1, s);
        if (!isMouseButtonDown()) setMouseDragSensitivity(baseSensitivity);
    }

    void setCoarseSensitivity(int s) noexcept { coarseSensitivity = juce::jmax(1, s); }
    void setFineSensitivity(int s)   noexcept { fineSensitivity   = juce::jmax(1, s); }

    std::function<double(double)> customSnapFunction;

    double snapValue(double v, juce::Slider::DragMode mode) override
    {
        double snapped = juce::Slider::snapValue(v, mode);
        if (customSnapFunction != nullptr)
        {
            snapped = customSnapFunction(snapped);
        }
        return snapped;
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

        applySensitivityForMods(e.mods);
        juce::Slider::mouseDown(e);
    }

    // NOTE: 修飾キーの判定はドラッグ「開始時」のみ行う。
    // JUCE の Slider はドラッグ開始位置を基準に相対計算するため、
    // 途中で感度を変えると値が飛ぶ。多くの DAW も押下時に確定する挙動なので、
    // Ctrl / Shift はノブを掴む前に押しておく。
    void mouseUp(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseUp(e);
        setMouseDragSensitivity(baseSensitivity);
        activeSensitivity = baseSensitivity;
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        // ダブルクリックでデフォルト値（または中央値）に復帰
        juce::Slider::mouseDoubleClick(e);
    }

private:
    int baseSensitivity   = 2000;
    int coarseSensitivity = 220;    // Ctrl: 一気に動かす
    int fineSensitivity   = 9000;   // Shift: 微調整
    int activeSensitivity = 2000;

    int sensitivityForMods(const juce::ModifierKeys& mods) const noexcept
    {
        if (mods.isCtrlDown() || mods.isCommandDown()) return coarseSensitivity;
        if (mods.isShiftDown()) return fineSensitivity;
        return baseSensitivity;
    }

    void applySensitivityForMods(const juce::ModifierKeys& mods) noexcept
    {
        activeSensitivity = sensitivityForMods(mods);
        setMouseDragSensitivity(activeSensitivity);
    }

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
