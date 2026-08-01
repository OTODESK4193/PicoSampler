// ==========================================
// File: ModPanel.cpp
// ModPanel 実装 (Granularベース・Macro除外・ArpスタイルCombo統一)
// ==========================================
#include "ModPanel.h"

namespace
{
    constexpr int kRowH       = 28;
    constexpr int kMatrixX    = 410;
    constexpr int kSrcX       = 16;
    constexpr int kSrcW       = 380;
    constexpr int kContentTop = 56;
    constexpr int kRowsPerPage= 8;

    inline int lfoRowY(int i)   { return kContentTop + 8 + i * 56; }
    inline int envBlockY(int i) { return kContentTop + 2 + i * 76; }
    inline int envKnobX(int k)  { return 22 + k * 58; }
    constexpr int kMatrixTop  = 56;
}

ModPanel::ModPanel(PicoSamplerAudioProcessor& processor)
    : procPtr(&processor), vts(processor.getAPVTS())
{
    const auto lav   = PicoColors::lavender;
    const auto peach = PicoColors::peach;

    // --- ソース・サブタブ ---
    for (auto* b : { &lfoTabBtn, &envTabBtn, &page1Btn, &page2Btn })
    {
        b->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(*b);
    }
    lfoTabBtn.onClick = [this] { setSourceTab(0); };
    envTabBtn.onClick = [this] { setSourceTab(1); };
    page1Btn.onClick  = [this] { setMatrixPage(0); };
    page2Btn.onClick  = [this] { setMatrixPage(1); };

    // --- LFO ×4 ---
    for (int i = 0; i < ModMatrix::kNumLfos; ++i)
    {
        const juce::String idx(i + 1);
        lfoWaveBox[(size_t)i].addItemList(ModMatrix::getWaveNames(), 1);
        setupCombo(lfoWaveBox[(size_t)i], "lfo" + idx + "Wave");

        lfoSyncRateBox[(size_t)i].addItemList(ModMatrix::getSyncRateNames(), 1);
        setupCombo(lfoSyncRateBox[(size_t)i], "lfo" + idx + "SyncRate");

        setupKnob(lfoRateKnob[(size_t)i], "lfo" + idx + "Rate", lav);

        lfoSyncButton[(size_t)i] = std::make_unique<GlowToggle>("SYNC", PicoColors::babyBlue);
        addAndMakeVisible(*lfoSyncButton[(size_t)i]);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            vts, "lfo" + idx + "Sync", *lfoSyncButton[(size_t)i]));
    }

    // --- ENV ×3 (ADSR) ---
    for (int i = 0; i < ModMatrix::kNumEnvs; ++i)
    {
        const juce::String idx(i + 1);
        setupKnob(envAttackKnob[(size_t)i],  "env" + idx + "Attack",  peach);
        setupKnob(envDecayKnob[(size_t)i],   "env" + idx + "Decay",   peach);
        setupKnob(envSustainKnob[(size_t)i], "env" + idx + "Sustain", peach);
        setupKnob(envReleaseKnob[(size_t)i], "env" + idx + "Release", peach);

        envLoopButton[(size_t)i] = std::make_unique<GlowToggle>("LOOP", PicoColors::lilac);
        addAndMakeVisible(*envLoopButton[(size_t)i]);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            vts, "env" + idx + "Loop", *envLoopButton[(size_t)i]));
    }

    // --- マトリクス16行 ---
    const auto srcNames = ModMatrix::getSourceNames();

    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        const juce::String idx(i + 1);

        rowLabel[(size_t)i].setText(juce::String(i + 1), juce::dontSendNotification);
        rowLabel[(size_t)i].setJustificationType(juce::Justification::centred);
        rowLabel[(size_t)i].setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        rowLabel[(size_t)i].setColour(juce::Label::textColourId, PicoColors::textDim);
        addAndMakeVisible(rowLabel[(size_t)i]);

        srcBox[(size_t)i].addItemList(srcNames, 1);
        setupCombo(srcBox[(size_t)i], "mod" + idx + "Src");

        addAndMakeVisible(dstBox[(size_t)i]);
        dstBox[(size_t)i].buildMenu = [this](int currentDst) { return buildDestMenu(currentDst); };
        dstBox[(size_t)i].bindTo(vts, "mod" + idx + "Dst");

        uniButton[(size_t)i] = std::make_unique<GlowToggle>("UNI", PicoColors::babyBlue);
        addAndMakeVisible(*uniButton[(size_t)i]);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            vts, "mod" + idx + "Uni", *uniButton[(size_t)i]));

        auto& amt = amtSlider[(size_t)i];
        amt.setSliderStyle(juce::Slider::LinearHorizontal);
        amt.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        amt.setColour(juce::Slider::trackColourId, PicoColors::lavender.withAlpha(0.55f));
        amt.setColour(juce::Slider::backgroundColourId, PicoColors::knobTrack);
        amt.setColour(juce::Slider::thumbColourId, PicoColors::text);
        amt.setPopupDisplayEnabled(true, true, this);
        amt.setDoubleClickReturnValue(true, 0.0);
        addAndMakeVisible(amt);
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, "mod" + idx + "Amt", amt));
    }

    setSourceTab(0);
    setMatrixPage(0);
}

