// ==========================================
// File: WaveformDisplay.cpp
// WaveformDisplay 実装 (Triangle 描画修正)
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

    if (!currentSlot || !currentSlot->isReady())
    {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(juce::FontOptions(14.0f, juce::Font::italic));
        g.drawText("Drag & Drop Sample File Here", getLocalBounds(), juce::Justification::centred, true);
        return;
    }

    const auto& buffer = currentSlot->getOriginalBuffer();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    const auto& meta = currentSlot->getMetadata();
    const float* samples = buffer.getReadPointer(0);

    // 2. ドットマトリクス波形描画
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

    // 3. サンプルStart / End マーカー描画 (黄色/オレンジ)
    const float sX = meta.sampleStartRatio * w;
    const float eX = meta.sampleEndRatio * w;

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

    // 4. ループStart / End & クロスフェード描画 (緑/ミント)
    if (meta.isLooping)
    {
        const float lsX = meta.loopStartRatio * w;
        const float leX = (meta.loopStartRatio + meta.loopLengthRatio) * w;
        const float xfX = (meta.loopStartRatio + meta.loopLengthRatio - meta.crossfadeRatio) * w;

        g.setColour(PicoColors::mint.withAlpha(0.15f));
        g.fillRect(xfX, 0.0f, leX - xfX, h);

        g.setColour(PicoColors::mint);
        g.drawLine(lsX, 0.0f, lsX, h, 1.5f);
        g.drawLine(leX, 0.0f, leX, h, 1.5f);
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
    if (!currentSlot || !currentSlot->isReady()) return;

    const float w = (float)getWidth();
    const float mouseX = (float)e.x;
    const auto& meta = currentSlot->getMetadata();

    const float sX = meta.sampleStartRatio * w;
    const float eX = meta.sampleEndRatio * w;

    if (std::abs(mouseX - sX) < 10.0f) activeDrag = DragTarget::SampleStart;
    else if (std::abs(mouseX - eX) < 10.0f) activeDrag = DragTarget::SampleEnd;
    else activeDrag = DragTarget::None;
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (activeDrag == DragTarget::None || !currentSlot) return;

    const float w = (float)getWidth();
    const float normX = juce::jlimit(0.0f, 1.0f, (float)e.x / w);
    auto& meta = currentSlot->getMetadata();

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
