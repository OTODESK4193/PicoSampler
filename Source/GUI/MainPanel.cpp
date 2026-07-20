// ==========================================
// File: MainPanel.cpp
// MainPanel 実装 (ADSR Link 連動のバグ修正 & スケール正規化対応)
// ==========================================
#include "MainPanel.h"

static int noteNameToMidiNumber(const juce::String& name)
{
    if (name.containsIgnoreCase("Auto")) return -1;
    
    static const juce::StringArray noteNames = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::String str = name.trim().toUpperCase();

    int octave = 4;
    juce::String notePart = str;

    for (int i = 0; i < str.length(); ++i)
    {
        if (str[i] == '-' || (str[i] >= '0' && str[i] <= '9'))
        {
            octave = str.substring(i).getIntValue();
            notePart = str.substring(0, i).trim();
            break;
        }
    }

    if (notePart.endsWith("B") && notePart.length() == 2 && notePart[0] != 'A' && notePart[0] != 'C')
    {
        char key = notePart[0];
        if (key == 'D') notePart = "C#";
        else if (key == 'E') notePart = "D#";
        else if (key == 'G') notePart = "F#";
        else if (key == 'A') notePart = "G#";
        else if (key == 'B') notePart = "A#";
    }

    for (int i = 0; i < 12; ++i)
    {
        if (noteNames[i] == notePart)
            return (octave + 1) * 12 + i;
    }

    return juce::jlimit(-1, 127, name.getIntValue());
}