void ModPanel::setupKnob(ValueKnob& s, const juce::String& paramID, juce::Colour accent)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setColour(juce::Slider::rotarySliderFillColourId, accent);
    s.setPopupDisplayEnabled(true, true, this);
    addAndMakeVisible(s);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        vts, paramID, s));
}

void ModPanel::setupCombo(juce::ComboBox& c, const juce::String& paramID)
{
    // ARPタブと同等のコンボボックス形状・カラー設定
    c.setColour(juce::ComboBox::backgroundColourId, PicoColors::panel);
    c.setColour(juce::ComboBox::outlineColourId, PicoColors::knobTrack);
    c.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    c.setColour(juce::ComboBox::arrowColourId, PicoColors::mint);

    addAndMakeVisible(c);
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        vts, paramID, c));
}

juce::PopupMenu ModPanel::buildDestMenu(int currentDst) const
{
    const auto names = ModMatrix::getDestNames();
    juce::PopupMenu menu;

    // itemID は Dst値+1 (0はPopupMenuの「選択せず閉じた」を表す予約値のため)
    // 現在選択中の項目にはチェックを付ける (どのサブメニューに居るか分かるように)
    auto addItem = [&](juce::PopupMenu& m, int dst)
    {
        if (dst >= 0 && dst < names.size())
            m.addItem(dst + 1, names[dst], true, dst == currentDst);
    };

    menu.addItem((int)ModMatrix::DstNone + 1, "None", true, currentDst == (int)ModMatrix::DstNone);

    for (int slot = 0; slot < 8; ++slot)
    {
        juce::PopupMenu sub;
        const int base = (int)ModMatrix::DstS1Start + slot * 5;
        addItem(sub, base + 0); // Start
        addItem(sub, base + 1); // End
        addItem(sub, base + 2); // L-Start
        addItem(sub, base + 3); // L-End
        addItem(sub, base + 4); // X-Fade
        addItem(sub, (int)ModMatrix::DstS1Pan + slot); // Pan (末尾に離れて配置されている)

        const int ampBase = (int)ModMatrix::DstS1AmpAttack + slot * 4;
        addItem(sub, ampBase + 0); // Amp Attack
        addItem(sub, ampBase + 1); // Amp Decay
        addItem(sub, ampBase + 2); // Amp Sustain
        addItem(sub, ampBase + 3); // Amp Release

        menu.addSubMenu("S" + juce::String(slot + 1), sub);
    }

    {
        // ユーザー指定のカテゴリ一覧には無いが、Master Pitch の行き場が無くなるため
        // 独立カテゴリとして追加。
        juce::PopupMenu sub;
        addItem(sub, (int)ModMatrix::DstMasterPitch);
        menu.addSubMenu("Master", sub);
    }

    {
        juce::PopupMenu sub;
        for (int d = (int)ModMatrix::DstArpOctaves; d <= (int)ModMatrix::DstArpAccent; ++d) addItem(sub, d);
        menu.addSubMenu("ARP", sub);
    }

    {
        juce::PopupMenu sub;
        for (int d = (int)ModMatrix::DstFltCutoff; d <= (int)ModMatrix::DstFltCombMix; ++d) addItem(sub, d);
        for (int d = (int)ModMatrix::DstFltEnvAttack; d <= (int)ModMatrix::DstFltEnvRelease; ++d) addItem(sub, d);
        menu.addSubMenu("Filter", sub);
    }

    {
        juce::PopupMenu sub;
        for (int d = (int)ModMatrix::DstLfo1Rate; d <= (int)ModMatrix::DstLfo4Rate; ++d) addItem(sub, d);
        menu.addSubMenu("LFO", sub);
    }

    {
        juce::PopupMenu sub;
        for (int d = (int)ModMatrix::DstFx1Amount; d <= (int)ModMatrix::DstFx5Amount; ++d) addItem(sub, d);
        menu.addSubMenu("FX", sub);
    }

    {
        juce::PopupMenu sub;
        for (int d = (int)ModMatrix::DstSatDrive; d <= (int)ModMatrix::DstSatTrim; ++d) addItem(sub, d);
        menu.addSubMenu("Sat", sub);
    }

    {
        juce::PopupMenu sub;
        for (int d = (int)ModMatrix::DstChoRate; d <= (int)ModMatrix::DstChoWidth; ++d) addItem(sub, d);
        menu.addSubMenu("Chorus", sub);
    }

    {
        juce::PopupMenu sub;
        for (int d = (int)ModMatrix::DstDlyFeedback; d <= (int)ModMatrix::DstDlyDamp; ++d) addItem(sub, d);
        menu.addSubMenu("Delay", sub);
    }

    {
        juce::PopupMenu sub;
        for (int d = (int)ModMatrix::DstRevDecay; d <= (int)ModMatrix::DstRevMod; ++d) addItem(sub, d);
        menu.addSubMenu("Reverb", sub);
    }

    {
        juce::PopupMenu sub;
        for (int d = (int)ModMatrix::DstFrzSize; d <= (int)ModMatrix::DstFrzDamp; ++d) addItem(sub, d);
        menu.addSubMenu("Freeze", sub);
    }

    return menu;
}

