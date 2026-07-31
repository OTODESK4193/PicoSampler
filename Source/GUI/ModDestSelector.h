// ==========================================
// File: ModDestSelector.h
// MODマトリクスの Destination 選択用コントロール。
// 通常の ComboBox の代わりに、クリックすると系統別 (S1..S8 / ARP / Filter / LFO /
// FX / Sat / Chorus / Delay / Reverb / Freeze) にサブメニュー分類された
// ツリー状の PopupMenu を表示する。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "../DSP/ModMatrix.h"

class ModDestSelector : public juce::Component,
                         private juce::Value::Listener
{
public:
    ModDestSelector() = default;

    ~ModDestSelector() override
    {
        if (bound) valueObj.removeListener(this);
    }

    // buildMenu: カテゴリ分類済みの PopupMenu を返す関数を外部から注入する
    // (Dst の並び自体は ModMatrix 側が持っているため、ModPanel 側で構築する)。
    std::function<juce::PopupMenu()> buildMenu;

    void bindTo(juce::AudioProcessorValueTreeState& state, const juce::String& paramID)
    {
        if (bound) valueObj.removeListener(this);

        valueObj = state.getParameterAsValue(paramID);
        valueObj.addListener(this);
        bound = true;
        updateFromValue();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        g.setColour(PicoColors::panel);
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(isMouseOver() ? PicoColors::mint : PicoColors::knobTrack);
        g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);

        auto textArea = b.reduced(6.0f, 0.0f);
        textArea.removeFromRight(14.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(currentText, textArea, juce::Justification::centredLeft, true);

        // 小さい下向き矢印 (通常のComboBoxと同じ見た目にして操作感を揃える)
        g.setColour(PicoColors::mint);
        const float ax = b.getRight() - 12.0f;
        const float ay = b.getCentreY();
        juce::Path arrow;
        arrow.addTriangle(ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.fillPath(arrow);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&)  override { repaint(); }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (buildMenu == nullptr) return;

        juce::PopupMenu menu = buildMenu();
        juce::Component::SafePointer<ModDestSelector> safeThis(this);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [safeThis](int result)
            {
                if (safeThis == nullptr || result <= 0) return;
                // itemID = Dst + 1 (0 は「未選択で閉じた」を意味するPopupMenuの予約値のため)
                safeThis->valueObj.setValue((double)(result - 1));
            });
    }

private:
    void valueChanged(juce::Value&) override { updateFromValue(); }

    void updateFromValue()
    {
        const int dst = (int)std::lround((double)valueObj.getValue());
        static const auto names = ModMatrix::getDestNames();
        currentText = (dst >= 0 && dst < names.size()) ? names[dst] : "None";
        repaint();
    }

    juce::Value valueObj;
    bool bound = false;
    juce::String currentText { "None" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModDestSelector)
};
