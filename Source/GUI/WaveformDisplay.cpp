// ==========================================
// File: WaveformDisplay.cpp
// WaveformDisplay 実装 (Crossfade フェード領域グラデーション表示 & Reverse描画反転)
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

void WaveformDisplay::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    // 1. 背景パネル
    g.setColour(PicoColors::panel);
    g.fillRoundedRectangle(0.0f, 0.0f, w, h, 6.0f);
    g.setColour(PicoColors::knobTrack);
    g.drawRoundedRectangle(0.0f, 0.0f, w, h, 6.0f, 1.0f);

    // 2. 解析中演出
    if (currentSlot && currentSlot->isAnalyzing())
    {
        g.setColour(PicoColors::mint);
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));

        juce::String dots;
        int numDots = (int)(animPhase * 4.0f) % 4;
        for (int d = 0; d < numDots; ++d) dots += ".";

        g.drawText("Analyzing Audio & Extracting Anchors" + dots,
                   0, 0, (int)w, (int)h, juce::Justification::centred, true);

        const float barW = 300.0f;
        const float barH = 4.0f;
        const float barX = (w - barW) * 0.5f;
        const float barY = (h * 0.5f) + 20.0f;

        g.setColour(PicoColors::knobTrack);
        g.fillRoundedRectangle(barX, barY, barW, barH, 2.0f);

        const float pX = std::fmod(animPhase * 250.0f, barW);
        g.setColour(PicoColors::pink);
        g.fillRoundedRectangle(barX + pX, barY, 40.0f, barH, 2.0f);
        return;
    }

    // 3. 空スロット状態
    if (!currentSlot || !currentSlot->isReady())
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText("Drag & Drop Sample File Here (Slot " + juce::String(activeSlot + 1) + ")",
                   0, 0, (int)w, (int)h, juce::Justification::centred, true);
        return;
    }

    const auto& buffer = currentSlot->getOriginalBuffer();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    const float* samples = buffer.getReadPointer(0);
    if (!samples) return;

    const juce::String s = juce::String(activeSlot);
    auto getParamFloat = [this](const juce::String& name, float def) {
        if (!vts) return def;
        if (auto* p = vts->getRawParameterValue(name)) return p->load();
        return def;
    };

    bool isReverse = getParamFloat("isReverse_" + s, 0.0f) > 0.5f;

    const float stepX = 4.0f;
    const int numCols = (int)(w / stepX);
    const int samplesPerCol = juce::jmax(1, numSamples / numCols);

    for (int col = 0; col < numCols; ++col)
    {
        const int actualCol = isReverse ? (numCols - 1 - col) : col;
        const int startIdx = actualCol * samplesPerCol;
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
        const float yMin = (h * 0.5f) - (minVal * h * 0.45f);
        const float yMax = (h * 0.5f) - (maxVal * h * 0.45f);

        g.setColour(getSpectralColor(rms));
        g.drawLine(x, yMin, x, yMax, 2.0f);
    }

    // 4. マーカー位置計算 (isReverse による描画反転の同期)
    float startRatio = getParamFloat("sampleStart_" + s, 0.0f);
    float endRatio   = getParamFloat("sampleEnd_" + s, 1.0f);
    float loopStart  = getParamFloat("loopStart_" + s, 0.2f);
    float loopEnd    = getParamFloat("loopEnd_" + s, 0.7f);
    float crossfade  = getParamFloat("crossfade_" + s, 0.05f);
    bool isLooping   = getParamFloat("isLooping_" + s, 0.0f) > 0.5f;

    float drawStartR = isReverse ? (1.0f - endRatio)   : startRatio;
    float drawEndR   = isReverse ? (1.0f - startRatio) : endRatio;
    float drawLpSr   = isReverse ? (1.0f - loopEnd)    : loopStart;
    float drawLpEr   = isReverse ? (1.0f - loopStart)  : loopEnd;

    const float sX = drawStartR * w;
    const float eX = drawEndR * w;

    g.setColour(juce::Colours::yellow);
    g.drawLine(sX, 0.0f, sX, h, 2.0f);

    juce::Path triS;
    triS.addTriangle(sX, 0.0f, sX + 6.0f, 0.0f, sX, 8.0f);
    g.fillPath(triS);

    g.setColour(juce::Colours::orange);
    g.drawLine(eX, 0.0f, eX, h, 2.0f);

    juce::Path triE;
    triE.addTriangle(eX, 0.0f, eX - 6.0f, 0.0f, eX, 8.0f);
    g.fillPath(triE);

    if (isLooping)
    {
        const float lpMarginY = h * 0.175f;
        const float lpH       = h * 0.65f;

        const float lsX = drawLpSr * w;
        const float leX = drawLpEr * w;
        const float lpLenR = drawLpEr - drawLpSr;
        const float xfW = (crossfade * lpLenR) * w;
        const float xfX = isReverse ? lsX : (leX - xfW);

        // X-Fade フェードオーバーラップ領域をグラデーション描画
        juce::ColourGradient xfGrad(PicoColors::mint.withAlpha(0.0f), xfX, 0.0f, PicoColors::mint.withAlpha(0.35f), xfX + xfW, 0.0f, false);
        g.setGradientFill(xfGrad);
        g.fillRect(xfX, lpMarginY, std::max(1.0f, xfW), lpH);

        g.setColour(PicoColors::mint);
        g.drawLine(lsX, lpMarginY, lsX, lpMarginY + lpH, 2.0f);
        g.drawLine(leX, lpMarginY, leX, lpMarginY + lpH, 2.0f);

        g.fillEllipse(lsX - 3.5f, lpMarginY - 3.0f, 7.0f, 7.0f);
        g.fillEllipse(leX - 3.5f, lpMarginY - 3.0f, 7.0f, 7.0f);
    }
}

