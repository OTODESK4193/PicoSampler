// ==========================================
// File: PluginEditor.cpp
// PicoSampler メインエディタ GUI 実装 (ReAnalyzeボタン機能)
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

PicoSamplerAudioProcessorEditor::PicoSamplerAudioProcessorEditor(PicoSamplerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      mainPanel(p.getAPVTS()),
      arpPanel(p.getAPVTS()),
      modPanel(p),
      fxPanel(p),
      configPanel(p.getAPVTS())
{
    setLookAndFeel(&lookAndFeel);
    setSize(1080, 700);

    waveDisplay.setAPVTS(&p.getAPVTS());
    addAndMakeVisible(waveDisplay);
    addAndMakeVisible(keyRangeMap);

    for (auto* b : { &btnTabMain, &btnTabArp, &btnTabMod, &btnTabFx, &btnTabConfig })
    {
        addAndMakeVisible(*b);
    }

    btnReAnalyze.setColour(juce::TextButton::buttonColourId, PicoColors::knobTrack);
    btnReAnalyze.setColour(juce::TextButton::textColourOffId, PicoColors::mint);
    addAndMakeVisible(btnReAnalyze);

    btnAutoSlice.setClickingTogglesState(true);
    btnAutoSlice.setColour(juce::TextButton::buttonColourId, PicoColors::knobTrack);
    btnAutoSlice.setColour(juce::TextButton::buttonOnColourId, PicoColors::pink);
    btnAutoSlice.setColour(juce::TextButton::textColourOffId, PicoColors::textDim);
    btnAutoSlice.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible(btnAutoSlice);
    autoSliceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.getAPVTS(), "autoSliceEnable", btnAutoSlice);

    btnPresets.setColour(juce::TextButton::buttonColourId, PicoColors::knobTrack);
    btnPresets.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(btnPresets);

    btnTabMain.onClick   = [this] { setActiveTab(0); };
    btnTabArp.onClick    = [this] { setActiveTab(1); };
    btnTabMod.onClick    = [this] { setActiveTab(2); };
    btnTabFx.onClick     = [this] { setActiveTab(3); };
    btnTabConfig.onClick = [this] { setActiveTab(4); };

    btnReAnalyze.onClick = [this] {
        const bool isAutoSlice = audioProcessor.getAPVTS().getRawParameterValue("autoSliceEnable")->load() > 0.5f;
        if (isAutoSlice) {
            const juce::File file = audioProcessor.getSamplerEngine().getSlot(0).getMetadata().filePath;
            if (file.existsAsFile()) {
                audioProcessor.requestLoadFile(0, file, true);
            }
        } else {
            const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
            audioProcessor.reanalyzeSlot(activeIdx);
        }
        repaint();
    };

    btnPresets.onClick   = [this] { presetBrowser.setVisible(!presetBrowser.isVisible()); };

    addAndMakeVisible(mainPanel);
    addChildComponent(arpPanel);
    addChildComponent(modPanel);
    addChildComponent(fxPanel);
    addChildComponent(configPanel);

    presetBrowser.setVisible(false);
    addAndMakeVisible(presetBrowser);

    // ---------------- プリセットブラウザ配線 ----------------
    presetBrowser.getCategories = []
    {
        return PicoSamplerAudioProcessor::getPresetCategories();
    };

    presetBrowser.getPresetsForCategory = [](juce::String category)
    {
        juce::Array<juce::File> out;

        const auto root = PicoSamplerAudioProcessor::getPresetRootDirectory();
        if (!root.isDirectory()) return out;

        // category が空 = "All" 選択時。サブフォルダも含めて再帰的に集める。
        const auto searchDir = category.isEmpty() ? root : root.getChildFile(category);
        if (!searchDir.isDirectory()) return out;

        for (const auto& e : juce::RangedDirectoryIterator(searchDir,
                                                           category.isEmpty(),
                                                           "*.picopreset",
                                                           juce::File::findFiles))
            out.add(e.getFile());

        // 表示順を安定させる (OS のファイル列挙順に依存させない)
        struct ByName {
            static int compareElements(const juce::File& a, const juce::File& b) {
                return a.getFileNameWithoutExtension()
                        .compareIgnoreCase(b.getFileNameWithoutExtension());
            }
        };
        ByName sorter;
        out.sort(sorter);
        return out;
    };

    presetBrowser.onSaveRequested = [this](juce::String category, juce::String name)
    {
        juce::String error;
        if (!audioProcessor.savePreset(category, name, error))
            presetBrowser.showMessage("Save Failed", error);
    };

    presetBrowser.onPresetChosen = [this](juce::File file)
    {
        juce::StringArray missing;
        juce::String error;

        if (!audioProcessor.loadPreset(file, missing, error))
        {
            presetBrowser.showMessage("Load Failed", error);
            return;
        }

        // パラメータが総入れ替えされるので、GUI の束縛もスロットに合わせ直す
        rebindActiveSlot();

        if (!missing.isEmpty())
        {
            presetBrowser.showMessage(
                "Missing Samples",
                "The preset loaded, but these sample files could not be found and "
                "their slots are empty:\n\n" + missing.joinIntoString("\n"));
        }

        presetBrowser.setVisible(false);
    };

    presetBrowser.onPresetDeleteRequested = [](juce::File file) -> bool
    {
        // 安全策: プリセットルート配下の .picopreset 以外は絶対に消さない。
        // (コールバック経由で任意のパスが渡ってきても事故らないようにする)
        const auto root = PicoSamplerAudioProcessor::getPresetRootDirectory();

        if (!file.existsAsFile()) return false;
        if (!file.hasFileExtension("picopreset")) return false;
        if (!file.isAChildOf(root)) return false;

        if (!file.moveToTrash())
            return file.deleteFile();   // ゴミ箱が使えない環境では直接削除

        return true;
    };

    presetBrowser.onInitConfirmed = [this]
    {
        audioProcessor.resetToInitState();
        rebindActiveSlot();
        presetBrowser.setVisible(false);
        repaint();
    };

    waveDisplay.setVisualizerData(&p.getVisualizerData());

    // 波形エリア D&D & Clear
    waveDisplay.onFileDropped = [this](const juce::File& file) {
        const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
        const bool autoSlice = audioProcessor.getAPVTS().getRawParameterValue("autoSliceEnable")->load() > 0.5f;
        audioProcessor.requestLoadFile(activeIdx, file, autoSlice && activeIdx == 0);
        repaint();
    };

    waveDisplay.onClearSlotRequested = [this](int slotIdx) {
        audioProcessor.clearSlot(slotIdx);
        repaint();
    };
    // KeyRangeMap 操作ハンドラ
    auto snapFunc = [this](double v) -> double {
        const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
        const bool isSnap = audioProcessor.getAPVTS().getRawParameterValue("isSnap_" + juce::String(activeIdx))->load() > 0.5f;
        if (isSnap) {
            return waveDisplay.findZeroCrossingRatio((float)v);
        }
        return v;
    };
    mainPanel.getSampleStartKnob().customSnapFunction = snapFunc;
    mainPanel.getSampleEndKnob().customSnapFunction = snapFunc;
    mainPanel.getLoopStartKnob().customSnapFunction = snapFunc;
    mainPanel.getLoopEndKnob().customSnapFunction = snapFunc;

    keyRangeMap.onKeyRangeChanged = [this](int slotIdx, int low, int high) {
        if (auto* pLow = audioProcessor.getAPVTS().getParameter("slotLowNote_" + juce::String(slotIdx)))
            pLow->setValueNotifyingHost((float)low / 127.0f);
        if (auto* pHigh = audioProcessor.getAPVTS().getParameter("slotHighNote_" + juce::String(slotIdx)))
            pHigh->setValueNotifyingHost((float)high / 127.0f);
        repaint();
    };

    setActiveTab(0);
    startTimerHz(60);
}

