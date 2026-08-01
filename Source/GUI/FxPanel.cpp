// ==========================================
// File: FxPanel.cpp
// FxPanel 実装 (Granular完全移植)
// ==========================================
#include "FxPanel.h"
#include "../DSP/FxChain.h"
#include "../DSP/ModMatrix.h"

namespace
{
    constexpr int kCardW = 196;
    constexpr int kCardH = 136;
    constexpr int kCardGap = 211;
    constexpr int kCardY = 32;
    constexpr int kDetailY = 180;
}

// ------------------------------------------
// FxSlotCard
// ------------------------------------------
FxSlotCard::FxSlotCard(PicoSamplerAudioProcessor& processor, int slotIndex,
                       std::function<void(int, int)> onSwapCallback,
                       std::function<void(int)> onSelectCallback,
                       std::function<void()> onTypeChangedCallback)
    : proc(processor), slot(slotIndex),
      onSwap(std::move(onSwapCallback)),
      onSelect(std::move(onSelectCallback)),
      onTypeChanged(std::move(onTypeChangedCallback))
{
    const juce::String idx(slot + 1);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(proc.getAPVTS().getParameter("fx" + idx + "Type")))
        typeBox.addItemList(choiceParam->choices, 1);
    addAndMakeVisible(typeBox);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.getAPVTS(), "fx" + idx + "Type", typeBox);
    typeBox.onChange = [this]
    {
        if (onSelect != nullptr) onSelect(slot);
        if (onTypeChanged != nullptr) onTypeChanged();
    };

    amountKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    amountKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    amountKnob.setColour(juce::Slider::rotarySliderFillColourId, PicoColors::mint);
    amountKnob.setPopupDisplayEnabled(true, true, this);
    addAndMakeVisible(amountKnob);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.getAPVTS(), "fx" + idx + "Amount", amountKnob);

    amountLabel.setText("AMOUNT", juce::dontSendNotification);
    amountLabel.setJustificationType(juce::Justification::centred);
    amountLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    amountLabel.setColour(juce::Label::textColourId, PicoColors::textDim);
    addAndMakeVisible(amountLabel);
}

void FxSlotCard::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour(selected ? PicoColors::panel.brighter(0.08f) : PicoColors::panel);
    g.fillRoundedRectangle(bounds, 10.0f);

    juce::Colour border = PicoColors::panelLine;
    float borderW = 1.0f;
    if (dragOver)      { border = PicoColors::mint.withAlpha(0.9f); borderW = 2.0f; }
    else if (selected) { border = PicoColors::mint.withAlpha(0.85f); borderW = 1.6f; }
    g.setColour(border);
    g.drawRoundedRectangle(bounds.reduced(0.75f), 10.0f, borderW);

    // ヘッダー (ドラッグハンドル)
    g.setColour(PicoColors::knobTrack.withAlpha(0.6f));
    g.fillRoundedRectangle(bounds.withHeight(24.0f), 10.0f);

    g.setColour(selected ? PicoColors::mint : PicoColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
    g.drawText("SLOT " + juce::String(slot + 1), 10, 3, 90, 18, juce::Justification::centredLeft);

    // ドラッグハンドルアイコン (≡)
    g.setColour(PicoColors::textDim);
    const float hx = bounds.getWidth() - 26.0f;
    for (int i = 0; i < 3; ++i)
        g.fillRoundedRectangle(hx, 7.0f + (float)i * 4.0f, 14.0f, 2.0f, 1.0f);
}

void FxSlotCard::resized()
{
    typeBox.setBounds(10, 30, getWidth() - 20, 24);
    amountKnob.setBounds((getWidth() - 54) / 2, 60, 54, 52);
    amountLabel.setBounds(0, 114, getWidth(), 13);
}

void FxSlotCard::mouseDown(const juce::MouseEvent&)
{
    if (onSelect != nullptr) onSelect(slot);
}

void FxSlotCard::mouseDrag(const juce::MouseEvent& e)
{
    if (e.mouseDownPosition.getY() <= 24.0f)
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            if (!container->isDragAndDropActive())
                container->startDragging(juce::var(slot), this);
}

bool FxSlotCard::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.isInt() && (int)details.description != slot;
}

void FxSlotCard::itemDropped(const SourceDetails& details)
{
    dragOver = false;
    repaint();
    if (onSwap != nullptr)
        onSwap((int)details.description, slot);
}

