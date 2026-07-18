// ==========================================
// File: MainPanel.cpp
// MainPanel 実装 (Granular完全準拠 ＆ 見切れゼロ配置)
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

    // Master ノブを常時バインド
    hpfAttach = std::make_unique<Attachment>(vts, "masterHPF", knobMasterHpf.knob);
    lpfAttach = std::make_unique<Attachment>(vts, "masterLPF", knobMasterLpf.knob);
    outGainAttach = std::make_unique<Attachment>(vts, "outGain", knobOutGain.knob);

    addAndMakeVisible(knobSampleStart);
    addAndMakeVisible(knobSampleEnd);
    addAndMakeVisible(knobLoopStart);
    addAndMakeVisible(knobLoopLength);
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
    addAndMakeVisible(knobOutGain);

    bindSlotParameters(0);
    updateStates();
}

void MainPanel::bindSlotParameters(int slotIdx)
{
    if (currentBoundSlot == slotIdx) return;
    currentBoundSlot = slotIdx;

    attachments.clear();
    const juce::String s = juce::String(slotIdx);

    auto bind = [this, &s](LabeledKnob& lk, const juce::String& pName) {
        attachments.push_back(std::make_unique<Attachment>(vts, pName + "_" + s, lk.knob));
    };

    bind(knobSampleStart, "sampleStart");
    bind(knobSampleEnd,   "sampleEnd");
    bind(knobLoopStart,   "loopStart");
    bind(knobLoopLength,  "loopLength");
    bind(knobCrossfade,   "crossfade");

    bind(knobOctave,   "octave");
    bind(knobSemitone, "pitchSt");
    bind(knobFineTune, "fineTune");

    bind(knobAttack,  "attack");
    bind(knobDecay,   "decay");
    bind(knobSustain, "sustain");
    bind(knobRelease, "release");
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
        g.fillRect(x, y + 16, w, 1);
    };

    drawSectionHeader("ACTIVE SLOT",   20,  10, 340, PicoColors::mint);
    drawSectionHeader("PLAYBACK MODE", 380, 10, 260, PicoColors::babyBlue);

    drawSectionHeader("SAMPLE / LOOP", 20,  70, 340, PicoColors::peach);
    drawSectionHeader("PITCH",         380, 70, 210, PicoColors::lavender);
    drawSectionHeader("ENVELOPE",      610, 70, 270, PicoColors::pink);
    drawSectionHeader("MASTER",        890, 70, 170, PicoColors::mint);
}

void MainPanel::resized()
{
    // Slot 1-8 Buttons (X=105..353, Y=10)
    for (int i = 0; i < 8; ++i)
    {
        btnSlots[(size_t)i].setBounds(105 + i * 31, 6, 28, 22);
    }

    // Mode Buttons (X=500..635, Y=10)
    btnSingle.setBounds(495, 6, 75, 22);
    btnLayer.setBounds(575, 6, 75, 22);
    btnRandom.setBounds(655, 6, 75, 22);

    const int knobY = 96;
    const int knobW = 60;
    const int knobH = 78;

    // SAMPLE / LOOP (5ノブ, X=20..340)
    knobSampleStart.setBounds(20 + 0 * 65, knobY, knobW, knobH);
    knobSampleEnd.setBounds(20 + 1 * 65,   knobY, knobW, knobH);
    knobLoopStart.setBounds(20 + 2 * 65,   knobY, knobW, knobH);
    knobLoopLength.setBounds(20 + 3 * 65,  knobY, knobW, knobH);
    knobCrossfade.setBounds(20 + 4 * 65,   knobY, knobW, knobH);

    // PITCH (3ノブ, X=380..590)
    knobOctave.setBounds(380 + 0 * 68,   knobY, knobW, knobH);
    knobSemitone.setBounds(380 + 1 * 68, knobY, knobW, knobH);
    knobFineTune.setBounds(380 + 2 * 68, knobY, knobW, knobH);

    // ENVELOPE (4ノブ, X=610..880)
    knobAttack.setBounds(610 + 0 * 66,  knobY, knobW, knobH);
    knobDecay.setBounds(610 + 1 * 66,   knobY, knobW, knobH);
    knobSustain.setBounds(610 + 2 * 66, knobY, knobW, knobH);
    knobRelease.setBounds(610 + 3 * 66, knobY, knobW, knobH);

    // MASTER (3ノブ, X=890..1060 <= 1080)
    knobMasterHpf.setBounds(890 + 0 * 56, knobY, knobW, knobH);
    knobMasterLpf.setBounds(890 + 1 * 56, knobY, knobW, knobH);
    knobOutGain.setBounds(890 + 2 * 56,   knobY, knobW, knobH);
}
