// ==========================================
// File: MainPanel.cpp
// MainPanel レイアウト & モード/スロットハイライト表示
// ==========================================
#include "MainPanel.h"

MainPanel::MainPanel(juce::AudioProcessorValueTreeState& apvts) : vts(apvts)
{
    addAndMakeVisible(waveformDisplay);

    // Slot 1-8 Selection Buttons
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

    // Setup Knobs
    auto setupKnob = [this](LabeledKnob& lk, const juce::String& paramId) {
        addAndMakeVisible(lk);
        attachments.push_back(std::make_unique<Attachment>(vts, paramId, lk.knob));
    };

    setupKnob(knobSampleStart, "sampleStart_0");
    setupKnob(knobSampleEnd,   "sampleEnd_0");
    setupKnob(knobLoopStart,   "loopStart_0");
    setupKnob(knobLoopLength,  "loopLength_0");
    setupKnob(knobCrossfade,   "crossfade_0");

    setupKnob(knobOctave,   "octave_0");
    setupKnob(knobSemitone, "pitchSt_0");
    setupKnob(knobFineTune, "fineTune_0");

    setupKnob(knobAttack,  "attack_0");
    setupKnob(knobDecay,   "decay_0");
    setupKnob(knobSustain, "sustain_0");
    setupKnob(knobRelease, "release_0");

    setupKnob(knobMasterHpf, "masterHPF");
    setupKnob(knobMasterLpf, "masterLPF");
    setupKnob(knobOutGain,   "outGain");
    setupKnob(knobCeiling,   "ceiling");

    updateStates();
}

void MainPanel::updateStates()
{
    // Mode ボタンのアクティブハイライト処理
    const int modeVal = (int)vts.getRawParameterValue("samplerMode")->load();

    auto styleModeBtn = [](juce::TextButton& b, bool active) {
        b.setColour(juce::TextButton::buttonColourId, active ? PicoColors::mint : PicoColors::knobTrack);
        b.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::black : juce::Colours::white);
    };

    styleModeBtn(btnSingle, modeVal == 0);
    styleModeBtn(btnLayer,  modeVal == 1);
    styleModeBtn(btnRandom, modeVal == 2);

    // Slot ボタンのアクティブハイライト処理
    const int activeSlotIdx = (int)vts.getRawParameterValue("activeSlot")->load();
    for (int i = 0; i < 8; ++i)
    {
        const bool isAct = (i == activeSlotIdx);
        const auto col = PicoColors::getSlotColor(i);
        btnSlots[(size_t)i].setColour(juce::TextButton::buttonColourId, isAct ? col : PicoColors::knobTrack);
        btnSlots[(size_t)i].setColour(juce::TextButton::textColourOffId, isAct ? juce::Colours::black : juce::Colours::white);
    }
}

void MainPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    // セクション背景カード描画
    auto drawCard = [&g](int x, int y, int w, int h, const juce::String& title)
    {
        g.setColour(PicoColors::panel);
        g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 6.0f);
        g.setColour(PicoColors::knobTrack);
        g.drawRoundedRectangle((float)x, (float)y, (float)w, (float)h, 6.0f, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(title, x + 10, y + 8, w - 20, 16, juce::Justification::left);
    };

    // Slot & Mode Header Cards
    drawCard(20, 205, 360, 45, "ACTIVE SLOT");
    drawCard(390, 205, 270, 45, "PLAYBACK MODE");

    // Knobs Section Cards
    drawCard(20, 260, 360, 115, "SAMPLE / LOOP");
    drawCard(390, 260, 220, 115, "PITCH");
    drawCard(620, 260, 250, 115, "ENVELOPE");
    drawCard(880, 260, 180, 115, "MASTER");
}

void MainPanel::resized()
{
    waveformDisplay.setBounds(20, 15, 1040, 180);

    // Slot 1-8 Buttons
    for (int i = 0; i < 8; ++i)
    {
        btnSlots[(size_t)i].setBounds(100 + i * 34, 215, 30, 26);
    }

    // Mode Buttons
    btnSingle.setBounds(495, 215, 75, 26);
    btnLayer.setBounds(575, 215, 75, 26);
    btnRandom.setBounds(655, 215, 75, 26);

    const int knobY = 290;
    const int knobSize = 52;
    const int knobCompH = 72; // ノブ52px + 余白2px + ラベル16px = 70px

    // Sample Row
    knobSampleStart.setBounds(30,  knobY, knobSize, knobCompH);
    knobSampleEnd.setBounds(95,   knobY, knobSize, knobCompH);
    knobLoopStart.setBounds(160,  knobY, knobSize, knobCompH);
    knobLoopLength.setBounds(225, knobY, knobSize, knobCompH);
    knobCrossfade.setBounds(290,  knobY, knobSize, knobCompH);

    // Pitch Row
    knobOctave.setBounds(405,   knobY, knobSize, knobCompH);
    knobSemitone.setBounds(470, knobY, knobSize, knobCompH);
    knobFineTune.setBounds(535, knobY, knobSize, knobCompH);

    // Env Row
    knobAttack.setBounds(635,  knobY, knobSize, knobCompH);
    knobDecay.setBounds(700,   knobY, knobSize, knobCompH);
    knobSustain.setBounds(765, knobY, knobSize, knobCompH);
    knobRelease.setBounds(830, knobY, knobSize, knobCompH);

    // Master
    knobMasterHpf.setBounds(895,  knobY, knobSize, knobCompH);
    knobMasterLpf.setBounds(960,  knobY, knobSize, knobCompH);
    knobOutGain.setBounds(1025,   knobY, knobSize, knobCompH);
}
