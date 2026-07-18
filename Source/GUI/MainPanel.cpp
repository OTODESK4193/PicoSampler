// ==========================================
// File: MainPanel.cpp
// MainPanel 実装 (下線重ね解消, 2行2列ボタン, Ceilingノブ)
// ==========================================
#include "MainPanel.h"

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

    // ADSR Link Button
    addAndMakeVisible(btnLinkEnv);
    btnLinkEnv.onClick = [this] {
        if (!isEnvLinked)
        {
            juce::AlertWindow::showOkCancelBox(
                juce::MessageBoxIconType::QuestionIcon,
                "Link Envelope Parameters",
                "Copy SLOT 1 Envelope (ADSR) parameters to all other slots and link them?",
                "Yes", "No", this,
                juce::ModalCallbackFunction::create([this](int result) {
                    if (result == 1) // Yes
                    {
                        isEnvLinked = true;
                        btnLinkEnv.setToggleState(true, juce::dontSendNotification);

                        const float a = vts.getRawParameterValue("attack_0")->load();
                        const float d = vts.getRawParameterValue("decay_0")->load();
                        const float s = vts.getRawParameterValue("sustain_0")->load();
                        const float r = vts.getRawParameterValue("release_0")->load();

                        for (int k = 1; k < 8; ++k)
                        {
                            const juce::String strKey = juce::String(k);
                            if (auto* p = vts.getParameter("attack_" + strKey)) p->setValueNotifyingHost(a / 5.0f);
                            if (auto* p = vts.getParameter("decay_" + strKey)) p->setValueNotifyingHost(d / 5.0f);
                            if (auto* p = vts.getParameter("sustain_" + strKey)) p->setValueNotifyingHost(s);
                            if (auto* p = vts.getParameter("release_" + strKey)) p->setValueNotifyingHost(r / 10.0f);
                        }
                    }
                    else // No
                    {
                        btnLinkEnv.setToggleState(false, juce::dontSendNotification);
                    }
                })
            );
        }
        else
        {
            isEnvLinked = false;
            btnLinkEnv.setToggleState(false, juce::dontSendNotification);
        }
    };

    // ADSR ノブ変更時の連動
    auto setupEnvCallback = [this](LabeledKnob& lk, const juce::String& pBaseName, float maxVal) {
        lk.knob.onValueChange = [this, pBaseName, &lk, maxVal] {
            if (isEnvLinked)
            {
                const float normVal = (float)lk.knob.getValue() / maxVal;
                for (int k = 0; k < 8; ++k)
                {
                    if (auto* p = vts.getParameter(pBaseName + "_" + juce::String(k)))
                        p->setValueNotifyingHost(normVal);
                }
            }
        };
    };

    setupEnvCallback(knobAttack,  "attack",  5.0f);
    setupEnvCallback(knobDecay,   "decay",   5.0f);
    setupEnvCallback(knobSustain, "sustain", 1.0f);
    setupEnvCallback(knobRelease, "release", 10.0f);

    // Master ノブ
    hpfAttach = std::make_unique<Attachment>(vts, "masterHPF", knobMasterHpf.knob);
    lpfAttach = std::make_unique<Attachment>(vts, "masterLPF", knobMasterLpf.knob);
    ceilingAttach = std::make_unique<Attachment>(vts, "ceiling", knobCeiling.knob);
    outGainAttach = std::make_unique<Attachment>(vts, "outGain", knobOutGain.knob);

    addAndMakeVisible(knobSampleStart);
    addAndMakeVisible(knobSampleEnd);
    addAndMakeVisible(knobLoopStart);
    addAndMakeVisible(knobLoopEnd);
    addAndMakeVisible(knobCrossfade);

    addAndMakeVisible(knobOctave);
    addAndMakeVisible(knobSemitone);
    addAndMakeVisible(knobFineTune);

    addAndMakeVisible(knobAttack);
    addAndMakeVisible(knobDecay);
    addAndMakeVisible(knobSustain);
    addAndMakeVisible(knobRelease);

    addAndMakeVisible(knobMasterHpf);
    addAndMakeVisible(knobMasterLpf);
    addAndMakeVisible(knobCeiling);
    addAndMakeVisible(knobOutGain);

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

    const juce::String s = juce::String(slotIdx);

    auto bind = [this, &s](LabeledKnob& lk, const juce::String& pName) {
        attachments.push_back(std::make_unique<Attachment>(vts, pName + "_" + s, lk.knob));
    };

    bind(knobSampleStart, "sampleStart");
    bind(knobSampleEnd,   "sampleEnd");
    bind(knobLoopStart,   "loopStart");
    bind(knobLoopEnd,     "loopEnd");
    bind(knobCrossfade,   "crossfade");

    bind(knobOctave,   "octave");
    bind(knobSemitone, "pitchSt");
    bind(knobFineTune, "fineTune");

    bind(knobAttack,  "attack");
    bind(knobDecay,   "decay");
    bind(knobSustain, "sustain");
    bind(knobRelease, "release");

    loopAttach    = std::make_unique<ButtonAttach>(vts, "isLooping_" + s, btnLoop);
    stretchAttach = std::make_unique<ButtonAttach>(vts, "isStretchMode_" + s, btnStretch);
    reverseAttach = std::make_unique<ButtonAttach>(vts, "isReverse_" + s, btnReverse);
    snapAttach    = std::make_unique<ButtonAttach>(vts, "isSnap_" + s, btnSnap);
}

