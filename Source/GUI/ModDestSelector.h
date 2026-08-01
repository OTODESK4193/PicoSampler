// ==========================================
// File: ModDestSelector.h
// MODマトリクスの Destination 選択用コントロール。
// 通常の ComboBox の代わりに、クリックすると系統別 (S1..S8 / ARP / Filter / LFO /
// FX / Sat / Chorus / Delay / Reverb / Freeze) にサブメニュー分類された
// ツリー状の PopupMenu を表示する。
//
// 【v1.1.0 での修正】
// 以前は AudioProcessorValueTreeState::getParameterAsValue() が返す
// juce::Value に直接ぶら下がっていた。この Value は APVTS 内部の
// ValueTree の子ノードを直接参照しているため、
//   apvts.replaceState()  (プリセット読込 / DAW セッション復元)
// でツリーが丸ごと差し替わると、参照先が孤立した古いノードのまま取り残される。
// その結果:
//   ・プリセットを読み込んでもアサイン先の表示が更新されない (None のまま)
//   ・以後この UI から値を変えてもパラメータに届かない
// という不具合になっていた。
//
// Slider / ComboBox / Button の各 Attachment は「パラメータ本体」を購読して
// いるため影響を受けない。ここだけが例外だったので、同じく
// juce::ParameterAttachment 経由に統一する。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <memory>
#include "ColorPalette.h"
#include "../DSP/ModMatrix.h"

class ModDestSelector : public juce::Component
{
public:
    ModDestSelector() = default;
    ~ModDestSelector() override = default;

    // buildMenu: カテゴリ分類済みの PopupMenu を返す関数を外部から注入する
    // (Dst の並び自体は ModMatrix 側が持っているため、ModPanel 側で構築する)。
    // 引数には現在の選択値が渡るので、該当項目にチェックを付けられる。
    std::function<juce::PopupMenu(int currentDst)> buildMenu;

    void bindTo(juce::AudioProcessorValueTreeState& state, const juce::String& paramID)
    {
        attachment.reset();
        param = state.getParameter(paramID);

        if (param == nullptr)
        {
            jassertfalse;   // パラメータIDの綴り間違い
            return;
        }

        // ParameterAttachment はパラメータ本体を購読し、変更をメッセージ
        // スレッドへマーシャリングしてくれる。replaceState でツリーが
        // 差し替わっても購読は切れない。
        attachment = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newDenormalisedValue) { updateFromValue(newDenormalisedValue); },
            nullptr);

        attachment->sendInitialUpdate();
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
        if (buildMenu == nullptr || attachment == nullptr) return;

        juce::PopupMenu menu = buildMenu(currentDst);
        juce::Component::SafePointer<ModDestSelector> safeThis(this);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [safeThis](int result)
            {
                if (safeThis == nullptr || result <= 0) return;
                if (safeThis->attachment == nullptr) return;

                // itemID = Dst + 1 (0 は「未選択で閉じた」を意味するPopupMenuの予約値のため)
                safeThis->attachment->setValueAsCompleteGesture((float)(result - 1));
            });
    }

private:
    void updateFromValue(float denormalisedValue)
    {
        // 非正規化値は float の丸め誤差で整数ちょうどにならないことがある。
        // 切り捨てるとアサイン先が1つ手前にずれるため必ず四捨五入する。
        const int dst = juce::roundToInt(denormalisedValue);

        static const auto names = ModMatrix::getDestNames();

        currentDst  = (dst >= 0 && dst < names.size()) ? dst : 0;
        currentText = names[currentDst];
        repaint();
    }

    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;

    int currentDst = 0;
    juce::String currentText { "None" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModDestSelector)
};