float WaveformDisplay::findZeroCrossingRatio(float targetRatio) const noexcept
{
    if (!currentSlot || !currentSlot->isReady()) return targetRatio;

    const auto& buffer = currentSlot->getOriginalBuffer();
    const int numSamples = buffer.getNumSamples();
    if (numSamples < 2) return targetRatio;

    const float* samples = buffer.getReadPointer(0);
    if (!samples) return targetRatio;

    const int targetIdx = juce::jlimit(0, numSamples - 1, (int)(targetRatio * (float)numSamples));
    const int searchRange = std::min(2048, numSamples / 10);

    int bestIdx = targetIdx;

    for (int d = 0; d < searchRange; ++d)
    {
        int left = targetIdx - d;
        int right = targetIdx + d;

        if (left >= 0 && left < numSamples - 1)
        {
            if (samples[left] * samples[left + 1] <= 0.0f || std::abs(samples[left]) < 1.0e-4f)
            {
                bestIdx = left;
                break;
            }
        }
        if (right >= 0 && right < numSamples - 1)
        {
            if (samples[right] * samples[right + 1] <= 0.0f || std::abs(samples[right]) < 1.0e-4f)
            {
                bestIdx = right;
                break;
            }
        }
    }

    return (float)bestIdx / (float)numSamples;
}

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
    if (!currentSlot || !currentSlot->isReady()) return;

    const float w = (float)getWidth();
    const float mouseX = (float)e.x;
    const juce::String s = juce::String(activeSlot);

    auto getParamFloat = [this](const juce::String& name, float def) {
        if (!vts) return def;
        if (auto* p = vts->getRawParameterValue(name)) return p->load();
        return def;
    };

    const bool isReverse   = getParamFloat("isReverse_" + s, 0.0f) > 0.5f;
    const float startRatio = getParamFloat("sampleStart_" + s, 0.0f);
    const float endRatio   = getParamFloat("sampleEnd_" + s, 1.0f);
    const float loopStart  = getParamFloat("loopStart_" + s, 0.2f);
    const float loopEnd    = getParamFloat("loopEnd_" + s, 0.7f);
    const bool isLooping   = getParamFloat("isLooping_" + s, 0.0f) > 0.5f;

    const float drawStartR = isReverse ? (1.0f - endRatio)   : startRatio;
    const float drawEndR   = isReverse ? (1.0f - startRatio) : endRatio;
    const float drawLpSr   = isReverse ? (1.0f - loopEnd)    : loopStart;
    const float drawLpEr   = isReverse ? (1.0f - loopStart)  : loopEnd;

    const float sX  = drawStartR * w;
    const float eX  = drawEndR * w;
    const float lsX = drawLpSr * w;
    const float leX = drawLpEr * w;

    if (isLooping && std::abs(mouseX - lsX) < 8.0f) activeDrag = isReverse ? DragTarget::LoopEnd : DragTarget::LoopStart;
    else if (isLooping && std::abs(mouseX - leX) < 8.0f) activeDrag = isReverse ? DragTarget::LoopStart : DragTarget::LoopEnd;
    else if (std::abs(mouseX - sX) < 10.0f) activeDrag = isReverse ? DragTarget::SampleEnd : DragTarget::SampleStart;
    else if (std::abs(mouseX - eX) < 10.0f) activeDrag = isReverse ? DragTarget::SampleStart : DragTarget::SampleEnd;
    else activeDrag = DragTarget::None;
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (activeDrag == DragTarget::None || !currentSlot || !vts) return;

    const float w = (float)getWidth();
    float normX = juce::jlimit(0.0f, 1.0f, (float)e.x / w);
    const juce::String s = juce::String(activeSlot);

    auto getParamFloat = [this](const juce::String& name, float def) {
        if (!vts) return def;
        if (auto* p = vts->getRawParameterValue(name)) return p->load();
        return def;
    };

    const bool isSnap = getParamFloat("isSnap_" + s, 0.0f) > 0.5f;
    if (isSnap) normX = findZeroCrossingRatio(normX);

    const float startVal = getParamFloat("sampleStart_" + s, 0.0f);
    const float endVal   = getParamFloat("sampleEnd_" + s, 1.0f);
    const float lStart   = getParamFloat("loopStart_" + s, 0.2f);
    const float lEnd     = getParamFloat("loopEnd_" + s, 0.7f);

    if (activeDrag == DragTarget::SampleStart)
    {
        const float val = std::min(normX, endVal - 0.01f);
        if (auto* p = vts->getParameter("sampleStart_" + s)) p->setValueNotifyingHost(val);
    }
    else if (activeDrag == DragTarget::SampleEnd)
    {
        const float val = std::max(normX, startVal + 0.01f);
        if (auto* p = vts->getParameter("sampleEnd_" + s)) p->setValueNotifyingHost(val);
    }
    else if (activeDrag == DragTarget::LoopStart)
    {
        const float val = std::min(normX, lEnd - 0.01f);
        if (auto* p = vts->getParameter("loopStart_" + s)) p->setValueNotifyingHost(val);
    }
    else if (activeDrag == DragTarget::LoopEnd)
    {
        const float val = std::max(normX, lStart + 0.01f);
        if (auto* p = vts->getParameter("loopEnd_" + s)) p->setValueNotifyingHost(val);
    }
    repaint();
}

void WaveformDisplay::mouseUp(const juce::MouseEvent&)
{
    activeDrag = DragTarget::None;
}

void WaveformDisplay::timerCallback()
{
    animPhase += 0.03f;
    repaint();
}

juce::Colour WaveformDisplay::getSpectralColor(float brightness) const noexcept
{
    const float b = juce::jlimit(0.0f, 1.0f, brightness * 3.0f);
    return PicoColors::babyBlue.interpolatedWith(PicoColors::pink, b);
}