MainPanel::MainPanel(juce::AudioProcessorValueTreeState& apvts) : vts(apvts)
{
    // Slot 1-8 Buttons
    for (int i = 0; i < 8; ++i)
    {
        btnSlots[(size_t)i].setButtonText("S" + juce::String(i + 1));
        btnSlots[(size_t)i].onClick = [this, i] {
            if (auto* p = vts.getParameter("activeSlot"))
                p->setValueNotifyingHost((float)i / 7.0f);
            updateStates();
        };
        addAndMakeVisible(btnSlots[(size_t)i]);
    }

    // Mode Buttons
    btnSingle.onClick = [this] { if (auto* p = vts.getParameter("samplerMode")) p->setValueNotifyingHost(0.0f); updateStates(); };
    btnLayer.onClick  = [this] { if (auto* p = vts.getParameter("samplerMode")) p->setValueNotifyingHost(0.5f); updateStates(); };
    btnRandom.onClick = [this] { if (auto* p = vts.getParameter("samplerMode")) p->setValueNotifyingHost(1.0f); updateStates(); };

    addAndMakeVisible(btnSingle);
    addAndMakeVisible(btnLayer);
    addAndMakeVisible(btnRandom);

    addAndMakeVisible(btnLoop);
    addAndMakeVisible(btnStretch);
    addAndMakeVisible(btnReverse);
    addAndMakeVisible(btnSnap);

    addAndMakeVisible(btnFltBypass);
    addAndMakeVisible(btnFxBypass);

    // RootKey ノブのテキスト表示関数設定 (-1 = Auto, 0-127 = C-1~G9)
    knobRootKey.knob.textFromValueFunction = [](double val) {
        const int v = (int)val;
        return (v < 0) ? juce::String("Auto") : juce::MidiMessage::getMidiNoteName(v, true, true, 4);
    };
    knobRootKey.knob.valueFromTextFunction = [](const juce::String& text) {
        return (double)noteNameToMidiNumber(text);
    };

    // ADSR Link Button (ダイアログなしでSlot1現在値に即時リンク・同期)
    addAndMakeVisible(btnLinkEnv);
    btnLinkEnv.onClick = [this] {
        isEnvLinked = !isEnvLinked;
        btnLinkEnv.setToggleState(isEnvLinked, juce::dontSendNotification);

        if (isEnvLinked && !isUpdatingFromLink)
        {
            isUpdatingFromLink = true;
            // Slot 1 (index 0) の正規化値を Slot 2~8 に即時同期
            for (const auto& pBaseName : { juce::String("attack"), juce::String("decay"), juce::String("sustain"), juce::String("release") })
            {
                if (auto* p0 = vts.getParameter(pBaseName + "_0"))
                {
                    const float normVal = p0->getValue();
                    for (int k = 1; k < 8; ++k)
                    {
                        if (auto* pOther = vts.getParameter(pBaseName + "_" + juce::String(k)))
                            pOther->setValueNotifyingHost(normVal);
                    }
                }
            }
            isUpdatingFromLink = false;
        }
    };

    // ADSR ノブ変更時の連動 (APVTS 正規化値 0.0~1.0 を使用 & 再帰防止)
    auto setupEnvCallback = [this](LabeledKnob& lk, const juce::String& pBaseName) {
        lk.knob.onValueChange = [this, pBaseName] {
            if (isEnvLinked && !isUpdatingFromLink)
            {
                isUpdatingFromLink = true;
                const juce::String activeSlotStr = juce::String(currentBoundSlot >= 0 ? currentBoundSlot : 0);
                if (auto* activeParam = vts.getParameter(pBaseName + "_" + activeSlotStr))
                {
                    const float normVal = activeParam->getValue(); // 正確な正規化値を取得
                    for (int k = 0; k < 8; ++k)
                    {
                        if (k != currentBoundSlot)
                        {
                            if (auto* p = vts.getParameter(pBaseName + "_" + juce::String(k)))
                                p->setValueNotifyingHost(normVal);
                        }
                    }
                }
                isUpdatingFromLink = false;
            }
        };
    };

    setupEnvCallback(knobAttack,  "attack");
    setupEnvCallback(knobDecay,   "decay");
    setupEnvCallback(knobSustain, "sustain");
    setupEnvCallback(knobRelease, "release");

    // Master ノブ
    masterPitchAttach = std::make_unique<Attachment>(vts, "masterPitch", knobMasterPitch.knob);
    masterHpfAttach = std::make_unique<Attachment>(vts, "masterHPF", knobMasterHpf.knob);
    masterLpfAttach = std::make_unique<Attachment>(vts, "masterLPF", knobMasterLpf.knob);
    outGainAttach   = std::make_unique<Attachment>(vts, "outGain", knobOutGain.knob);
    ceilingAttach   = std::make_unique<Attachment>(vts, "ceiling", knobCeiling.knob);

    // ノブダブルクリック時のデフォルト値リセット設定
    knobSampleStart.knob.setDoubleClickReturnValue(true, 0.0);
    knobSampleEnd.knob.setDoubleClickReturnValue(true, 1.0);
    knobLoopStart.knob.setDoubleClickReturnValue(true, 0.2);
    knobLoopEnd.knob.setDoubleClickReturnValue(true, 0.7);
    knobCrossfade.knob.setDoubleClickReturnValue(true, 0.05);

    knobRootKey.knob.setDoubleClickReturnValue(true, -1.0);
    knobOctave.knob.setDoubleClickReturnValue(true, 0.0);
    knobSemitone.knob.setDoubleClickReturnValue(true, 0.0);
    knobFineTune.knob.setDoubleClickReturnValue(true, 0.0);

    knobAttack.knob.setDoubleClickReturnValue(true, 0.01);
    knobDecay.knob.setDoubleClickReturnValue(true, 0.3);
    knobSustain.knob.setDoubleClickReturnValue(true, 1.0);
    knobRelease.knob.setDoubleClickReturnValue(true, 0.3);

    knobMasterPitch.knob.setDoubleClickReturnValue(true, 0.0);
    knobMasterHpf.knob.setDoubleClickReturnValue(true, 20.0);
    knobMasterLpf.knob.setDoubleClickReturnValue(true, 20000.0);
    knobOutGain.knob.setDoubleClickReturnValue(true, 0.0);
    knobCeiling.knob.setDoubleClickReturnValue(true, 0.0);
    knobSlotVolume.knob.setDoubleClickReturnValue(true, 0.0);   // 0 dB = ユニティ
    knobSlotPan.knob.setDoubleClickReturnValue(true, 0.0);      // センター

    // Pan は L/C/R が一目で分かる表記にする (-1.00 では向きが直感的でない)
    knobSlotPan.knob.textFromValueFunction = [](double v)
    {
        if (std::abs(v) < 0.005) return juce::String("C");
        const int amount = (int)std::round(std::abs(v) * 100.0);
        return (v < 0.0 ? juce::String("L") : juce::String("R")) + juce::String(amount);
    };

    // スロット音量は dB 表示 (0.00 のような生値では単位が伝わらない)
    knobSlotVolume.knob.textFromValueFunction = [](double v)
    {
        if (v <= -35.99) return juce::String("-inf");
        return juce::String(v, 1) + " dB";
    };

    addAndMakeVisible(knobSampleStart);
    addAndMakeVisible(knobSampleEnd);
    addAndMakeVisible(knobLoopStart);
    addAndMakeVisible(knobLoopEnd);
    addAndMakeVisible(knobCrossfade);
    addAndMakeVisible(knobSlotPan);
    addAndMakeVisible(knobSlotVolume);

    addAndMakeVisible(knobRootKey);
    addAndMakeVisible(knobOctave);
    addAndMakeVisible(knobSemitone);
    addAndMakeVisible(knobFineTune);

    addAndMakeVisible(knobAttack);
    addAndMakeVisible(knobDecay);
    addAndMakeVisible(knobSustain);
    addAndMakeVisible(knobRelease);

    addAndMakeVisible(knobMasterPitch);
    addAndMakeVisible(knobMasterHpf);
    addAndMakeVisible(knobMasterLpf);
    addAndMakeVisible(knobOutGain);
    addAndMakeVisible(knobCeiling);

    bindSlotParameters(0);
    updateStates();
}

