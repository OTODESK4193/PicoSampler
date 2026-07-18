// ==========================================
// File: FxPanel.cpp
// FxPanel 実装
// ==========================================
#include "FxPanel.h"
#include "../DSP/FxChain.h"

FxPanel::FxPanel(juce::AudioProcessorValueTreeState& apvts) : vts(apvts)
{
    const auto typeNames = FxChain::getTypeNames();

    for (int i = 0; i < 5; ++i)
    {
        fxCards[(size_t)i] = std::make_unique<FxCardComponent>();
        fxCards[(size_t)i]->slotIndex = i;
        fxCards[(size_t)i]->comboType.addItemList(typeNames, 1);

        const juce::String pPrefix = "fx" + juce::String(i + 1);
        comboAttachments.push_back(std::make_unique<ComboAttach>(vts, pPrefix + "Type", fxCards[(size_t)i]->comboType));
        attachments.push_back(std::make_unique<Attachment>(vts, pPrefix + "Amount", fxCards[(size_t)i]->knobAmount));

        addAndMakeVisible(fxCards[(size_t)i].get());
    }
}

void FxPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("EFFECTS CHAIN (Drag to reorder)", 30, 20, 300, 24, juce::Justification::left);
}

void FxPanel::resized()
{
    const int cardW = 180;
    const int cardH = 200;
    const int startX = 30;
    const int spacing = 20;

    for (int i = 0; i < 5; ++i)
    {
        const int x = startX + i * (cardW + spacing);
        fxCards[(size_t)i]->setBounds(x, 60, cardW, cardH);
        fxCards[(size_t)i]->comboType.setBounds(10, 35, cardW - 20, 26);
        fxCards[(size_t)i]->knobAmount.setBounds((cardW - 65) / 2, 80, 65, 65);
    }
}

void FxPanel::FxCardComponent::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    g.setColour(PicoColors::panel);
    g.fillRoundedRectangle(0.0f, 0.0f, w, h, 6.0f);
    g.setColour(PicoColors::knobTrack);
    g.drawRoundedRectangle(0.0f, 0.0f, w, h, 6.0f, 1.0f);

    g.setColour(PicoColors::mint);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("FX " + juce::String(slotIndex + 1), 10, 8, (int)w - 20, 20, juce::Justification::left);
}

void FxPanel::FxCardComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.originalComponent == this)
    {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            container->startDragging(juce::String(slotIndex), this);
        }
    }
}

bool FxPanel::FxCardComponent::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&)
{
    return true;
}

void FxPanel::FxCardComponent::itemDropped(const juce::DragAndDropTarget::SourceDetails& details)
{
    const int srcIdx = details.description.toString().getIntValue();
    if (srcIdx != slotIndex && onReorder)
    {
        onReorder(srcIdx, slotIndex);
    }
}