void ModPanel::styleTab(juce::TextButton& b, bool active)
{
    b.setColour(juce::TextButton::textColourOffId,
                active ? PicoColors::text : PicoColors::textDim);
}

void ModPanel::setSourceTab(int t)
{
    activeSrcTab = t;
    const bool L = (t == 0), E = (t == 1);

    for (int i = 0; i < ModMatrix::kNumLfos; ++i)
    {
        lfoWaveBox[(size_t)i].setVisible(L);
        lfoSyncButton[(size_t)i]->setVisible(L);
        lfoRateKnob[(size_t)i].setVisible(L);
        lfoSyncRateBox[(size_t)i].setVisible(L);
    }
    for (int i = 0; i < ModMatrix::kNumEnvs; ++i)
    {
        envAttackKnob[(size_t)i].setVisible(E);
        envDecayKnob[(size_t)i].setVisible(E);
        envSustainKnob[(size_t)i].setVisible(E);
        envReleaseKnob[(size_t)i].setVisible(E);
        envLoopButton[(size_t)i]->setVisible(E);
    }

    styleTab(lfoTabBtn, L);
    styleTab(envTabBtn, E);
    repaint();
}

void ModPanel::setMatrixPage(int pg)
{
    matrixPage = pg;
    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        const bool vis = (i / kRowsPerPage) == pg;
        rowLabel[(size_t)i].setVisible(vis);
        srcBox[(size_t)i].setVisible(vis);
        dstBox[(size_t)i].setVisible(vis);
        uniButton[(size_t)i]->setVisible(vis);
        amtSlider[(size_t)i].setVisible(vis);
    }
    styleTab(page1Btn, pg == 0);
    styleTab(page2Btn, pg == 1);
    repaint();
}

