// ==========================================
// File: KeyRangeMapComponent.cpp
// 8スロット倍幅音域レーン ＆ リアル88鍵盤描画実装
// ==========================================
#include "KeyRangeMapComponent.h"

float KeyRangeMapComponent::noteToX(int midiNote, float w) const noexcept
{
    const float norm = juce::jlimit(21.0f, 108.0f, (float)midiNote);
    return ((norm - 21.0f) / 87.0f) * (w - 20.0f) + 10.0f;
}

int KeyRangeMapComponent::xToNote(float x, float w) const noexcept
{
    const float norm = juce::jlimit(0.0f, 1.0f, (x - 10.0f) / (w - 20.0f));
    return (int)std::round(norm * 87.0f + 21.0f);
}

void KeyRangeMapComponent::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    // 背景パネル
    g.setColour(PicoColors::panel);
    g.fillRoundedRectangle(0.0f, 0.0f, w, h, 6.0f);
    g.setColour(PicoColors::knobTrack);
    g.drawRoundedRectangle(0.0f, 0.0f, w, h, 6.0f, 1.0f);

    // --- 1. 8スロット 音域レーン (上部 85px, 各レーン高さ 9.5px) ---
    const float laneH = 9.5f;
    const float laneStartY = 4.0f;

    for (int s = 0; s < 8; ++s)
    {
        const float lY = laneStartY + (float)s * (laneH + 1.0f);
        const auto col = PicoColors::getSlotColor(s);
        const bool isAct = (s == activeSlot);

        if (slotReady[(size_t)s])
        {
            const float x1 = noteToX(slotRanges[(size_t)s].first, w);
            const float x2 = noteToX(slotRanges[(size_t)s].second, w);
            const float bandW = std::max(6.0f, x2 - x1);

            // 太い音域帯描画
            g.setColour(col.withAlpha(isAct ? 0.95f : 0.45f));
            g.fillRoundedRectangle(x1, lY, bandW, laneH, 2.5f);

            if (isAct)
            {
                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(x1, lY, bandW, laneH, 2.5f, 1.5f);
            }

            // ルートキー表示 'R' 発光イエロードット
            const float rx = noteToX(rootKeys[(size_t)s], w);
            if (rx >= x1 && rx <= x2)
            {
                g.setColour(juce::Colours::yellow);
                g.fillEllipse(rx - 3.5f, lY + 1.0f, 7.0f, 7.0f);
            }
        }
    }

    // --- 2. リアリスティック 88鍵盤描画 (下部 32px) ---
    const float keyY = h - 33.0f;
    const float keyH = 30.0f;
    const float whiteW = (w - 20.0f) / 52.0f;
    static const bool isBlackNote[12] = { false, true, false, true, false, false, true, false, true, false, true, false };

    // A: 白鍵 52個描画 ＋ C音（C1〜C8）表記
    int whiteIdx = 0;
    for (int note = 21; note <= 108; ++note)
    {
        const int noteInOct = note % 12;
        if (!isBlackNote[noteInOct])
        {
            const float x = 10.0f + (float)whiteIdx * whiteW;
            g.setColour(juce::Colours::white.withAlpha(0.92f));
            g.fillRoundedRectangle(x + 0.5f, keyY, whiteW - 1.0f, keyH, 1.5f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(x + 0.5f, keyY, whiteW - 1.0f, keyH, 1.5f, 0.5f);

            // C音 (Note 24, 36, 48, 60, 72, 84, 96, 108) 表記
            if (noteInOct == 0)
            {
                const int octaveNum = (note / 12) - 1;
                g.setColour(juce::Colours::black);
                g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
                g.drawText("C" + juce::String(octaveNum), (int)x, (int)(keyY + keyH - 11.0f), (int)whiteW, 10, juce::Justification::centred);
            }
            whiteIdx++;
        }
    }

    // B: 黒鍵 36個描画
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
            g.setColour(juce::Colours::black);
            g.fillRoundedRectangle(x, keyY, whiteW * 0.68f, keyH * 0.62f, 1.0f);
            g.setColour(PicoColors::knobTrack);
            g.drawRoundedRectangle(x, keyY, whiteW * 0.68f, keyH * 0.62f, 1.0f, 0.5f);
        }
    }
}

void KeyRangeMapComponent::mouseDown(const juce::MouseEvent& e)
{
    const float w = (float)getWidth();
    const int clickedNote = xToNote((float)e.x, w);
    const int low = slotRanges[(size_t)activeSlot].first;
    const int high = slotRanges[(size_t)activeSlot].second;

    if (std::abs(clickedNote - low) <= std::abs(clickedNote - high))
        activeDrag = DragTarget::LowNote;
    else
        activeDrag = DragTarget::HighNote;
}

void KeyRangeMapComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (activeDrag == DragTarget::None) return;

    const float w = (float)getWidth();
    const int currentNote = xToNote((float)e.x, w);
    int low = slotRanges[(size_t)activeSlot].first;
    int high = slotRanges[(size_t)activeSlot].second;

    if (activeDrag == DragTarget::LowNote) low = std::min(currentNote, high);
    else high = std::max(currentNote, low);

    if (onKeyRangeChanged) onKeyRangeChanged(activeSlot, low, high);
    repaint();
}

void KeyRangeMapComponent::mouseUp(const juce::MouseEvent&)
{
    activeDrag = DragTarget::None;
}