PicoSamplerAudioProcessorEditor::~PicoSamplerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PicoSamplerAudioProcessorEditor::styleTabButton(juce::TextButton& b, bool active)
{
    b.setColour(juce::TextButton::buttonColourId,
                active ? PicoColors::panel : juce::Colours::transparentBlack);
    b.setColour(juce::TextButton::textColourOffId,
                active ? juce::Colours::white : PicoColors::textDim);
}

void PicoSamplerAudioProcessorEditor::setActiveTab(int tabIndex)
{
    activeTab = tabIndex;
    if (presetBrowser.isVisible()) presetBrowser.setVisible(false);

    mainPanel.setVisible(activeTab == 0);
    arpPanel.setVisible(activeTab == 1);
    modPanel.setVisible(activeTab == 2);
    fxPanel.setVisible(activeTab == 3);
    configPanel.setVisible(activeTab == 4);

    styleTabButton(btnTabMain,   activeTab == 0);
    styleTabButton(btnTabArp,    activeTab == 1);
    styleTabButton(btnTabMod,    activeTab == 2);
    styleTabButton(btnTabFx,     activeTab == 3);
    styleTabButton(btnTabConfig, activeTab == 4);

    repaint(0, 310, getWidth(), 45);
}

void PicoSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    // 1. ヘッダーロゴ
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    juce::ColourGradient titleGrad(PicoColors::mint, 20.0f, 15.0f, PicoColors::pink, 280.0f, 35.0f, false);
    titleGrad.addColour(0.5, PicoColors::lavender);
    g.setGradientFill(titleGrad);
    g.drawText("P I C O  S A M P L E R", 20, 10, 280, 28, juce::Justification::centredLeft);

    // 2. HUD 情報
    const int activeIdx = (int)audioProcessor.getAPVTS().getRawParameterValue("activeSlot")->load();
    const auto& slot = audioProcessor.getSamplerEngine().getSlot(activeIdx);
    const auto& meta = slot.getMetadata();

    const int lowNote = (int)audioProcessor.getAPVTS().getRawParameterValue("slotLowNote_" + juce::String(activeIdx))->load();
    const int highNote = (int)audioProcessor.getAPVTS().getRawParameterValue("slotHighNote_" + juce::String(activeIdx))->load();

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("Slot " + juce::String(activeIdx + 1) + ": " + (slot.isReady() ? meta.fileName : "Empty"), 310, 8, 380, 15, juce::Justification::left, true);

    g.setColour(PicoColors::mint);
    g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
    const juce::String rootStr = slot.isReady() ? juce::MidiMessage::getMidiNoteName(meta.rootKey, true, true, 4) : "-";
    const juce::String rangeStr = juce::MidiMessage::getMidiNoteName(lowNote, true, true, 4) + " ~ " + juce::MidiMessage::getMidiNoteName(highNote, true, true, 4);

    g.drawText("Root Key: " + rootStr + "  |  Key Range: " + rangeStr, 310, 25, 380, 15, juce::Justification::left);

    // 3. アクティブタブの下線バー描画
    auto drawUnderline = [&g](const juce::TextButton& b, juce::Colour c)
    {
        const auto r = b.getBounds();
        g.setColour(c);
        g.fillRoundedRectangle((float)r.getX() + 6.0f, (float)r.getBottom() + 1.0f,
                               (float)r.getWidth() - 12.0f, 3.0f, 1.5f);
    };

    if (activeTab == 0) drawUnderline(btnTabMain, PicoColors::mint);
    if (activeTab == 1) drawUnderline(btnTabArp, PicoColors::lavender);
    if (activeTab == 2) drawUnderline(btnTabMod, PicoColors::babyBlue);
    if (activeTab == 3) drawUnderline(btnTabFx, PicoColors::pink);
    if (activeTab == 4) drawUnderline(btnTabConfig, PicoColors::sage);
}