void ModPanel::paint(juce::Graphics& g)
{
    g.fillAll(PicoColors::bgDk);

    auto drawSection = [&g](const juce::String& name, int x, int y, int w, juce::Colour accent)
    {
        g.setColour(accent);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(name, x, y, w, 14, juce::Justification::centredLeft);
        g.setColour(accent.withAlpha(0.35f));
        g.fillRect(x, y + 16, w, 1);
    };

    drawSection("SOURCES", 20, 6, kSrcW, PicoColors::lavender);
    drawSection("MATRIX  (Source x Amount -> Destination, dbl-click=0, UNI=unipolar)",
                kMatrixX, 6, getWidth() - kMatrixX - 16, PicoColors::mint);

    auto underline = [&g](const juce::TextButton& b, bool active, juce::Colour c)
    {
        if (!active) return;
        const auto r = b.getBounds();
        g.setColour(c);
        g.fillRoundedRectangle((float)r.getX() + 6.0f, (float)r.getBottom() - 1.0f,
                               (float)r.getWidth() - 12.0f, 2.0f, 1.0f);
    };
    underline(lfoTabBtn, activeSrcTab == 0, PicoColors::lavender);
    underline(envTabBtn, activeSrcTab == 1, PicoColors::lavender);
    underline(page1Btn,  matrixPage == 0,   PicoColors::mint);
    underline(page2Btn,  matrixPage == 1,   PicoColors::mint);

    g.setColour(PicoColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));

    if (activeSrcTab == 0)
    {
        for (int i = 0; i < ModMatrix::kNumLfos; ++i)
            g.drawText("LFO" + juce::String(i + 1), kSrcX + 4, lfoRowY(i) + 4, 42, 24,
                       juce::Justification::centredLeft);
    }
    else if (activeSrcTab == 1)
    {
        static const char* adsr[4] = { "A", "D", "S", "R" };
        for (int i = 0; i < ModMatrix::kNumEnvs; ++i)
        {
            const int yb = envBlockY(i);
            g.setColour(PicoColors::peach);
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            g.drawText("ENV" + juce::String(i + 1), 20, yb, 80, 14, juce::Justification::centredLeft);

            g.setColour(PicoColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
            for (int k = 0; k < 4; ++k)
                g.drawText(adsr[k], envKnobX(k), yb + 16 + 44, 44, 11, juce::Justification::centred);
        }
    }
}

void ModPanel::resized()
{
    lfoTabBtn.setBounds(20, 26, 72, 24);
    envTabBtn.setBounds(96, 26, 72, 24);

    page1Btn.setBounds(kMatrixX, 28, 90, 22);
    page2Btn.setBounds(kMatrixX + 96, 28, 90, 22);

    for (int i = 0; i < ModMatrix::kNumLfos; ++i)
    {
        const int y = lfoRowY(i);
        lfoWaveBox[(size_t)i].setBounds(64, y, 96, 26);
        lfoSyncButton[(size_t)i]->setBounds(168, y, 58, 26);
        lfoRateKnob[(size_t)i].setBounds(236, y - 9, 44, 44);
        lfoSyncRateBox[(size_t)i].setBounds(288, y, 100, 26);
    }

    for (int i = 0; i < ModMatrix::kNumEnvs; ++i)
    {
        const int yb = envBlockY(i);
        const int ky = yb + 16;
        envAttackKnob[(size_t)i].setBounds(envKnobX(0), ky, 44, 44);
        envDecayKnob[(size_t)i].setBounds(envKnobX(1), ky, 44, 44);
        envSustainKnob[(size_t)i].setBounds(envKnobX(2), ky, 44, 44);
        envReleaseKnob[(size_t)i].setBounds(envKnobX(3), ky, 44, 44);
        envLoopButton[(size_t)i]->setBounds(envKnobX(3) + 52, ky + 9, 70, 26);
    }

    const int amtX = kMatrixX + 308;
    const int amtW = getWidth() - 16 - amtX;
    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        const int row = i % kRowsPerPage;
        const int y = kMatrixTop + row * kRowH;
        rowLabel[(size_t)i].setBounds(kMatrixX, y, 20, 24);
        srcBox[(size_t)i].setBounds(kMatrixX + 24, y, 110, 24);
        dstBox[(size_t)i].setBounds(kMatrixX + 138, y, 116, 24);
        uniButton[(size_t)i]->setBounds(kMatrixX + 258, y, 46, 24);
        amtSlider[(size_t)i].setBounds(amtX, y, amtW, 24);
    }
}
