// ==========================================
// File: SlotPanel.cpp
// SlotPanel 88鍵盤グラフィック強化版 実装
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

    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("SLOT CARDS (Drag & Drop samples)", 20, 10, 300, 20, juce::Justification::left);
    g.drawText("KEYBOARD RANGE MAP (88-Keys Graphic Overlay)", 20, 250, 400, 20, juce::Justification::left);
}

void SlotPanel::resized()
{
    const int cardW = 122;
    const int cardH = 150;
    const int startX = 20;
    const int spacing = 10;

    for (int i = 0; i < 8; ++i)
    {
        const int x = startX + i * (cardW + spacing);
        slotCards[(size_t)i]->setBounds(x, 35, cardW, cardH);
        lowNoteKnobs[(size_t)i]->setBounds(x + 8, 192, 48, 48);
        highNoteKnobs[(size_t)i]->setBounds(x + 66, 192, 48, 48);
    }

    keyboardVisualizer.setBounds(startX, 275, 8 * (cardW + spacing) - spacing, 360);
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
        keyboardVisualizer.rootKeys[(size_t)i] = meta.rootKey;
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

    g.setColour(isSelected ? col.withAlpha(0.2f) : PicoColors::panel);
    g.fillRoundedRectangle(0.0f, 0.0f, w, h, 6.0f);

    g.setColour(isSelected ? col : PicoColors::knobTrack);
    g.drawRoundedRectangle(0.0f, 0.0f, w, h, 6.0f, isSelected ? 2.0f : 1.0f);

    // Header Color Tag
    g.setColour(col);
    g.fillRoundedRectangle(4.0f, 4.0f, w - 8.0f, 8.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("SLOT " + juce::String(slotIndex + 1), 8, 16, (int)w - 16, 20, juce::Justification::left);

    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawText(fileName, 8, 40, (int)w - 16, 38, juce::Justification::topLeft, true);

    g.setColour(PicoColors::mint);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("Root: " + juce::MidiMessage::getMidiNoteName(rootKey, true, true, 4), 8, 85, (int)w - 16, 18, juce::Justification::left);

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
    g.drawText("Low: " + juce::MidiMessage::getMidiNoteName(lowNote, true, true, 4), 8, 105, (int)w - 16, 16, juce::Justification::left);
    g.drawText("High: " + juce::MidiMessage::getMidiNoteName(highNote, true, true, 4), 8, 122, (int)w - 16, 16, juce::Justification::left);
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

// GraphicalKeyboardComponent グラフィック強化版描画
float SlotPanel::GraphicalKeyboardComponent::noteToX(int midiNote) const noexcept
{
    const float w = (float)getWidth();
    const float noteNorm = juce::jlimit(21.0f, 108.0f, (float)midiNote);
    return ((noteNorm - 21.0f) / 87.0f) * (w - 20.0f) + 10.0f;
}

int SlotPanel::GraphicalKeyboardComponent::xToNote(float x) const noexcept
{
    const float w = (float)getWidth();
    const float norm = juce::jlimit(0.0f, 1.0f, (x - 10.0f) / (w - 20.0f));
    return (int)std::round(norm * 87.0f + 21.0f);
}

void SlotPanel::GraphicalKeyboardComponent::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    // 1. 背景パネル
    g.setColour(PicoColors::panel);
    g.fillRoundedRectangle(0.0f, 0.0f, w, h, 8.0f);
    g.setColour(PicoColors::knobTrack);
    g.drawRoundedRectangle(0.0f, 0.0f, w, h, 8.0f, 1.0f);

    // 2. 8スロット用グラフィカルレーン (上部 8レーン × 22px = 176px)
    const float laneH = 22.0f;
    for (int s = 0; s < 8; ++s)
    {
        const float laneY = 10.0f + (float)s * (laneH + 2.0f);
        const auto col = PicoColors::getSlotColor(s);
        const bool isAct = (s == activeSlot);

        // レーン背景
        g.setColour(PicoColors::track);
        g.fillRoundedRectangle(10.0f, laneY, w - 20.0f, laneH, 3.0f);

        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("S" + juce::String(s + 1), 14, (int)laneY, 20, (int)laneH, juce::Justification::centredLeft);

        if (slotEnabled[(size_t)s])
        {
            const float x1 = noteToX(slotRanges[(size_t)s].first);
            const float x2 = noteToX(slotRanges[(size_t)s].second);
            const float bandW = std::max(6.0f, x2 - x1);

            // 音域帯描画
            g.setColour(col.withAlpha(isAct ? 0.85f : 0.45f));
            g.fillRoundedRectangle(x1, laneY + 2.0f, bandW, laneH - 4.0f, 3.0f);

            if (isAct)
            {
                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(x1, laneY + 2.0f, bandW, laneH - 4.0f, 3.0f, 1.5f);
            }

            // ルートキー位置の 'R' 発光マーカー
            const float rx = noteToX(rootKeys[(size_t)s]);
            if (rx >= x1 && rx <= x2)
            {
                g.setColour(juce::Colours::yellow);
                g.fillEllipse(rx - 4.0f, laneY + laneH * 0.5f - 4.0f, 8.0f, 8.0f);
            }
        }
    }

    // 3. 88鍵盤ビジュアル描画 (下部)
    const float kbY = h - 130.0f;
    const float kbH = 120.0f;

    g.setColour(juce::Colours::black);
    g.fillRect(10.0f, kbY, w - 20.0f, kbH);

    // 白鍵描画 (52鍵)
    const float numWhite = 52.0f;
    const float whiteW = (w - 20.0f) / numWhite;

    static const bool isBlackNote[12] = { false, true, false, true, false, false, true, false, true, false, true, false };
    int whiteIdx = 0;

    for (int note = 21; note <= 108; ++note)
    {
        const int noteInOct = note % 12;
        if (!isBlackNote[noteInOct])
        {
            const float x = 10.0f + (float)whiteIdx * whiteW;
            g.setColour(juce::Colours::white.withAlpha(0.92f));
            g.fillRect(x + 1.0f, kbY + 1.0f, whiteW - 2.0f, kbH - 2.0f);

            // オクターブ C 音ラベル
            if (noteInOct == 0)
            {
                const int octaveNum = (note / 12) - 1;
                g.setColour(juce::Colours::darkgrey);
                g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
                g.drawText("C" + juce::String(octaveNum), (int)x, (int)(kbY + kbH - 18.0f), (int)whiteW, 16, juce::Justification::centred);
            }
            whiteIdx++;
        }
    }

    // 黒鍵描画 (白鍵の上にレイヤー重ね)
    whiteIdx = 0;
    for (int note = 21; note <= 108; ++note)
    {
        const int noteInOct = note % 12;
        if (!isBlackNote[noteInOct])
        {
            whiteIdx++;
        }
        else
        {
            const float x = 10.0f + ((float)whiteIdx - 0.35f) * whiteW;
            g.setColour(PicoColors::bgDk);
            g.fillRect(x, kbY, whiteW * 0.7f, kbH * 0.6f);
            g.setColour(juce::Colours::black);
            g.drawRect(x, kbY, whiteW * 0.7f, kbH * 0.6f, 1.0f);
        }
    }
}

void SlotPanel::GraphicalKeyboardComponent::mouseDown(const juce::MouseEvent& e)
{
    const int clickedNote = xToNote((float)e.x);
    draggingSlot = activeSlot;

    const int low = slotRanges[(size_t)activeSlot].first;
    const int high = slotRanges[(size_t)activeSlot].second;

    draggingLowEdge = (std::abs(clickedNote - low) <= std::abs(clickedNote - high));
}

void SlotPanel::GraphicalKeyboardComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingSlot < 0) return;

    const int currentNote = xToNote((float)e.x);
    auto range = slotRanges[(size_t)draggingSlot];

    if (draggingLowEdge) range.first = std::min(currentNote, range.second);
    else range.second = std::max(currentNote, range.first);

    if (onRangeChanged) onRangeChanged(draggingSlot, range.first, range.second);
    repaint();
}

void SlotPanel::GraphicalKeyboardComponent::mouseUp(const juce::MouseEvent&)
{
    draggingSlot = -1;
}
