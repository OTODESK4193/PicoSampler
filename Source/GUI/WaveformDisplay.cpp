// ==========================================
// File: WaveformDisplay.cpp
// WaveformDisplay 実装 (リアル88鍵盤 ＋ 黒鍵 ＋ C音表記統合)
// ==========================================
#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay()
{
    startTimerHz(30);
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

float WaveformDisplay::noteToX(int midiNote, float w) const noexcept
{
    const float norm = juce::jlimit(21.0f, 108.0f, (float)midiNote);
    return ((norm - 21.0f) / 87.0f) * (w - 20.0f) + 10.0f;
}

int WaveformDisplay::xToNote(float x, float w) const noexcept
{
    const float norm = juce::jlimit(0.0f, 1.0f, (x - 10.0f) / (w - 20.0f));
    return (int)std::round(norm * 87.0f + 21.0f);
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();
    const float kbAreaH = 75.0f;
    const float waveH = h - kbAreaH;
    const float kbY = h - kbAreaH;

    // 1. 背景パネル
    g.setColour(PicoColors::panel);
    g.fillRoundedRectangle(0.0f, 0.0f, w, h, 6.0f);
    g.setColour(PicoColors::knobTrack);
    g.drawRoundedRectangle(0.0f, 0.0f, w, h, 6.0f, 1.0f);

    // --- 2. 波形表示エリア (0 .. waveH) ---
    if (!currentSlot || !currentSlot->isReady())
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText("Drag & Drop Sample File Here (Slot " + juce::String(activeSlot + 1) + ")",
                   0, 0, (int)w, (int)waveH, juce::Justification::centred, true);
    }
    else
    {
        const auto& buffer = currentSlot->getOriginalBuffer();
        const int numSamples = buffer.getNumSamples();
        if (numSamples > 0)
        {
            const auto& meta = currentSlot->getMetadata();
            const float* samples = buffer.getReadPointer(0);

            const float stepX = 4.0f;
            const int numCols = (int)(w / stepX);
            const int samplesPerCol = juce::jmax(1, numSamples / numCols);

            for (int col = 0; col < numCols; ++col)
            {
                const int startIdx = col * samplesPerCol;
                float minVal = 0.0f, maxVal = 0.0f, energy = 0.0f;

                for (int i = 0; i < samplesPerCol && (startIdx + i) < numSamples; ++i)
                {
                    const float v = samples[startIdx + i];
                    minVal = std::min(minVal, v);
                    maxVal = std::max(maxVal, v);
                    energy += v * v;
                }

                const float rms = std::sqrt(energy / (float)samplesPerCol);
                const float x = (float)col * stepX + stepX * 0.5f;
                const float yMin = (waveH * 0.5f) - (minVal * waveH * 0.45f);
                const float yMax = (waveH * 0.5f) - (maxVal * waveH * 0.45f);

                g.setColour(getSpectralColor(rms));
                g.drawLine(x, yMin, x, yMax, 2.0f);
            }

            // サンプルStart / End マーカー描画
            const float sX = meta.sampleStartRatio * w;
            const float eX = meta.sampleEndRatio * w;

            g.setColour(juce::Colours::yellow);
            g.drawLine(sX, 0.0f, sX, waveH, 2.0f);

            juce::Path triS;
            triS.addTriangle(sX, 0.0f, sX + 6.0f, 0.0f, sX, 8.0f);
            g.fillPath(triS);

            g.setColour(juce::Colours::orange);
            g.drawLine(eX, 0.0f, eX, waveH, 2.0f);

            juce::Path triE;
            triE.addTriangle(eX, 0.0f, eX - 6.0f, 0.0f, eX, 8.0f);
            g.fillPath(triE);

            // ループStart / End
            if (meta.isLooping)
            {
                const float lsX = meta.loopStartRatio * w;
                const float leX = (meta.loopStartRatio + meta.loopLengthRatio) * w;
                const float xfX = (meta.loopStartRatio + meta.loopLengthRatio - meta.crossfadeRatio) * w;

                g.setColour(PicoColors::mint.withAlpha(0.15f));
                g.fillRect(xfX, 0.0f, leX - xfX, waveH);

                g.setColour(PicoColors::mint);
                g.drawLine(lsX, 0.0f, lsX, waveH, 1.5f);
                g.drawLine(leX, 0.0f, leX, waveH, 1.5f);
            }
        }
    }

    // --- 3. KeyRangeMap (88鍵音域オーバーレイ) エリア描画 (kbY .. h) ---
    g.setColour(PicoColors::track);
    g.fillRect(10.0f, kbY, w - 20.0f, kbAreaH - 4.0f);

    // 8スロット音域バー描画 (8レーン, 上部 40px)
    const float laneH = 4.0f;
    for (int s = 0; s < 8; ++s)
    {
        const float lY = kbY + 2.0f + (float)s * (laneH + 1.0f);
        const auto col = PicoColors::getSlotColor(s);
        const bool isAct = (s == activeSlot);

        if (slotReady[(size_t)s])
        {
            const float x1 = noteToX(slotRanges[(size_t)s].first, w);
            const float x2 = noteToX(slotRanges[(size_t)s].second, w);
            const float bandW = std::max(4.0f, x2 - x1);

            g.setColour(col.withAlpha(isAct ? 0.95f : 0.45f));
            g.fillRoundedRectangle(x1, lY, bandW, laneH, 1.5f);

            // ルートキー表示 'R' ドット
            const float rx = noteToX(rootKeys[(size_t)s], w);
            if (rx >= x1 && rx <= x2)
            {
                g.setColour(juce::Colours::yellow);
                g.fillEllipse(rx - 2.5f, lY - 1.0f, 5.0f, 5.0f);
            }
        }
    }

    // --- リアリスティック 88鍵盤描画 (下部 32px) ---
    const float keyY = h - 33.0f;
    const float keyH = 30.0f;
    const float whiteW = (w - 20.0f) / 52.0f;
    static const bool isBlackNote[12] = { false, true, false, true, false, false, true, false, true, false, true, false };

    // A: 白鍵 52個描画 ＋ C音（C1〜C8）文字表示
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
                g.drawText("C" + juce::String(octaveNum), (int)x, (int)(keyY + keyH - 12.0f), (int)whiteW, 11, juce::Justification::centred);
            }
            whiteIdx++;
        }
    }

    // B: 黒鍵 36個描画 (白鍵の上にレイヤー重ね)
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

