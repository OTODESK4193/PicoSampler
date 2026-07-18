// ==========================================
// File: SlotPanel.cpp
// 8スロット 2x4 カード ＆ パラメータ描画実装
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

        lowNoteKnobs[(size_t)i] = std::make_unique<ValueKnob>();
        lowNoteKnobs[(size_t)i]->setRange(0, 127, 1);
        addAndMakeVisible(lowNoteKnobs[(size_t)i].get());
        attachments[(size_t)i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, "slotLowNote_" + juce::String(i), *lowNoteKnobs[(size_t)i]);

        highNoteKnobs[(size_t)i] = std::make_unique<ValueKnob>();
        highNoteKnobs[(size_t)i]->setRange(0, 127, 1);
        addAndMakeVisible(highNoteKnobs[(size_t)i].get());
        attachments[(size_t)(i + 8)] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, "slotHighNote_" + juce::String(i), *highNoteKnobs[(size_t)i]);
    }

    updateSlotStates();
}

void SlotPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("8-SLOT CONFIGURATION (Drag & Drop samples to any card)", 20, 10, 500, 20, juce::Justification::left);
}

void SlotPanel::resized()
{
    const int cardW = 240;
    const int cardH = 135;
    const int startX = 20;
    const int startY = 35;
    const int gapX = 25;
    const int gapY = 25;

    for (int i = 0; i < 8; ++i)
    {
        const int row = i / 4;
        const int col = i % 4;

        const int x = startX + col * (cardW + gapX);
        const int y = startY + row * (cardH + gapY);

        slotCards[(size_t)i]->setBounds(x, y, cardW, cardH);
        lowNoteKnobs[(size_t)i]->setBounds(x + 130, y + 80, 44, 44);
        highNoteKnobs[(size_t)i]->setBounds(x + 185, y + 80, 44, 44);
    }
}

void SlotPanel::updateSlotStates()
{
    const int activeIdx = (int)vts.getRawParameterValue("activeSlot")->load();

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

        slotCards[(size_t)i]->repaint();
    }
}

std::array<std::pair<int, int>, 8> SlotPanel::getSlotRanges() const
{
    std::array<std::pair<int, int>, 8> res {};
    for (int i = 0; i < 8; ++i)
    {
        res[(size_t)i] = { (int)lowNoteKnobs[(size_t)i]->getValue(), (int)highNoteKnobs[(size_t)i]->getValue() };
    }
    return res;
}

std::array<int, 8> SlotPanel::getRootKeys() const
{
    std::array<int, 8> res {};
    for (int i = 0; i < 8; ++i)
    {
        res[(size_t)i] = samplerEngine.getSlot(i).getMetadata().rootKey;
    }
    return res;
}

std::array<bool, 8> SlotPanel::getSlotReadyStates() const
{
    std::array<bool, 8> res {};
    for (int i = 0; i < 8; ++i)
    {
        res[(size_t)i] = samplerEngine.getSlot(i).isReady();
    }
    return res;
}

// SlotCardComponent 描画
void SlotPanel::SlotCardComponent::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();
    const auto col = PicoColors::getSlotColor(slotIndex);

    g.setColour(isSelected ? col.withAlpha(0.2f) : PicoColors::panel);
    g.fillRoundedRectangle(0.0f, 0.0f, w, h, 6.0f);

    g.setColour(isSelected ? col : PicoColors::knobTrack);
    g.drawRoundedRectangle(0.0f, 0.0f, w, h, 6.0f, isSelected ? 2.0f : 1.0f);

    // Header Color Tag
    g.setColour(col);
    g.fillRoundedRectangle(6.0f, 6.0f, w - 12.0f, 6.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("SLOT " + juce::String(slotIndex + 1), 12, 16, 100, 20, juce::Justification::left);

    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawText(fileName, 12, 40, (int)w - 24, 35, juce::Justification::topLeft, true);

    g.setColour(PicoColors::mint);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("Root: " + juce::MidiMessage::getMidiNoteName(rootKey, true, true, 4), 12, 80, 110, 18, juce::Justification::left);

    g.setColour(PicoColors::textDim);
    g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    g.drawText("Low", 130, 68, 44, 12, juce::Justification::centred);
    g.drawText("High", 185, 68, 44, 12, juce::Justification::centred);
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
