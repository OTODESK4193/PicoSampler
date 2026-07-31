// ==========================================
// File: MainPanel.h
// メイン操作パネル (RootKeyノブをPITCHエリアへ移設 & 表示カスタマイズ)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ArcDial.h"
#include "ValueKnob.h"
#include "GlowToggle.h"

class MainPanel : public juce::Component
{
public:
    MainPanel(juce::AudioProcessorValueTreeState& apvts);
    ~MainPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateStates();
    void bindSlotParameters(int slotIdx);

    ValueKnob& getSampleStartKnob() { return knobSampleStart.knob; }
    ValueKnob& getSampleEndKnob()   { return knobSampleEnd.knob; }
    ValueKnob& getLoopStartKnob()   { return knobLoopStart.knob; }
    ValueKnob& getLoopEndKnob()     { return knobLoopEnd.knob; }
    ValueKnob& getCrossfadeKnob()   { return knobCrossfade.knob; }
    ValueKnob& getSlotPanKnob()     { return knobSlotPan.knob; }
    ValueKnob& getSlotVolumeKnob()  { return knobSlotVolume.knob; }
    ValueKnob& getMasterPitchKnob() { return knobMasterPitch.knob; }

    // Amp ADSR (現在バインド中のスロットの値)。MODアサイン先の変調表示用に公開。
    ValueKnob& getAttackKnob()  { return knobAttack.knob; }
    ValueKnob& getDecayKnob()   { return knobDecay.knob; }
    ValueKnob& getSustainKnob() { return knobSustain.knob; }
    ValueKnob& getReleaseKnob() { return knobRelease.knob; }

    int getCurrentBoundSlot() const noexcept { return currentBoundSlot; }

    // 束縛済みフラグだけを落とす。次の updateStates() で貼り直される。
    // bindSlotParameters(-1) を呼ぶと存在しないパラメータID ("sampleStart_-1")
    // にアタッチしにいってしまうため、必ずこちらを使うこと。
    void invalidateBinding() noexcept { currentBoundSlot = -1; }

private:
    // ---- 1段目のレイアウト定数 (paint と resized で共有) ----
    static constexpr int kNumSampleKnobs = 7;  // Start,End,L-Start,L-End,X-Fade,Pan,Volume
    static constexpr int kSampleX    = 20;
    static constexpr int kSampleStep = 62;
    static constexpr int kToggleX    = 462;   // Loop/Stretch/Reverse/Snap の列
    static constexpr int kEnvX       = 630;   // ENVELOPE ブロック開始X
    static constexpr int kEnvStep    = 78;

    struct LabeledKnob : public juce::Component
    {
        ValueKnob knob;
        juce::Label label;

        LabeledKnob(const juce::String& name, juce::Colour accent = PicoColors::mint)
        {
            knob.setColour(juce::Slider::rotarySliderFillColourId, accent);
            label.setText(name, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            label.setColour(juce::Label::textColourId, PicoColors::textDim);
            addAndMakeVisible(knob);
            addAndMakeVisible(label);
        }

        void resized() override
        {
            auto area = getLocalBounds();
            label.setBounds(area.removeFromBottom(16));
            knob.setBounds(area);
        }
    };

    juce::AudioProcessorValueTreeState& vts;

    // Slot 1-8 Selection Buttons
    std::array<juce::TextButton, 8> btnSlots;

    // Mode Toggle Buttons
    juce::TextButton btnSingle { "SINGLE" };
    juce::TextButton btnLayer  { "LAYER" };
    juce::TextButton btnRandom { "RANDOM" };

    // Routing Toggles
    GlowToggle btnFltBypass { "FLT BYPASS", PicoColors::rose };
    GlowToggle btnFxBypass  { "FX BYPASS",  PicoColors::peach };

    // Loop, Stretch, Reverse & Snap Toggles (2列2行)
    GlowToggle btnLoop    { "LOOP" };
    GlowToggle btnStretch { "STRETCH" };
    GlowToggle btnReverse { "REVERSE" };
    GlowToggle btnSnap    { "SNAP" };

    // ADSR Link Toggle
    GlowToggle btnLinkEnv { "LINK" };
    bool isEnvLinked = false;
    bool isUpdatingFromLink = false;

    // Labeled Knobs
    LabeledKnob knobSampleStart { "Start",   PicoColors::peach };
    LabeledKnob knobSampleEnd   { "End",     PicoColors::peach };
    LabeledKnob knobLoopStart   { "L-Start", PicoColors::peach };
    LabeledKnob knobLoopEnd     { "L-End",   PicoColors::peach };
    LabeledKnob knobCrossfade   { "X-Fade",  PicoColors::peach };
    LabeledKnob knobSlotPan     { "Pan",     PicoColors::peach };  // スロット毎の定位 (pan_)
    LabeledKnob knobSlotVolume  { "Volume",  PicoColors::peach };  // スロット毎の音量 (slotGain_)

    // PITCH エリア (Root, Octave, Semi, Fine)
    LabeledKnob knobRootKey   { "Root",   PicoColors::lavender };
    LabeledKnob knobOctave    { "Octave", PicoColors::lavender };
    LabeledKnob knobSemitone  { "Semi",   PicoColors::lavender };
    LabeledKnob knobFineTune  { "Fine",   PicoColors::lavender };

    LabeledKnob knobAttack   { "Attack",  PicoColors::pink };
    LabeledKnob knobDecay    { "Decay",   PicoColors::pink };
    LabeledKnob knobSustain  { "Sustain", PicoColors::pink };
    LabeledKnob knobRelease  { "Release", PicoColors::pink };

    LabeledKnob knobMasterPitch { "Pitch",   PicoColors::mint };
    LabeledKnob knobMasterHpf   { "HPF",     PicoColors::mint };
    LabeledKnob knobMasterLpf   { "LPF",     PicoColors::mint };
    LabeledKnob knobOutGain     { "Gain",    PicoColors::mint };
    LabeledKnob knobCeiling     { "Ceiling", PicoColors::mint };

private:
    int currentBoundSlot = -1;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<Attachment>> attachments;
    std::unique_ptr<ButtonAttach> loopAttach;
    std::unique_ptr<ButtonAttach> stretchAttach;
    std::unique_ptr<ButtonAttach> reverseAttach;
    std::unique_ptr<ButtonAttach> snapAttach;
    std::unique_ptr<ButtonAttach> fltBypassAttach;
    std::unique_ptr<ButtonAttach> fxBypassAttach;

    std::unique_ptr<Attachment> masterPitchAttach;
    std::unique_ptr<Attachment> masterHpfAttach;
    std::unique_ptr<Attachment> masterLpfAttach;
    std::unique_ptr<Attachment> outGainAttach;
    std::unique_ptr<Attachment> ceilingAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};