void MainPanel::bindSlotParameters(int slotIdx)
{
    if (currentBoundSlot == slotIdx) return;
    currentBoundSlot = slotIdx;

    attachments.clear();
    loopAttach.reset();
    stretchAttach.reset();
    reverseAttach.reset();
    snapAttach.reset();
    fltBypassAttach.reset();
    fxBypassAttach.reset();

    // Slot固有のテーマカラー適用 & 変調残像の即時クリア
    const auto slotColor = PicoColors::getSlotColor(slotIdx);
    for (auto* lk : { &knobSampleStart, &knobSampleEnd, &knobLoopStart, &knobLoopEnd, &knobCrossfade })
    {
        lk->knob.setColour(juce::Slider::rotarySliderFillColourId, slotColor);
        lk->knob.getProperties().remove("mod_active");
        lk->knob.getProperties().remove("mod_min");
        lk->knob.getProperties().remove("mod_max");
        lk->knob.getProperties().remove("mod_live");
        lk->knob.repaint();
    }

    const juce::String s = juce::String(slotIdx);

    auto bind = [this, &s](LabeledKnob& lk, const juce::String& pName) {
        attachments.push_back(std::make_unique<Attachment>(vts, pName + "_" + s, lk.knob));
    };

    bind(knobSampleStart, "sampleStart");
    bind(knobSampleEnd,   "sampleEnd");
    bind(knobLoopStart,   "loopStart");
    bind(knobLoopEnd,     "loopEnd");
    bind(knobCrossfade,   "crossfade");
    bind(knobSlotPan,     "pan");
    bind(knobSlotVolume,  "slotGain");

    // Start/End/Loop は連続値パラメータ (interval = 0)。
    // JUCE は interval が 0 だと表示桁を 7 桁に決め打ちするため、
    // "0.1872500" のように テキスト表示からあふれて "0.18725..." と切れてしまう。
    //
    // setNumDecimalPlacesToDisplay() は setNormalisableRange() 側の再計算に
    // 上書きされる可能性があるので、表示関数そのものを明示指定して確実に固定する。
    for (auto* lk : { &knobSampleStart, &knobSampleEnd, &knobLoopStart, &knobLoopEnd, &knobCrossfade })
    {
        lk->knob.setNumDecimalPlacesToDisplay(4);
        lk->knob.textFromValueFunction = [](double v) { return juce::String(v, 4); };
        lk->knob.updateText();

        lk->knob.setBaseSensitivity(4000);    // 通常: 高解像度で追い込む
        lk->knob.setCoarseSensitivity(200);   // Ctrl: 端から端まで一気に
        lk->knob.setFineSensitivity(16000);   // Shift: サンプル単位の微調整
    }

    knobSlotPan.knob.updateText();
    knobSlotVolume.knob.updateText();

    bind(knobRootKey,  "rootKey");
    bind(knobOctave,   "octave");
    bind(knobSemitone, "pitchSt");
    bind(knobFineTune, "fineTune");

    bind(knobAttack,  "attack");
    bind(knobDecay,   "decay");
    bind(knobSustain, "sustain");
    bind(knobRelease, "release");

    loopAttach      = std::make_unique<ButtonAttach>(vts, "isLooping_" + s, btnLoop);
    stretchAttach   = std::make_unique<ButtonAttach>(vts, "isStretchMode_" + s, btnStretch);
    reverseAttach   = std::make_unique<ButtonAttach>(vts, "isReverse_" + s, btnReverse);
    snapAttach      = std::make_unique<ButtonAttach>(vts, "isSnap_" + s, btnSnap);
    fltBypassAttach = std::make_unique<ButtonAttach>(vts, "filterBypass_" + s, btnFltBypass);
    fxBypassAttach  = std::make_unique<ButtonAttach>(vts, "fxBypass_" + s, btnFxBypass);
}