void MainPanel::updateStates()
{
    const int activeSlotIdx = (int)vts.getRawParameterValue("activeSlot")->load();
    bindSlotParameters(activeSlotIdx);

    // Mode ボタンハイライト
    const int modeVal = (int)vts.getRawParameterValue("samplerMode")->load();
    auto styleModeBtn = [](juce::TextButton& b, bool active) {
        b.setColour(juce::TextButton::buttonColourId, active ? PicoColors::mint : PicoColors::knobTrack);
        b.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::black : PicoColors::textDim);
    };

    styleModeBtn(btnSingle, modeVal == 0);
    styleModeBtn(btnLayer,  modeVal == 1);
    styleModeBtn(btnRandom, modeVal == 2);

    // Slot ボタンハイライト
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
        g.fillRect(x, y + 16, w, 1); // 下線は y + 16 にクリアに配置
    };

    // セパレーター下線 (Y=26)
    drawSectionHeader("ACTIVE SLOT",   20,  10, 360, PicoColors::mint);
    drawSectionHeader("PLAYBACK MODE", 420, 10, 260, PicoColors::babyBlue);

    // 1段目: SAMPLE / LOOP, PITCH, ENVELOPE (Y=80)
    drawSectionHeader("SAMPLE / LOOP", 20,  80, 440, PicoColors::peach);
    drawSectionHeader("PITCH",         480, 80, 210, PicoColors::lavender);
    drawSectionHeader("ENVELOPE",      710, 80, 350, PicoColors::pink);

    // 2段目: MASTER (Y=206)
    drawSectionHeader("MASTER",        20,  206, 440, PicoColors::mint);
}

void MainPanel::resized()
{
    // ボタンの Y 座標を下線（Y=26）の完全に下の Y=32 に配置してクリアに分離！
    const int btnY = 32;

    for (int i = 0; i < 8; ++i)
    {
        btnSlots[(size_t)i].setBounds(115 + i * 31, btnY, 28, 22);
    }

    btnSingle.setBounds(525, btnY, 70, 22);
    btnLayer.setBounds(600, btnY, 70, 22);
    btnRandom.setBounds(675, btnY, 70, 22);

    const int knobY1 = 106;
    const int knobW = 60;
    const int knobH = 78;

    // --- 1段目 ---
    // SAMPLE / LOOP (5ノブ + 2行2列ボタン: LOOP, STRETCH, REVERSE, SNAP)
    knobSampleStart.setBounds(20 + 0 * 56, knobY1, knobW, knobH);
    knobSampleEnd.setBounds(20 + 1 * 56,   knobY1, knobW, knobH);
    knobLoopStart.setBounds(20 + 2 * 56,   knobY1, knobW, knobH);
    knobLoopEnd.setBounds(20 + 3 * 56,     knobY1, knobW, knobH);
    knobCrossfade.setBounds(20 + 4 * 56,   knobY1, knobW, knobH);

    // 2行2列 ボタン配置 (見切れ完全回避 & 余裕)
    btnLoop.setBounds(305, knobY1 + 4, 66, 24);
    btnStretch.setBounds(376, knobY1 + 4, 76, 24);
    btnReverse.setBounds(305, knobY1 + 36, 66, 24);
    btnSnap.setBounds(376, knobY1 + 36, 66, 24);

    // PITCH (3ノブ)
    knobOctave.setBounds(480 + 0 * 68,   knobY1, knobW, knobH);
    knobSemitone.setBounds(480 + 1 * 68, knobY1, knobW, knobH);
    knobFineTune.setBounds(480 + 2 * 68, knobY1, knobW, knobH);

    // ENVELOPE (4ノブ + LINK ボタン)
    knobAttack.setBounds(710 + 0 * 64,  knobY1, knobW, knobH);
    knobDecay.setBounds(710 + 1 * 64,   knobY1, knobW, knobH);
    knobSustain.setBounds(710 + 2 * 64, knobY1, knobW, knobH);
    knobRelease.setBounds(710 + 3 * 64, knobY1, knobW, knobH);

    btnLinkEnv.setBounds(975, knobY1 + 24, 56, 24);

    // --- 2段目 ---
    // MASTER (4ノブ: HPF, LPF, Ceiling, Gain)
    const int knobY2 = 232;
    knobMasterHpf.setBounds(20 + 0 * 75, knobY2, knobW, knobH);
    knobMasterLpf.setBounds(20 + 1 * 75, knobY2, knobW, knobH);
    knobCeiling.setBounds(20 + 2 * 75,   knobY2, knobW, knobH);
    knobOutGain.setBounds(20 + 3 * 75,   knobY2, knobW, knobH);
}
