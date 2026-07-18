// ==========================================
// File: SlotPanel.cpp
// SlotPanel 描画＆インタラクション実装
// ==========================================
#include "SlotPanel.h"

SlotPanel::SlotPanel(juce::AudioProcessorValueTreeState& apvts, SamplerEngine& engine)
    : vts(apvts), samplerEngine(engine)
{
    for (int i = 0; i < 8; ++i)
    {
        slotCards[(size_t)i] = std::make_unique<SlotCardComponent>();
        slotCards[(size_t)i]->slotIndex = i;
        slotCards[(size_t)i]->onSelect = [this](int idx) {
            if (auto* p = vts.getParameter("activeSlot"))
                p->setValueNotifyingHost((float)idx / 7.0f);
            updateSlotStates();
        };
        slotCards[(size_t)i]->onFileDropped = [this](int idx, const juce::File& file) {
            samplerEngine.getSlot(idx).loadFromFile(file);
            updateSlotStates();
        };
        addAndMakeVisible(slotCards[(size_t)i].get());

        // Low Note Knob
        lowNoteKnobs[(size_t)i] = std::make_unique<ValueKnob>();
        lowNoteKnobs[(size_t)i]->setRange(0, 127, 1);
        addAndMakeVisible(lowNoteKnobs[(size_t)i].get());
        attachments[(size_t)i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, "slotLowNote_" + juce::String(i), *lowNoteKnobs[(size_t)i]);

        // High Note Knob
        highNoteKnobs[(size_t)i] = std::make_unique<ValueKnob>();
        highNoteKnobs[(size_t)i]->setRange(0, 127, 1);
        addAndMakeVisible(highNoteKnobs[(size_t)i].get());
        attachments[(size_t)(i + 8)] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, "slotHighNote_" + juce::String(i), *highNoteKnobs[(size_t)i]);
    }

    keyboardVisualizer.onRangeChanged = [this](int slotIdx, int low, int high) {
        if (lowNoteKnobs[(size_t)slotIdx]) lowNoteKnobs[(size_t)slotIdx]->setValue(low);
        if (highNoteKnobs[(size_t)slotIdx]) highNoteKnobs[(size_t)slotIdx]->setValue(high);
        updateSlotStates();
    };

    addAndMakeVisible(keyboardVisualizer);
    updateSlotStates();
}

void SlotPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);
}

void SlotPanel::resized()
{
    const int cardW = 120;
    const int cardH = 140;
    const int startX = 20;
    const int spacing = 10;

    for (int i = 0; i < 8; ++i)
    {
        const int x = startX + i * (cardW + spacing);
        slotCards[(size_t)i]->setBounds(x, 20, cardW, cardH);
        lowNoteKnobs[(size_t)i]->setBounds(x + 10, 170, 45, 45);
        highNoteKnobs[(size_t)i]->setBounds(x + 65, 170, 45, 45);
    }

    keyboardVisualizer.setBounds(startX, 230, 8 * (cardW + spacing) - spacing, 180);
}

void SlotPanel::updateSlotStates()
{
    const int activeIdx = (int)vts.getRawParameterValue("activeSlot")->load();
    keyboardVisualizer.activeSlot = activeIdx;

    for (int i = 0; i < 8; ++i)
    {
        const auto& slot = samplerEngine.getSlot(i);
        const auto& meta = slot.getMetadata();

        slotCards[(size_t)i]->isSelected = (i == activeIdx);
        slotCards[(size_t)i]->fileName = slot.isReady() ? meta.fileName : "Empty";
        slotCards[(size_t)i]->rootKey = meta.rootKey;

        const int low = (int)lowNoteKnobs[(size_t)i]->getValue();
        const int high = (int)highNoteKnobs[(size_t)i]->getValue();
        slotCards[(size_t)i]->lowNote = low;
        slotCards[(size_t)i]->highNote = high;

        keyboardVisualizer.slotRanges[(size_t)i] = { low, high };
        keyboardVisualizer.slotEnabled[(size_t)i] = slot.isReady();

        slotCards[(size_t)i]->repaint();
    }
    keyboardVisualizer.repaint();
}