void MainPanel::updateStates()
{
    auto* pActive = vts.getRawParameterValue("activeSlot");
    auto* pMode   = vts.getRawParameterValue("samplerMode");
    if (!pActive || !pMode) return;

    const int activeSlotIdx = (int)pActive->load();
    bindSlotParameters(activeSlotIdx);

    const int modeVal = (int)pMode->load();
    auto styleModeBtn = [](juce::TextButton& b, bool active) {
        b.setColour(juce::TextButton::buttonColourId, active ? PicoColors::mint : PicoColors::knobTrack);
        b.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::black : PicoColors::textDim);
    };

    styleModeBtn(btnSingle, modeVal == 0);
    styleModeBtn(btnLayer,  modeVal == 1);
    styleModeBtn(btnRandom, modeVal == 2);

    for (int i = 0; i < 8; ++i)
    {
        const bool isAct = (i == activeSlotIdx);
        const auto col = PicoColors::getSlotColor(i);
        btnSlots[(size_t)i].setColour(juce::TextButton::buttonColourId, isAct ? col : PicoColors::knobTrack);
        btnSlots[(size_t)i].setColour(juce::TextButton::textColourOffId, isAct ? juce::Colours::black : PicoColors::textDim);
    }
}

void MainPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    auto drawSectionHeader = [&g](const juce::String& name, int x, int y, int w, juce::Colour accent)
    {
        g.setColour(accent);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(name, x, y, w, 14, juce::Justification::left);
        g.setColour(accent.withAlpha(0.35f));
        g.fillRect(x, y + 16, w, 1);
    };

    // セパレーター下線 (Y=26)
    drawSectionHeader("ACTIVE SLOT",   20,  10, 320, PicoColors::mint);
    drawSectionHeader("PLAYBACK MODE", 360, 10, 220, PicoColors::babyBlue);
    drawSectionHeader("ROUTING",       600, 10, 220, PicoColors::rose);

    // 1段目: SAMPLE / LOOP, ENVELOPE (Y=80)
    drawSectionHeader("SAMPLE / LOOP", kSampleX, 80, kEnvX - kSampleX - 20, PicoColors::peach);
    drawSectionHeader("ENVELOPE",      kEnvX,    80, 1060 - kEnvX,          PicoColors::pink);

    // 2段目: PITCH, MASTER (Y=206)
    drawSectionHeader("PITCH",         20,  206, 300, PicoColors::lavender);
    drawSectionHeader("MASTER",        360, 206, 420, PicoColors::mint);
}

