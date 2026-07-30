// ==========================================
// File: WaveformDisplay.cpp
// WaveformDisplay 実装 (Reverseオン時の波形反転描画 & 直感的一貫マーカー操作)
// ==========================================
#include "WaveformDisplay.h"
#include <cmath>

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

    // ローダースレッドがバッファを差し替えても解放済みメモリを読まないよう保持する。
    // (isReady() を見た直後に clear()/loadFromFile() が走るとポインタが無効になる)
    const SampleSlot::ReadGuard slotGuard(*currentSlot);
    if (!slotGuard.isValid()) return;

    const auto& buffer = currentSlot->getOriginalBuffer();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || buffer.getNumChannels() < 1) return;

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
    // APVTS 側が外部要因 (ノブ/プリセット/オートメーション) で動いた時のみ UI 値を追従させる。
    // ドラッグ中は UI 値が真値なので絶対に上書きしない。
    const bool isDraggingMarker = (activeDrag != DragTarget::None && activeDrag != DragTarget::Scrollbar);
    auto getDouble = [&](const juce::String& name, double& uiVal, float def) {
        if (!vts) return def;
        if (auto* p = vts->getRawParameterValue(name)) {
            const float apvtsVal = p->load();
            if (!isDraggingMarker && apvtsVal != (float)uiVal)
                uiVal = (double)apvtsVal;
            return (float)uiVal;
        }
        return def;
    };

    float startRatio = getDouble("sampleStart_" + s, uiStartRatio[activeSlot], 0.0f);
    float endRatio   = getDouble("sampleEnd_" + s, uiEndRatio[activeSlot], 1.0f);
    float loopStart  = getDouble("loopStart_" + s, uiLoopStart[activeSlot], 0.2f);
    float loopEnd    = getDouble("loopEnd_" + s, uiLoopEnd[activeSlot], 0.7f);
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

    float sX = ratioToX(startRatio);
    float eX = ratioToX(endRatio);
    const float liveSX = ratioToX(liveStartRatio);
    const float liveEX = ratioToX(liveEndRatio);
    float liveLSX = ratioToX(liveLStart);
    float liveLEX = ratioToX(liveLEnd);

    if (activeDrag == DragTarget::SampleStart) sX = ratioToX(uiStartRatio[activeSlot]);
    else if (activeDrag == DragTarget::SampleEnd) eX = ratioToX(uiEndRatio[activeSlot]);
    else if (activeDrag == DragTarget::LoopStart) liveLSX = ratioToX(uiLoopStart[activeSlot]);
    else if (activeDrag == DragTarget::LoopEnd) liveLEX = ratioToX(uiLoopEnd[activeSlot]);

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

        // X-Fade フェードオーバーラップ領域をグラデーション描画 (L-End手前: フェードアウト側)
        juce::ColourGradient xfGrad(PicoColors::mint.withAlpha(0.0f), xfX, 0.0f, PicoColors::mint.withAlpha(0.35f), xfX + xfW, 0.0f, false);
        g.setGradientFill(xfGrad);
        g.fillRect(xfX, lpMarginY, std::max(1.0f, xfW), lpH);

        // L-Start 側 (フェードイン側): ループ突入時・周回時ともにここの内容が
        // 上のフェードアウト領域とブレンドされるため、同じ幅で対になる表示にする。
        juce::ColourGradient xfGradIn(PicoColors::mint.withAlpha(0.35f), lsX, 0.0f, PicoColors::mint.withAlpha(0.0f), lsX + xfW, 0.0f, false);
        g.setGradientFill(xfGradIn);
        g.fillRect(lsX, lpMarginY, std::max(1.0f, xfW), lpH);

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

void WaveformDisplay::syncUiFromParams() noexcept
{
    if (!vts) return;
    const juce::String s = juce::String(activeSlot);

    auto pull = [this, &s](const juce::String& name, double& uiVal)
    {
        if (auto* p = vts->getRawParameterValue(name + "_" + s))
        {
            const float v = p->load();
            // float に丸めて一致していなければ外部変更とみなして取り込む
            if (v != (float)uiVal)
                uiVal = (double)v;
        }
    };

    pull("sampleStart", uiStartRatio[activeSlot]);
    pull("sampleEnd",   uiEndRatio[activeSlot]);
    pull("loopStart",   uiLoopStart[activeSlot]);
    pull("loopEnd",     uiLoopEnd[activeSlot]);
}

