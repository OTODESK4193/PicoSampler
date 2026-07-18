// ==========================================
// File: WaveformDisplay.cpp
// WaveformDisplay 実装 (APVTSノブ双方向連動対応)
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

    // 2. 波形描画
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
        const float yMin = (h * 0.5f) - (minVal * h * 0.45f);
        const float yMax = (h * 0.5f) - (maxVal * h * 0.45f);

        g.setColour(getSpectralColor(rms));
        g.drawLine(x, yMin, x, yMax, 2.0f);
    }

    // 3. APVTSからのリアルタイムパラメーター描画
    const juce::String s = juce::String(activeSlot);
    float startRatio = 0.0f;
    float endRatio   = 1.0f;
    float loopStart  = 0.2f;
    float loopLen    = 0.5f;
    float crossfade  = 0.05f;
    bool isLooping   = false;

    if (vts)
    {
        startRatio = vts->getRawParameterValue("sampleStart_" + s)->load();
        endRatio   = vts->getRawParameterValue("sampleEnd_" + s)->load();
        loopStart  = vts->getRawParameterValue("loopStart_" + s)->load();
        loopLen    = vts->getRawParameterValue("loopLength_" + s)->load();
        crossfade  = vts->getRawParameterValue("crossfade_" + s)->load();
        isLooping  = vts->getRawParameterValue("isLooping_" + s)->load() > 0.5f;
    }
    else
    {
        const auto& meta = currentSlot->getMetadata();
        startRatio = meta.sampleStartRatio;
        endRatio   = meta.sampleEndRatio;
        loopStart  = meta.loopStartRatio;
        loopLen    = meta.loopLengthRatio;
        crossfade  = meta.crossfadeRatio;
        isLooping  = meta.isLooping;
    }

    // サンプルStart / End マーカー描画
    const float sX = startRatio * w;
    const float eX = endRatio * w;

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

    // ループStart / End & クロスフェード
    if (isLooping)
    {
        const float lsX = loopStart * w;
        const float leX = (loopStart + loopLen) * w;
        const float xfX = (loopStart + loopLen - crossfade) * w;

        g.setColour(PicoColors::mint.withAlpha(0.15f));
        g.fillRect(xfX, 0.0f, leX - xfX, h);

        g.setColour(PicoColors::mint);
        g.drawLine(lsX, 0.0f, lsX, h, 1.5f);
        g.drawLine(leX, 0.0f, leX, h, 1.5f);
    }
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

    float startRatio = 0.0f, endRatio = 1.0f;
    if (vts)
    {
        startRatio = vts->getRawParameterValue("sampleStart_" + s)->load();
        endRatio   = vts->getRawParameterValue("sampleEnd_" + s)->load();
    }

    const float sX = startRatio * w;
    const float eX = endRatio * w;

    if (std::abs(mouseX - sX) < 10.0f) activeDrag = DragTarget::SampleStart;
    else if (std::abs(mouseX - eX) < 10.0f) activeDrag = DragTarget::SampleEnd;
    else activeDrag = DragTarget::None;
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (activeDrag == DragTarget::None || !currentSlot || !vts) return;

    const float w = (float)getWidth();
    const float normX = juce::jlimit(0.0f, 1.0f, (float)e.x / w);
    const juce::String s = juce::String(activeSlot);

    float startVal = vts->getRawParameterValue("sampleStart_" + s)->load();
    float endVal   = vts->getRawParameterValue("sampleEnd_" + s)->load();

    if (activeDrag == DragTarget::SampleStart)
    {
        const float val = std::min(normX, endVal - 0.01f);
        if (auto* p = vts->getParameter("sampleStart_" + s))
            p->setValueNotifyingHost(val);
    }
    else if (activeDrag == DragTarget::SampleEnd)
    {
        const float val = std::max(normX, startVal + 0.01f);
        if (auto* p = vts->getParameter("sampleEnd_" + s))
            p->setValueNotifyingHost(val);
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