void MainPanel::resized()
{
    const int btnY = 32;

    for (int i = 0; i < 8; ++i)
    {
        btnSlots[(size_t)i].setBounds(20 + i * 31, btnY, 28, 22);
    }

    btnSingle.setBounds(360, btnY, 68, 22);
    btnLayer.setBounds(433, btnY, 68, 22);
    btnRandom.setBounds(506, btnY, 68, 22);

    btnFltBypass.setBounds(600, btnY, 102, 22);
    btnFxBypass.setBounds(707, btnY, 102, 22);

    const int knobY1 = 106;
    const int knobW = 60;
    const int knobH = 78;

    // --- 1段目 ---
    // SAMPLE / LOOP (7ノブ + 2行2列ボタン)
    // ノブが増えるたびにボタン列と ENVELOPE ブロックを右へずらす。
    // 座標は MainPanel.h の kSampleX / kToggleX / kEnvX で一元管理。
    LabeledKnob* const sampleRow[] = { &knobSampleStart, &knobSampleEnd, &knobLoopStart,
                                       &knobLoopEnd, &knobCrossfade, &knobSlotPan, &knobSlotVolume };
    static_assert(sizeof(sampleRow) / sizeof(sampleRow[0]) == kNumSampleKnobs, "sampleRow と kNumSampleKnobs が不一致");

    for (int i = 0; i < kNumSampleKnobs; ++i)
        sampleRow[i]->setBounds(kSampleX + i * kSampleStep, knobY1, knobW, knobH);

    btnLoop.setBounds   (kToggleX,      knobY1 + 4,  66, 24);
    btnStretch.setBounds(kToggleX + 72, knobY1 + 4,  72, 24);
    btnReverse.setBounds(kToggleX,      knobY1 + 36, 66, 24);
    btnSnap.setBounds   (kToggleX + 72, knobY1 + 36, 66, 24);

    // ENVELOPE (4ノブ + LINK ボタン)
    knobAttack.setBounds (kEnvX + 0 * kEnvStep, knobY1, knobW, knobH);
    knobDecay.setBounds  (kEnvX + 1 * kEnvStep, knobY1, knobW, knobH);
    knobSustain.setBounds(kEnvX + 2 * kEnvStep, knobY1, knobW, knobH);
    knobRelease.setBounds(kEnvX + 3 * kEnvStep, knobY1, knobW, knobH);

    btnLinkEnv.setBounds(kEnvX + 4 * kEnvStep + 6, knobY1 + 24, 56, 24);

    // --- 2段目 ---
    const int knobY2 = 232;
    // PITCH (4ノブ: Root, Octave, Semi, Fine)
    knobRootKey.setBounds(20 + 0 * 75,  knobY2, knobW, knobH);
    knobOctave.setBounds(20 + 1 * 75,   knobY2, knobW, knobH);
    knobSemitone.setBounds(20 + 2 * 75, knobY2, knobW, knobH);
    knobFineTune.setBounds(20 + 3 * 75, knobY2, knobW, knobH);

    // MASTER (5ノブ: Pitch, HPF, LPF, Gain, Ceiling の順)
    knobMasterPitch.setBounds(360 + 0 * 68, knobY2, knobW, knobH);
    knobMasterHpf.setBounds(360 + 1 * 68,   knobY2, knobW, knobH);
    knobMasterLpf.setBounds(360 + 2 * 68,   knobY2, knobW, knobH);
    knobOutGain.setBounds(360 + 3 * 68,     knobY2, knobW, knobH);
    knobCeiling.setBounds(360 + 4 * 68,     knobY2, knobW, knobH);
}