// ------------------------------------------
// FxPanel
// ------------------------------------------
FxPanel::FxPanel(PicoSamplerAudioProcessor& p) : proc(p)
{
    for (int i = 0; i < FxChain::kNumSlots; ++i)
    {
        cards[(size_t)i] = std::make_unique<FxSlotCard>(proc, i,
            [this](int a, int b) { swapSlots(a, b); },
            [this](int s) { selectSlot(s); },
            [this] { rebuildDetails(); });
        addAndMakeVisible(*cards[(size_t)i]);
    }

    selectSlot(0);
}

int FxPanel::getSlotType(int slot) const
{
    if (auto* p = proc.getAPVTS().getRawParameterValue("fx" + juce::String(slot + 1) + "Type"))
        return juce::roundToInt(p->load());
    return 0;
}

void FxPanel::selectSlot(int slot)
{
    slot = juce::jlimit(0, FxChain::kNumSlots - 1, slot);
    selectedSlot = slot;
    for (int i = 0; i < FxChain::kNumSlots; ++i)
        if (cards[(size_t)i] != nullptr)
            cards[(size_t)i]->setSelected(i == slot);
    rebuildDetails();
    repaint();
}

void FxPanel::swapSlots(int a, int b)
{
    if (a == b || a < 0 || b < 0 || a >= FxChain::kNumSlots || b >= FxChain::kNumSlots)
        return;

    for (const auto* suffix : { "Type", "Amount" })
    {
        auto* pa = proc.getAPVTS().getParameter("fx" + juce::String(a + 1) + suffix);
        auto* pb = proc.getAPVTS().getParameter("fx" + juce::String(b + 1) + suffix);
        if (pa == nullptr || pb == nullptr) continue;

        const float va = pa->getValue();
        const float vb = pb->getValue();

        pa->beginChangeGesture();
        pa->setValueNotifyingHost(vb);
        pa->endChangeGesture();

        pb->beginChangeGesture();
        pb->setValueNotifyingHost(va);
        pb->endChangeGesture();
    }

    selectSlot(b);
}

void FxPanel::rebuildDetails()
{
    detailComboAttachment.reset();
    detailKnobAttachments.clear();
    detailCombo.reset();
    detailComboLabel.reset();
    detailKnobs.clear();
    detailKnobLabels.clear();
    detailKnobDsts.clear();

    struct KnobDef { const char* id; const char* label; int dst; };
    std::vector<KnobDef> knobDefs;
    const char* comboId = nullptr;
    const char* comboLabelText = nullptr;

    // 【重要】 各ノブに ModMatrix::Dst を明示的に対応付けておく。
    // detailKnobs は選択中のFXタイプに応じて毎回中身が totally 入れ替わる
    // (常に Sat の3個とは限らない) ため、呼び出し側 (PluginEditor) で
    // 「先頭からのindex = DstSatDriveからの連番」と決め打ちすると、
    // Chorus/Delay/Freeze/Reverb 選択時に全く別のDstの変調レンジを
    // 表示してしまうバグになる。ここで正しいDstを持たせて解消する。
    switch (getSlotType(selectedSlot))
    {
    case FxChain::Saturation:
        comboId = "satAlgo"; comboLabelText = "ALGO";
        knobDefs = { { "satDrive", "DRIVE", ModMatrix::DstSatDrive },
                     { "satPreHz", "PRE-HPF", ModMatrix::DstSatPreHz },
                     { "satTrim", "TRIM", ModMatrix::DstSatTrim } };
        break;
    case FxChain::Chorus:
        knobDefs = { { "choRate", "RATE", ModMatrix::DstChoRate },
                     { "choDepth", "DEPTH", ModMatrix::DstChoDepth },
                     { "choWidth", "WIDTH", ModMatrix::DstChoWidth } };
        break;
    case FxChain::Delay:
        comboId = "dlyTime"; comboLabelText = "TIME";
        knobDefs = { { "dlyFeedback", "FEEDBACK", ModMatrix::DstDlyFeedback },
                     { "dlyDuck", "DUCK", ModMatrix::DstDlyDuck },
                     { "dlyDamp", "DAMP", ModMatrix::DstDlyDamp } };
        break;
    case FxChain::Freeze:
        knobDefs = { { "frzSize", "SIZE", ModMatrix::DstFrzSize },
                     { "frzFeedback", "FEEDBACK", ModMatrix::DstFrzFeedback },
                     { "frzDamp", "DAMP", ModMatrix::DstFrzDamp } };
        break;
    case FxChain::Reverb:
        knobDefs = { { "revDecay", "DECAY", ModMatrix::DstRevDecay },
                     { "revShimmer", "SHIMMER", ModMatrix::DstRevShimmer },
                     { "revDamp", "DAMP", ModMatrix::DstRevDamp },
                     { "revMod", "MOD", ModMatrix::DstRevMod } };
        break;
    default:
        break;
    }

    if (comboId != nullptr)
    {
        detailCombo = std::make_unique<juce::ComboBox>();
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(proc.getAPVTS().getParameter(comboId)))
            detailCombo->addItemList(choiceParam->choices, 1);
        addAndMakeVisible(*detailCombo);
        detailComboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            proc.getAPVTS(), comboId, *detailCombo);

        detailComboLabel = std::make_unique<juce::Label>();
        detailComboLabel->setText(comboLabelText, juce::dontSendNotification);
        detailComboLabel->setJustificationType(juce::Justification::centredLeft);
        detailComboLabel->setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        detailComboLabel->setColour(juce::Label::textColourId, PicoColors::textDim);
        addAndMakeVisible(*detailComboLabel);
    }

    for (const auto& def : knobDefs)
    {
        auto knob = std::make_unique<ValueKnob>();
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob->setColour(juce::Slider::rotarySliderFillColourId, PicoColors::pink);
        knob->setPopupDisplayEnabled(true, true, this);
        addAndMakeVisible(*knob);
        detailKnobAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            proc.getAPVTS(), def.id, *knob));
        detailKnobs.push_back(std::move(knob));
        detailKnobDsts.push_back(def.dst);

        auto label = std::make_unique<juce::Label>();
        label->setText(def.label, juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        label->setColour(juce::Label::textColourId, PicoColors::textDim);
        addAndMakeVisible(*label);
        detailKnobLabels.push_back(std::move(label));
    }

    layoutDetails();
    repaint();
}