void PicoSamplerAudioProcessorEditor::resized()
{
    // 1. 波形表示 (高さ 140px)
    waveDisplay.setBounds(20, 44, 1040, 140);

    // 2. 倍幅 KeyRangeMap コンポーネント (高さ 122px)
    keyRangeMap.setBounds(20, 190, 1040, 122);

    // 3. タブボタン群 (Y=322)
    const int tabY = 322;
    const int tabH = 26;

    btnTabMain.setBounds(20,  tabY, 70,  tabH);
    btnTabArp.setBounds(98,   tabY, 115, tabH);
    btnTabMod.setBounds(221,  tabY, 70,  tabH);
    btnTabFx.setBounds(299,   tabY, 70,  tabH);
    btnTabConfig.setBounds(377, tabY, 80,  tabH);

    btnAutoSlice.setBounds(1080 - 305, 10, 85, 26);
    btnReAnalyze.setBounds(1080 - 215, 10, 95, 26);
    btnPresets.setBounds(1080 - 110, 10, 90, 26);

    // 4. パネル領域 (Y=356)
    const auto panelBounds = juce::Rectangle<int>(0, 356, 1080, 344);
    mainPanel.setBounds(panelBounds);
    arpPanel.setBounds(panelBounds);
    modPanel.setBounds(panelBounds);
    fxPanel.setBounds(panelBounds);
    configPanel.setBounds(panelBounds);

    presetBrowser.setBounds(0, 356, 1080, 344);
}