// SlotCardComponent 描画
void SlotPanel::SlotCardComponent::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();
    const auto col = PicoColors::getSlotColor(slotIndex);

    g.setColour(isSelected ? col.withAlpha(0.25f) : PicoColors::panel);
    g.fillRoundedRectangle(0.0f, 0.0f, w, h, 6.0f);

    g.setColour(isSelected ? col : PicoColors::knobTrack);
    g.drawRoundedRectangle(0.0f, 0.0f, w, h, 6.0f, isSelected ? 2.0f : 1.0f);

    // Header Color Tag
    g.setColour(col);
    g.fillRoundedRectangle(4.0f, 4.0f, w - 8.0f, 8.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("SLOT " + juce::String(slotIndex + 1), 6, 16, (int)w - 12, 20, juce::Justification::left);

    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText(fileName, 6, 40, (int)w - 12, 35, juce::Justification::topLeft, true);

    g.setColour(PicoColors::mint);
    g.drawText("Root: " + juce::MidiMessage::getMidiNoteName(rootKey, true, true, 4), 6, 80, (int)w - 12, 18, juce::Justification::left);

    g.setColour(juce::Colours::grey);
    g.drawText("Key: " + juce::String(lowNote) + "-" + juce::String(highNote), 6, 100, (int)w - 12, 18, juce::Justification::left);
}

void SlotPanel::SlotCardComponent::mouseDown(const juce::MouseEvent&)
{
    if (onSelect) onSelect(slotIndex);
}

bool SlotPanel::SlotCardComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    return !files.isEmpty();
}

void SlotPanel::SlotCardComponent::filesDropped(const juce::StringArray& files, int, int)
{
    if (!files.isEmpty() && onFileDropped)
        onFileDropped(slotIndex, juce::File(files[0]));
}

// MiniKeyboardComponent 描画
void SlotPanel::MiniKeyboardComponent::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    g.setColour(PicoColors::panel);
    g.fillRoundedRectangle(0.0f, 0.0f, w, h, 6.0f);

    // 88鍵盤の白鍵/黒鍵簡易描画
    const float keyW = w / 88.0f;
    for (int k = 0; k < 88; ++k)
    {
        const int note = k + 21; // A0 (21) ~ C8 (108)
        const bool isBlack = (note % 12 == 1 || note % 12 == 3 || note % 12 == 6 || note % 12 == 8 || note % 12 == 10);
        const float x = (float)k * keyW;

        g.setColour(isBlack ? juce::Colours::black : juce::Colours::white.withAlpha(0.8f));
        g.fillRect(x, isBlack ? 0.0f : h * 0.5f, keyW - 1.0f, h * 0.5f);
    }

    // 8スロットの音域帯オーバーレイ描画
    for (int s = 0; s < 8; ++s)
    {
        if (!slotEnabled[(size_t)s]) continue;

        const int lowK = juce::jlimit(21, 108, slotRanges[(size_t)s].first) - 21;
        const int highK = juce::jlimit(21, 108, slotRanges[(size_t)s].second) - 21;
        const float x1 = (float)lowK * keyW;
        const float x2 = (float)(highK + 1) * keyW;

        const float bandY = (float)s * 10.0f + 4.0f;
        g.setColour(PicoColors::getSlotColor(s).withAlpha(s == activeSlot ? 0.8f : 0.4f));
        g.fillRoundedRectangle(x1, bandY, std::max(4.0f, x2 - x1), 8.0f, 2.0f);
    }
}

void SlotPanel::MiniKeyboardComponent::mouseDown(const juce::MouseEvent& e)
{
    const float w = (float)getWidth();
    const float keyW = w / 88.0f;
    const int clickedNote = juce::jlimit(0, 127, (int)(e.x / keyW) + 21);

    draggingSlot = activeSlot;
    const int low = slotRanges[(size_t)activeSlot].first;
    const int high = slotRanges[(size_t)activeSlot].second;

    draggingLowEdge = (std::abs(clickedNote - low) < std::abs(clickedNote - high));
}

void SlotPanel::MiniKeyboardComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingSlot < 0) return;

    const float w = (float)getWidth();
    const float keyW = w / 88.0f;
    const int currentNote = juce::jlimit(0, 127, (int)(e.x / keyW) + 21);

    auto range = slotRanges[(size_t)draggingSlot];
    if (draggingLowEdge) range.first = std::min(currentNote, range.second);
    else range.second = std::max(currentNote, range.first);

    if (onRangeChanged) onRangeChanged(draggingSlot, range.first, range.second);
    repaint();
}