void FxPanel::layoutDetails()
{
    int x = 20;

    if (detailCombo != nullptr)
    {
        detailComboLabel->setBounds(x, kDetailY + 24, 100, 14);
        detailCombo->setBounds(x, kDetailY + 40, 130, 26);
        x += 150;
    }

    for (size_t i = 0; i < detailKnobs.size(); ++i)
    {
        detailKnobs[i]->setBounds(x, kDetailY + 32, 58, 56);
        detailKnobLabels[i]->setBounds(x - 8, kDetailY + 90, 74, 13);
        x += 82;
    }
}

void FxPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    auto drawSection = [&g](const juce::String& name, int x, int y, int w, juce::Colour accent)
    {
        g.setColour(accent);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(name, x, y, w, 14, juce::Justification::centredLeft);
        g.setColour(accent.withAlpha(0.35f));
        g.fillRect(x, y + 16, w, 1);
    };

    drawSection("FX RACK  (signal flows Slot 1 -> 5, drag headers to reorder)", 20, 6, 1040,
                PicoColors::mint);

    const auto typeNames = FxChain::getTypeNames();
    const int t = getSlotType(selectedSlot);
    drawSection("DETAILS  -  SLOT " + juce::String(selectedSlot + 1) + "  ["
                + typeNames[juce::jlimit(0, typeNames.size() - 1, t)] + "]",
                20, kDetailY, 1040, PicoColors::pink);

    if (t == FxChain::None)
    {
        g.setColour(PicoColors::textDim.withAlpha(0.55f));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText("Select an FX type on this slot to edit its parameters.",
                   20, kDetailY + 30, 500, 16, juce::Justification::centredLeft);
    }

    // スロット間の矢印
    g.setColour(PicoColors::textDim.withAlpha(0.55f));
    g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    for (int i = 0; i < FxChain::kNumSlots - 1; ++i)
    {
        const int x = 20 + kCardW + i * kCardGap;
        g.drawText(">", x, kCardY + kCardH / 2 - 10, kCardGap - kCardW, 20, juce::Justification::centred);
    }
}

void FxPanel::resized()
{
    for (int i = 0; i < FxChain::kNumSlots; ++i)
        cards[(size_t)i]->setBounds(20 + i * kCardGap, kCardY, kCardW, kCardH);

    layoutDetails();
}