void PicoSamplerAudioProcessorEditor::rebindActiveSlot()
{
    // MainPanel は「同じスロットなら何もしない」最適化が入っているため、
    // 一度別の値に落としてから貼り直させる。
    mainPanel.invalidateBinding();
    mainPanel.updateStates();
    arpPanel.updateFilterUIState();

    auto* pActive = audioProcessor.getAPVTS().getRawParameterValue("activeSlot");
    const int activeIdx = pActive ? juce::jlimit(0, 7, (int)pActive->load()) : 0;

    waveDisplay.setActiveSlotIndex(activeIdx);
    waveDisplay.setSampleSlot(&audioProcessor.getSamplerEngine().getSlot(activeIdx));
    waveDisplay.setZoomLevel(1.0f);
    waveDisplay.repaint();

    repaint();
}

void PicoSamplerAudioProcessorEditor::timerCallback()
{
    auto* pActive = audioProcessor.getAPVTS().getRawParameterValue("activeSlot");
    if (pActive == nullptr) return;

    const int activeIdx = (int)pActive->load();
    waveDisplay.setSampleSlot(&audioProcessor.getSamplerEngine().getSlot(activeIdx));
    waveDisplay.setModMatrix(&audioProcessor.getModMatrix());
    arpPanel.setModMatrix(&audioProcessor.getModMatrix());
    arpPanel.setSampleRate(audioProcessor.getSampleRate());
    arpPanel.setFilterEnvValue(audioProcessor.getFilterEnvValue());
    waveDisplay.setActiveSlotIndex(activeIdx);
    waveDisplay.repaint();

    auto* pTheme = audioProcessor.getAPVTS().getRawParameterValue("colorTheme");
    if (pTheme != nullptr)
    {
        const int themeIdx = (int)pTheme->load();
        if (themeIdx != lastThemeIdx)
        {
            lastThemeIdx = themeIdx;
            PicoColors::setTheme(themeIdx);
            repaint();
        }
    }

    mainPanel.updateStates();

    std::array<std::pair<int, int>, 8> ranges {};
    std::array<int, 8> roots {};
    std::array<bool, 8> readyStates {};

    for (int i = 0; i < 8; ++i)
    {
        const juce::String s = juce::String(i);
        auto* pLow  = audioProcessor.getAPVTS().getRawParameterValue("slotLowNote_" + s);
        auto* pHigh = audioProcessor.getAPVTS().getRawParameterValue("slotHighNote_" + s);

        const int low  = pLow  ? (int)pLow->load()  : 0;
        const int high = pHigh ? (int)pHigh->load() : 127;
        ranges[(size_t)i] = { low, high };

        const auto& slot = audioProcessor.getSamplerEngine().getSlot(i);
        roots[(size_t)i] = slot.getMetadata().rootKey;
        readyStates[(size_t)i] = slot.isReady();
    }

    keyRangeMap.updateKeyRanges(ranges, roots, readyStates, activeIdx);
    keyRangeMap.setPlayingNotes(audioProcessor.getSamplerEngine().getPlayingNotes());
    arpPanel.updateFilterUIState();

    // 6. モジュレーション変調リング & ライブドット表示のGUIプロパティ更新
    const auto& mod = audioProcessor.getModMatrix();

    auto updateKnobProps = [&](juce::Slider& knob, int dstIdx)
    {
        const float minOffset = mod.getRangeMin(dstIdx);
        const float maxOffset = mod.getRangeMax(dstIdx);
        const float liveOffset = mod.get(dstIdx);
        const bool active = (std::abs(minOffset) > 0.001f || std::abs(maxOffset) > 0.001f);

        auto& props = knob.getProperties();
        const bool prevActive = props.getWithDefault("mod_active", false);
        const float prevLive = props.getWithDefault("mod_live", -999.0f);

        props.set("mod_active", active);
        if (active)
        {
            const float normVal = (float)knob.valueToProportionOfLength(knob.getValue());
            const float liveVal = juce::jlimit(0.0f, 1.0f, normVal + liveOffset);
            props.set("mod_min", juce::jlimit(0.0f, 1.0f, normVal + minOffset));
            props.set("mod_max", juce::jlimit(0.0f, 1.0f, normVal + maxOffset));
            props.set("mod_live", liveVal);

            if (std::abs(liveVal - prevLive) > 0.0001f || !prevActive)
            {
                knob.repaint();
            }
        }
        else
        {
            props.remove("mod_min");
            props.remove("mod_max");
            props.remove("mod_live");
            if (prevActive) knob.repaint();
        }
    };

    // MainPanel ノブ範囲更新 (現在画面にバインドされているスロットの変調を正確に指定)
    const int boundSlot = mainPanel.getCurrentBoundSlot() >= 0 ? mainPanel.getCurrentBoundSlot() : activeIdx;
    updateKnobProps(mainPanel.getSampleStartKnob(), ModMatrix::DstS1Start + boundSlot * 5);
    updateKnobProps(mainPanel.getSampleEndKnob(),   ModMatrix::DstS1End + boundSlot * 5);
    updateKnobProps(mainPanel.getLoopStartKnob(),   ModMatrix::DstS1LStart + boundSlot * 5);
    updateKnobProps(mainPanel.getLoopEndKnob(),     ModMatrix::DstS1LEnd + boundSlot * 5);
    updateKnobProps(mainPanel.getCrossfadeKnob(),   ModMatrix::DstS1XFade + boundSlot * 5);

    // Pan は Start..X-Fade の 5個ブロックとは別に、末尾へ 8個まとめて追加してある
    // (既存プロジェクトのアサイン先番号をずらさないため) ので stride は 1。
    updateKnobProps(mainPanel.getSlotPanKnob(),     ModMatrix::DstS1Pan + boundSlot);

    updateKnobProps(mainPanel.getMasterPitchKnob(), ModMatrix::DstMasterPitch);

    // ArpPanel ノブ範囲更新
    updateKnobProps(arpPanel.getOctavesKnob(), ModMatrix::DstArpOctaves);
    updateKnobProps(arpPanel.getRateKnob(),    ModMatrix::DstArpRate);
    updateKnobProps(arpPanel.getGateKnob(),    ModMatrix::DstArpGate);
    updateKnobProps(arpPanel.getOffsetKnob(),  ModMatrix::DstArpOffset);
    updateKnobProps(arpPanel.getSwingKnob(),   ModMatrix::DstArpSwing);
    updateKnobProps(arpPanel.getRepeatKnob(),  ModMatrix::DstArpRepeat);
    updateKnobProps(arpPanel.getAccentKnob(),  ModMatrix::DstArpAccent);

    updateKnobProps(arpPanel.getCutoffKnob(),  ModMatrix::DstFltCutoff);
    updateKnobProps(arpPanel.getResoKnob(),    ModMatrix::DstFltReso);
    updateKnobProps(arpPanel.getFormantKnob(), ModMatrix::DstFltFormant);
    updateKnobProps(arpPanel.getCombMixKnob(), ModMatrix::DstFltCombMix);

    // ModPanel: LFO Rate ノブ範囲更新 (LFO→LFO クロスモジュレーション)
    for (int i = 0; i < ModMatrix::kNumLfos; ++i)
    {
        updateKnobProps(modPanel.getLfoRateKnob(i), ModMatrix::DstLfo1Rate + i);
    }

    // FxPanel ノブ範囲更新
    for (int i = 0; i < 5; ++i)
    {
        if (auto* k = fxPanel.getSlotAmountKnob(i))
        {
            updateKnobProps(*k, ModMatrix::DstFx1Amount + i);
        }
    }

    // 【修正】 detailKnobs は選択中のFXタイプによって中身が丸ごと入れ替わるため、
    // 「先頭からの連番 = DstSatDrive起点の連番」という決め打ちは誤り
    // (例: Chorus選択時に detailKnobs[0]=ChoRate が DstSatDrive の変調レンジを
    //  表示してしまっていた)。FxPanel側が保持する実際のDst対応表を使う。
    const auto& detailKnobs = fxPanel.getDetailKnobs();
    const auto& detailKnobDsts = fxPanel.getDetailKnobDsts();
    for (size_t i = 0; i < detailKnobs.size() && i < detailKnobDsts.size(); ++i)
    {
        if (detailKnobs[i])
        {
            updateKnobProps(*detailKnobs[i], detailKnobDsts[i]);
        }
    }

    repaint(0, 0, 700, 44);
}