void WaveformDisplay::resized() {}

bool WaveformDisplay::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".aif") ||
            f.endsWithIgnoreCase(".aiff") || f.endsWithIgnoreCase(".mp3") ||
            f.endsWithIgnoreCase(".flac") || f.endsWithIgnoreCase(".ogg"))
            return true;
    }
    return false;
}

void WaveformDisplay::filesDropped(const juce::StringArray& files, int, int)
{
    if (!files.isEmpty() && onFileDropped)
    {
        onFileDropped(juce::File(files[0]));
    }
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& e)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();
    const float mouseX = (float)e.x;
    const float mouseY = (float)e.y;

    if (mouseY > h - 75.0f)
    {
        // KeyRange エリア操作
        activeDrag = DragTarget::KeyRangeLow;
        const int clickedNote = xToNote(mouseX, w);
        const int low = slotRanges[(size_t)activeSlot].first;
        const int high = slotRanges[(size_t)activeSlot].second;

        if (std::abs(clickedNote - low) <= std::abs(clickedNote - high))
            activeDrag = DragTarget::KeyRangeLow;
        else
            activeDrag = DragTarget::KeyRangeHigh;

        return;
    }

    if (!currentSlot || !currentSlot->isReady()) return;

    const auto& meta = currentSlot->getMetadata();
    const float sX = meta.sampleStartRatio * w;
    const float eX = meta.sampleEndRatio * w;

    if (std::abs(mouseX - sX) < 10.0f) activeDrag = DragTarget::SampleStart;
    else if (std::abs(mouseX - eX) < 10.0f) activeDrag = DragTarget::SampleEnd;
    else activeDrag = DragTarget::None;
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
    const float w = (float)getWidth();
    if (activeDrag == DragTarget::None) return;

    if (activeDrag == DragTarget::KeyRangeLow || activeDrag == DragTarget::KeyRangeHigh)
    {
        const int currentNote = xToNote((float)e.x, w);
        int low = slotRanges[(size_t)activeSlot].first;
        int high = slotRanges[(size_t)activeSlot].second;

        if (activeDrag == DragTarget::KeyRangeLow) low = std::min(currentNote, high);
        else high = std::max(currentNote, low);

        if (onKeyRangeChanged) onKeyRangeChanged(activeSlot, low, high);
        repaint();
        return;
    }

    if (!currentSlot) return;
    const float normX = juce::jlimit(0.0f, 1.0f, (float)e.x / w);
    auto& meta = const_cast<SampleSlot*>(currentSlot)->getMetadata();

    if (activeDrag == DragTarget::SampleStart)
    {
        meta.sampleStartRatio = std::min(normX, meta.sampleEndRatio - 0.01f);
        if (onSampleRangeChanged) onSampleRangeChanged(meta.sampleStartRatio, meta.sampleEndRatio);
    }
    else if (activeDrag == DragTarget::SampleEnd)
    {
        meta.sampleEndRatio = std::max(normX, meta.sampleStartRatio + 0.01f);
        if (onSampleRangeChanged) onSampleRangeChanged(meta.sampleStartRatio, meta.sampleEndRatio);
    }
    repaint();
}

void WaveformDisplay::mouseUp(const juce::MouseEvent&)
{
    activeDrag = DragTarget::None;
}

void WaveformDisplay::timerCallback()
{
    repaint();
}

juce::Colour WaveformDisplay::getSpectralColor(float brightness) const noexcept
{
    const float b = juce::jlimit(0.0f, 1.0f, brightness * 3.0f);
    return PicoColors::babyBlue.interpolatedWith(PicoColors::pink, b);
}