float WaveformDisplay::findZeroCrossingRatio(float targetRatio) const noexcept
{
    if (!currentSlot) return targetRatio;

    const SampleSlot::ReadGuard slotGuard(*currentSlot);
    if (!slotGuard.isValid()) return targetRatio;

    const auto& buffer = currentSlot->getOriginalBuffer();
    const int numSamples = buffer.getNumSamples();
    if (numSamples < 2 || buffer.getNumChannels() < 1) return targetRatio;

    const float* samples = buffer.getReadPointer(0);
    if (!samples) return targetRatio;

    const int lastIdx = numSamples - 1;
    const double dSamples = (double)lastIdx;

    const int targetIdx = juce::jlimit(0, lastIdx, (int)std::llround((double)targetRatio * dSamples));

    // 「ゼロ交差」判定: samples[i] と samples[i+1] の符号が変わる点、
    // または振幅がほぼ 0 の点。JUCE の float は ±1.0 スケールなので閾値は十分小さく。
    auto isZeroCross = [samples, lastIdx](int i) noexcept
    {
        if (i < 0 || i >= lastIdx) return false;
        const float a = samples[i];
        const float b = samples[i + 1];
        if (a == 0.0f) return true;
        return (a < 0.0f) != (b < 0.0f);
    };

    // 交差ペア (i, i+1) のうち、絶対値が小さい側を採用する。
    auto refine = [samples, lastIdx](int i) noexcept
    {
        if (i < 0 || i >= lastIdx) return juce::jlimit(0, lastIdx, i);
        return (std::abs(samples[i]) <= std::abs(samples[i + 1])) ? i : i + 1;
    };

    // ------------------------------------------------------------------
    // 1) トランジェント (Onset) への磁力スナップ
    //    磁力半径は「画面上のピクセル距離」基準にする。
    //    こうするとズーム率が変わってもマウス感覚上の吸着距離が一定になる。
    // ------------------------------------------------------------------
    const double pixelsPerSample = ((double)getWidth() * (double)zoomLevel) / std::max(1.0, dSamples);
    const int onsetMagnetSamples =
        juce::jlimit(4, numSamples, (int)std::ceil(12.0 / std::max(1.0e-9, pixelsPerSample)));

    const auto& onsets = currentSlot->getOnsetSamples();
    int bestOnset = -1;
    int minOnsetDist = onsetMagnetSamples;

    for (int onset : onsets)
    {
        if (onset < 0 || onset > lastIdx) continue;
        const int dist = std::abs(onset - targetIdx);
        if (dist < minOnsetDist)
        {
            minOnsetDist = dist;
            bestOnset = onset;
        }
    }

    // ------------------------------------------------------------------
    // 2) ゼロ交差検索。
    //    Onset が見つかった場合はその近傍を、見つからなければマウス位置近傍を探す。
    //    検索範囲はズーム率に依存させない。見つかるまで最後まで探す
    //    (= Snap ON なら必ずゼロ交差に着地することを保証する)。
    // ------------------------------------------------------------------
    const int anchor = (bestOnset >= 0) ? bestOnset : targetIdx;

    if (isZeroCross(anchor))
        return (float)((double)refine(anchor) / dSamples);

    const int maxSearch = numSamples;
    for (int d = 1; d < maxSearch; ++d)
    {
        const int left  = anchor - d;
        const int right = anchor + d;
        const bool leftValid  = (left >= 0);
        const bool rightValid = (right < lastIdx);

        if (!leftValid && !rightValid) break;

        if (leftValid && isZeroCross(left))
            return (float)((double)refine(left) / dSamples);

        if (rightValid && isZeroCross(right))
            return (float)((double)refine(right) / dSamples);
    }

    // ゼロ交差が皆無 (DC オフセット波形など) の場合のみ、元の位置を返す。
    return (float)((double)targetIdx / dSamples);
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

    // ドラッグ開始前に、高精度 UI 値を APVTS の現在値と同期させる。
    // (プリセット読込 / AutoSlice / ノブ操作で外部から変更された場合に
    //  ドラッグ開始直後にマーカーが飛ぶのを防ぐ)
    syncUiFromParams();

    const double startRatio = uiStartRatio[activeSlot];
    const double endRatio   = uiEndRatio[activeSlot];
    const double loopStart  = uiLoopStart[activeSlot];
    const double loopEnd    = uiLoopEnd[activeSlot];
    const bool isLooping    = getParamFloat("isLooping_" + s, 0.0f) > 0.5f;

    auto ratioToX = [&](double ratio) { return (float)((ratio - (double)viewStartRatio) * (double)zoomLevel * (double)w); };

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

    if (activeDrag == DragTarget::SampleStart) dragStartParamValue = uiStartRatio[activeSlot];
    else if (activeDrag == DragTarget::SampleEnd) dragStartParamValue = uiEndRatio[activeSlot];
    else if (activeDrag == DragTarget::LoopStart) dragStartParamValue = uiLoopStart[activeSlot];
    else if (activeDrag == DragTarget::LoopEnd) dragStartParamValue = uiLoopEnd[activeSlot];

    dragStartX = e.x;
    dragStartXf = e.position.x;   // サブピクセル精度の開始位置
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (activeDrag == DragTarget::None || !currentSlot || !vts) return;

    currentMouseX = e.x;

    const float w = (float)getWidth();
    
    if (activeDrag == DragTarget::Scrollbar)
    {
        viewStartRatio = (e.x - (w / zoomLevel) * 0.5f) / w;
        viewStartRatio = juce::jlimit(0.0f, std::max(0.0f, 1.0f - 1.0f / zoomLevel), viewStartRatio);
        repaint();
        return;
    }

    // サブピクセル精度でドラッグ量を積算する (int の e.x ではなく float の e.position.x)。
    // ズーム率で割ることで、拡大時は 1px あたりの移動量が細かくなり
    // サンプル単位の追い込みが可能になる。
    const double deltaRatio = (double)(e.position.x - dragStartXf) / (double)w / (double)zoomLevel;
    double normX = dragStartParamValue + deltaRatio;
    normX = juce::jlimit(0.0, 1.0, normX);
    
    const juce::String s = juce::String(activeSlot);

    auto getParamFloat = [this](const juce::String& name, float def) {
        if (!vts) return def;
        if (auto* p = vts->getRawParameterValue(name)) return p->load();
        return def;
    };

    const bool isSnap = getParamFloat("isSnap_" + s, 0.0f) > 0.5f;
    if (isSnap) normX = (double)findZeroCrossingRatio((float)normX);

    const double startVal = uiStartRatio[activeSlot];
    const double endVal   = uiEndRatio[activeSlot];
    const double lStart   = uiLoopStart[activeSlot];
    const double lEnd     = uiLoopEnd[activeSlot];

    if (activeDrag == DragTarget::SampleStart)
    {
        uiStartRatio[activeSlot] = std::min(normX, endVal - 0.000001);
        if (auto* p = vts->getParameter("sampleStart_" + s)) p->setValueNotifyingHost((float)uiStartRatio[activeSlot]);
    }
    else if (activeDrag == DragTarget::SampleEnd)
    {
        uiEndRatio[activeSlot] = std::max(normX, startVal + 0.000001);
        if (auto* p = vts->getParameter("sampleEnd_" + s)) p->setValueNotifyingHost((float)uiEndRatio[activeSlot]);
    }
    else if (activeDrag == DragTarget::LoopStart)
    {
        uiLoopStart[activeSlot] = std::min(normX, lEnd - 0.000001);
        if (auto* p = vts->getParameter("loopStart_" + s)) p->setValueNotifyingHost((float)uiLoopStart[activeSlot]);
    }
    else if (activeDrag == DragTarget::LoopEnd)
    {
        uiLoopEnd[activeSlot] = std::max(normX, lStart + 0.000001);
        if (auto* p = vts->getParameter("loopEnd_" + s)) p->setValueNotifyingHost((float)uiLoopEnd[activeSlot]);
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
