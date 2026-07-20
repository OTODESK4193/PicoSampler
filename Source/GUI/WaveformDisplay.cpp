// ==========================================
// File: WaveformDisplay.cpp
// WaveformDisplay 実装 (Reverseオン時の波形反転描画 & 直感的一貫マーカー操作)
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

    const int viewLenSamples = std::max(2, (int)((float)numSamples / zoomLevel));
    const int viewStartSample = (int)(viewStartRatio * (float)numSamples);

    const float stepX = 4.0f;
    const int numCols = (int)(w / stepX);
    const int samplesPerCol = juce::jmax(1, viewLenSamples / numCols);

    // 波形描画 (Reverse ON 時は画面上の見た目を反転)
    for (int col = 0; col < numCols; ++col)
    {
        const int actualCol = isReverse ? (numCols - 1 - col) : col;
        const int startIdx = viewStartSample + actualCol * samplesPerCol;
        float minVal = 0.0f, maxVal = 0.0f, energy = 0.0f;
        int count = 0;

        for (int i = 0; i < samplesPerCol && (startIdx + i) < numSamples; ++i)
        {
            const float v = samples[startIdx + i];
            minVal = std::min(minVal, v);
            maxVal = std::max(maxVal, v);
            energy += v * v;
            count++;
        }

        const float rms = (count > 0) ? std::sqrt(energy / (float)count) : 0.0f;
        const float x = (float)col * stepX + stepX * 0.5f;
        const float yMin = (h * 0.5f) - (minVal * h * 0.45f);
        const float yMax = (h * 0.5f) - (maxVal * h * 0.45f);

        g.setColour(getSpectralColor(rms));
        g.drawLine(x, yMin, x, yMax, 2.0f);
    }

    // 4. マーカー位置描画
    // 見た目の波形を正として、画面左=Start, 画面右=End で統一
    float startRatio = getParamFloat("sampleStart_" + s, 0.0f);
    float endRatio   = getParamFloat("sampleEnd_" + s, 1.0f);
    float loopStart  = getParamFloat("loopStart_" + s, 0.2f);
    float loopEnd    = getParamFloat("loopEnd_" + s, 0.7f);
    float crossfade  = getParamFloat("crossfade_" + s, 0.05f);
    bool isLooping   = getParamFloat("isLooping_" + s, 0.0f) > 0.5f;

    // ModMatrix の変調値を取得
    const int dstBase = ModMatrix::DstS1Start + activeSlot * 5;
    const float modStart = modMatrix ? modMatrix->get(dstBase + 0) : 0.0f;
    const float modEnd   = modMatrix ? modMatrix->get(dstBase + 1) : 0.0f;
    const float modLStart= modMatrix ? modMatrix->get(dstBase + 2) : 0.0f;
    const float modLEnd  = modMatrix ? modMatrix->get(dstBase + 3) : 0.0f;
    const float modXFade = modMatrix ? modMatrix->get(dstBase + 4) : 0.0f;

    // 変調反映後の動的比率 (リアルタイム位置)
    const float liveStartRatio = juce::jlimit(0.0f, 1.0f, startRatio + modStart);
    const float liveEndRatio   = juce::jlimit(0.01f, 1.0f, endRatio + modEnd);
    const float liveLStart     = juce::jlimit(0.0f, 1.0f, loopStart + modLStart);
    const float liveLEnd       = juce::jlimit(0.01f, 1.0f, loopEnd + modLEnd);
    const float liveXFade      = juce::jlimit(0.0f, 0.5f, crossfade + modXFade);

    auto ratioToX = [&](float ratio) {
        return (ratio - viewStartRatio) * zoomLevel * w;
    };

    const float sX = ratioToX(startRatio);
    const float eX = ratioToX(endRatio);
    const float liveSX = ratioToX(liveStartRatio);
    const float liveEX = ratioToX(liveEndRatio);
    const float liveLSX = ratioToX(liveLStart);
    const float liveLEX = ratioToX(liveLEnd);

    // 静的ベース Start マーカー
    g.setColour(juce::Colours::yellow.withAlpha(0.6f));
    g.drawLine(sX, 0.0f, sX, h, 1.5f);
    juce::Path triS;
    triS.addTriangle(sX, 0.0f, sX + 6.0f, 0.0f, sX, 8.0f);
    g.fillPath(triS);

    // 変調適用リアルタイム Start マーカー (動的白/黄ライン)
    if (std::abs(modStart) > 0.001f)
    {
        g.setColour(juce::Colours::yellow);
        g.drawLine(liveSX, 0.0f, liveSX, h, 2.5f);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillRect(std::min(sX, liveSX), h * 0.5f - 1.0f, std::abs(liveSX - sX), 2.0f);
    }

    // 静的ベース End マーカー
    g.setColour(juce::Colours::orange.withAlpha(0.6f));
    g.drawLine(eX, 0.0f, eX, h, 1.5f);
    juce::Path triE;
    triE.addTriangle(eX, 0.0f, eX - 6.0f, 0.0f, eX, 8.0f);
    g.fillPath(triE);

    // 変調適用リアルタイム End マーカー
    if (std::abs(modEnd) > 0.001f)
    {
        g.setColour(juce::Colours::orange);
        g.drawLine(liveEX, 0.0f, liveEX, h, 2.5f);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillRect(std::min(eX, liveEX), h * 0.5f - 1.0f, std::abs(liveEX - eX), 2.0f);
    }

    if (isLooping)
    {
        const float lpMarginY = h * 0.175f;
        const float lpH       = h * 0.65f;

        const float lsX = liveLSX;
        const float leX = liveLEX;
        const float lpLenR = liveLEnd - liveLStart;
        const float xfW = (liveXFade * lpLenR) * zoomLevel * w;
        const float xfX = leX - xfW;

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

    // 5. ズーム時のスクロールバー描画
    if (zoomLevel > 1.001f)
    {
        const float sbHeight = 12.0f;
        const float sbY = h - sbHeight;
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRect(0.0f, sbY, w, sbHeight);
        
        const float thumbW = std::max(20.0f, w / zoomLevel);
        const float thumbX = viewStartRatio * w; 
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillRoundedRectangle(thumbX, sbY + 2.0f, thumbW, sbHeight - 4.0f, 4.0f);
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
    
    // Zoom in = smaller search range (disables snapping visually when zoomed heavily for sample-accurate edits)
    const int searchRange = std::min(2048, (int)(numSamples / (10.0f * zoomLevel)));

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
    if (e.mods.isPopupMenu() || e.mods.isRightButtonDown())
    {
        if (zoomLevel > 1.001f)
        {
            zoomLevel = 1.0f;
            viewStartRatio = 0.0f;
            repaint();
            return;
        }

        if (currentSlot && currentSlot->isReady())
        {
            const int slotNum = activeSlot + 1;
            juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::WarningIcon,
                "Delete Sample",
                "Are you sure you want to delete the loaded sample in Slot " + juce::String(slotNum) + "?",
                "Yes (Delete)", "No (Cancel)",
                this,
                juce::ModalCallbackFunction::create([this](int result) {
                    if (result == 1 && onClearSlotRequested)
                    {
                        onClearSlotRequested(activeSlot);
                    }
                })
            );
        }
        return;
    }

    if (!currentSlot || !currentSlot->isReady()) return;

    const float w = (float)getWidth();
    const float mouseX = (float)e.x;
    const juce::String s = juce::String(activeSlot);

    auto getParamFloat = [this](const juce::String& name, float def) {
        if (!vts) return def;
        if (auto* p = vts->getRawParameterValue(name)) return p->load();
        return def;
    };

    const float startRatio = getParamFloat("sampleStart_" + s, 0.0f);
    const float endRatio   = getParamFloat("sampleEnd_" + s, 1.0f);
    const float loopStart  = getParamFloat("loopStart_" + s, 0.2f);
    const float loopEnd    = getParamFloat("loopEnd_" + s, 0.7f);
    const bool isLooping   = getParamFloat("isLooping_" + s, 0.0f) > 0.5f;

    auto ratioToX = [&](float ratio) { return (ratio - viewStartRatio) * zoomLevel * w; };

    const float sX  = ratioToX(startRatio);
    const float eX  = ratioToX(endRatio);
    const float lsX = ratioToX(loopStart);
    const float leX = ratioToX(loopEnd);

    if (zoomLevel > 1.001f && e.y >= getHeight() - 12)
    {
        activeDrag = DragTarget::Scrollbar;
        viewStartRatio = (e.x - (w / zoomLevel) * 0.5f) / w;
        viewStartRatio = juce::jlimit(0.0f, std::max(0.0f, 1.0f - 1.0f / zoomLevel), viewStartRatio);
        repaint();
        return;
    }

    if (isLooping && std::abs(mouseX - lsX) < 8.0f) activeDrag = DragTarget::LoopStart;
    else if (isLooping && std::abs(mouseX - leX) < 8.0f) activeDrag = DragTarget::LoopEnd;
    else if (std::abs(mouseX - sX) < 10.0f) activeDrag = DragTarget::SampleStart;
    else if (std::abs(mouseX - eX) < 10.0f) activeDrag = DragTarget::SampleEnd;
    else activeDrag = DragTarget::None;
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (activeDrag == DragTarget::None || !currentSlot || !vts) return;

    const float w = (float)getWidth();
    
    if (activeDrag == DragTarget::Scrollbar)
    {
        viewStartRatio = (e.x - (w / zoomLevel) * 0.5f) / w;
        viewStartRatio = juce::jlimit(0.0f, std::max(0.0f, 1.0f - 1.0f / zoomLevel), viewStartRatio);
        repaint();
        return;
    }

    float normX = (e.x / w) / zoomLevel + viewStartRatio;
    normX = juce::jlimit(0.0f, 1.0f, normX);
    
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

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (!currentSlot || !currentSlot->isReady()) return;

    if (wheel.deltaY != 0.0f)
    {
        float preZoomRatio = viewStartRatio + (e.x / (float)getWidth()) / zoomLevel;
        
        zoomLevel *= (1.0f + wheel.deltaY * 2.5f);
        zoomLevel = juce::jlimit(1.0f, 100000.0f, zoomLevel); // allow extreme zoom for sample-accuracy
        
        viewStartRatio = preZoomRatio - (e.x / (float)getWidth()) / zoomLevel;
        viewStartRatio = juce::jlimit(0.0f, std::max(0.0f, 1.0f - 1.0f / zoomLevel), viewStartRatio);
        
        repaint();
    }
    else if (wheel.deltaX != 0.0f)
    {
        viewStartRatio -= wheel.deltaX * 0.5f / zoomLevel;
        viewStartRatio = juce::jlimit(0.0f, std::max(0.0f, 1.0f - 1.0f / zoomLevel), viewStartRatio);
        repaint();
    }
}

void WaveformDisplay::timerCallback()
{
    animPhase += 0.03f;
    repaint();
}

juce::Colour WaveformDisplay::getSpectralColor(float brightness) const noexcept
{
    const float b = juce::jlimit(0.0f, 1.0f, brightness * 3.0f);
    return PicoColors::waveGradStart.interpolatedWith(PicoColors::waveGradEnd, b);
}
