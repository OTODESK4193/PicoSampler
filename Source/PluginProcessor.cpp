// ==========================================
// File: PluginProcessor.cpp
// PicoSampler メインプロセッサ実装 (safe APVTS パラメーターガード & reanalyzeSlot 対応)
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

PicoSamplerAudioProcessor::PicoSamplerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      Thread("PicoSampleLoaderThread"),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    samplerEngine.prepare(44100.0);

    samplerEngine.onActiveSlotTriggered = [this](int newActiveSlot) {
        if (auto* p = apvts.getParameter("activeSlot"))
            p->setValueNotifyingHost((float)juce::jlimit(0, 7, newActiveSlot) / 7.0f);
    };

    startThread();
}

PicoSamplerAudioProcessor::~PicoSamplerAudioProcessor()
{
    stopThread(4000);
}

float PicoSamplerAudioProcessor::getParamFloat(const juce::String& paramId, float defaultVal) const
{
    if (auto* p = apvts.getRawParameterValue(paramId))
        return p->load();
    return defaultVal;
}

juce::AudioProcessorValueTreeState::ParameterLayout PicoSamplerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global Parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>("samplerMode", "Sampler Mode", juce::StringArray{ "Single", "Layer", "Random" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("activeSlot", "Active Slot", 0, 7, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", -36.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterPitch", "Master Pitch", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterHPF", "Master HPF", 20.0f, 2000.0f, 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterLPF", "Master LPF", 200.0f, 20000.0f, 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Limiter Ceiling", -12.0f, 0.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("limRelease", "Limiter Release", 1.0f, 500.0f, 50.0f));

    // Config / Global Settings
    params.push_back(std::make_unique<juce::AudioParameterChoice>("analysisEngine", "Analysis Engine", juce::StringArray{ "Auto", "Crisp", "Smooth", "Formant" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("filterSlope", "Filter Slope", juce::StringArray{ "12dB/oct", "24dB/oct" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("colorTheme", "Color Theme", juce::StringArray{ "Midnight", "Sakura", "Ocean", "Forest", "Sunset", "Mono" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("polyphony", "Polyphony", juce::StringArray{ "1 Voice", "2 Voices", "4 Voices", "8 Voices", "16 Voices", "32 Voices" }, 4));

    // Slots 1-8 Parameters
    for (int i = 0; i < 8; ++i)
    {
        const juce::String s = juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterFloat>("attack_" + s, "Attack " + s, juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.3f), 0.01f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("decay_" + s, "Decay " + s, juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.3f), 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("sustain_" + s, "Sustain " + s, juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("release_" + s, "Release " + s, juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));

        params.push_back(std::make_unique<juce::AudioParameterInt>("octave_" + s, "Octave " + s, -3, 3, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>("pitchSt_" + s, "Pitch Semi " + s, -24, 24, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("fineTune_" + s, "Fine Tune " + s, juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("pan_" + s, "Pan " + s, juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("slotGain_" + s, "Slot Gain " + s, juce::NormalisableRange<float>(-36.0f, 12.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>("sampleStart_" + s, "Start " + s, 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("sampleEnd_" + s, "End " + s, 0.0f, 1.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("loopStart_" + s, "Loop Start " + s, 0.0f, 1.0f, 0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("loopEnd_" + s, "Loop End " + s, 0.0f, 1.0f, 0.7f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("crossfade_" + s, "Crossfade " + s, 0.0f, 0.5f, 0.05f));
        params.push_back(std::make_unique<juce::AudioParameterBool>("isLooping_" + s, "Looping " + s, false));
        params.push_back(std::make_unique<juce::AudioParameterBool>("isStretchMode_" + s, "Stretch Mode " + s, false));
        params.push_back(std::make_unique<juce::AudioParameterBool>("isReverse_" + s, "Reverse " + s, false));
        params.push_back(std::make_unique<juce::AudioParameterBool>("isSnap_" + s, "Snap " + s, true));
        params.push_back(std::make_unique<juce::AudioParameterBool>("filterBypass_" + s, "Filter Bypass " + s, false));
        params.push_back(std::make_unique<juce::AudioParameterBool>("fxBypass_" + s, "FX Bypass " + s, false));

        // RootKey 手動オーバーライド (-1 = Auto, 0-127 = 手動)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            "rootKey_" + s, "Root Key " + s, -1, 127, -1,
            juce::AudioParameterIntAttributes().withStringFromValueFunction(
                [](int val, int) {
                    return (val < 0) ? juce::String("Auto") : juce::MidiMessage::getMidiNoteName(val, true, true, 4);
                }
            )
        ));

        params.push_back(std::make_unique<juce::AudioParameterInt>("slotLowNote_" + s, "Low Note " + s, 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>("slotHighNote_" + s, "High Note " + s, 0, 127, 127));
    }

    // Arpeggiator
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpEnable", "Arp Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpLatch", "Arp Latch", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpSync", "Arp Sync", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpPattern", "Arp Pattern", Arpeggiator::getPatternNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpRateSync", "Arp Rate Sync", Arpeggiator::getSyncRateNames(), 6));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("arpRateFree", "Arp Rate Free", 1.0f, 30.0f, 8.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("arpOctaves", "Arp Octaves", 1, 4, 1));
    params.push_back(std::make_unique<juce::AudioParameterInt>("arpOffset", "Arp Offset", -12, 12, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("arpRepeat", "Arp Repeat", 1, 4, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("arpAccent", "Arp Accent", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("arpSwing", "Arp Swing", 0.0f, 0.75f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("arpGate", "Arp Gate", 0.1f, 1.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("key", "Root Key", juce::StringArray{ "Auto", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("scale", "Scale", ScaleQuantizer::getScaleNames(), 0));

    // PicoFilter (CleanSVF, Vowel, Comb)
    params.push_back(std::make_unique<juce::AudioParameterBool>("fltEnable", "Filter Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("fltModel", "Filter Model", juce::StringArray{ "Clean SVF", "Vowel Formant", "Comb Filter" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fltCutoff", "Filter Cutoff", 20.0f, 20000.0f, 2000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fltRes", "Filter Resonance", 0.1f, 10.0f, 0.707f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("fltType", "Filter Type", juce::StringArray{ "LowPass", "BandPass", "HighPass", "Notch" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("fltSlope", "Filter Slope", juce::StringArray{ "12dB/oct", "24dB/oct" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fltFormant", "Filter Formant", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fltCombMix", "Filter Comb Mix", 0.0f, 1.0f, 0.5f));

    // Filter Envelope
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fltEnvAttack",  "Filter Env Attack",  juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.3f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fltEnvDecay",   "Filter Env Decay",   juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.3f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fltEnvSustain", "Filter Env Sustain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fltEnvRelease", "Filter Env Release", juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fltEnvAmt",     "Filter Env Amount",  juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    // FX 5スロット
    for (int i = 1; i <= 5; ++i)
    {
        const juce::String s = juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterChoice>("fx" + s + "Type", "FX" + s + " Type", FxChain::getTypeNames(), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("fx" + s + "Amount", "FX" + s + " Amount", 0.0f, 1.0f, 0.0f));
    }

    // FX 詳細パラメータ
    params.push_back(std::make_unique<juce::AudioParameterChoice>("satAlgo", "Sat Algo", FxChain::getSatAlgoNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satDrive", "Sat Drive", juce::NormalisableRange<float>(1.0f, 12.0f, 0.01f, 0.5f), 2.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satPreHz", "Sat Pre-HPF", juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satTrim", "Sat Trim", -12.0f, 12.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("choRate", "Chorus Rate", juce::NormalisableRange<float>(0.05f, 4.0f, 0.01f, 0.5f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("choDepth", "Chorus Depth", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("choWidth", "Chorus Width", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>("dlyTime", "Delay Time", FxChain::getDelayTimeNames(), 4));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dlyFeedback", "Delay Feedback", 0.0f, 0.95f, 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dlyDuck", "Delay Duck", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dlyDamp", "Delay Damp", 0.0f, 1.0f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("frzSize", "Freeze Size", juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.4f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("frzFeedback", "Freeze Feedback", 0.0f, 0.99f, 0.9f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("frzDamp", "Freeze Damp", 0.0f, 1.0f, 0.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("revDecay",   "Reverb Decay",   0.0f, 1.0f, 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("revShimmer", "Reverb Shimmer", 0.0f, 1.0f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("revDamp",    "Reverb Damp",    0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("revMod",     "Reverb Mod",     0.0f, 1.0f, 0.4f));

    // ModMatrix LFO 1-4
    for (int i = 1; i <= ModMatrix::kNumLfos; ++i)
    {
        const juce::String s = juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterChoice>("lfo" + s + "Wave", "LFO " + s + " Wave", ModMatrix::getWaveNames(), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("lfo" + s + "Rate", "LFO " + s + " Rate", juce::NormalisableRange<float>(0.05f, 30.0f, 0.01f, 0.4f), 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterBool>("lfo" + s + "Sync", "LFO " + s + " Sync", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>("lfo" + s + "SyncRate", "LFO " + s + " Sync Rate", ModMatrix::getSyncRateNames(), 6));
    }

    // ModMatrix ENV 1-3
    for (int i = 1; i <= ModMatrix::kNumEnvs; ++i)
    {
        const juce::String s = juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterFloat>("env" + s + "Attack",  "ENV " + s + " Attack",  juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.05f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("env" + s + "Decay",   "ENV " + s + " Decay",   juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("env" + s + "Sustain", "ENV " + s + " Sustain", 0.0f, 1.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("env" + s + "Release", "ENV " + s + " Release", juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterBool>("env" + s + "Loop",     "ENV " + s + " Loop",    false));
    }

    // ModMatrix (16スロット)
    const auto srcNames = ModMatrix::getSourceNames();
    const auto dstNames = ModMatrix::getDestNames();
    for (int i = 1; i <= 16; ++i)
    {
        const juce::String s = juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterChoice>("mod" + s + "Src", "Mod " + s + " Src", srcNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>("mod" + s + "Dst", "Mod " + s + " Dst", dstNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("mod" + s + "Amt", "Mod " + s + " Amt", -1.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterBool>("mod" + s + "Uni", "Mod " + s + " Uni", false));
    }

    return { params.begin(), params.end() };
}

void PicoSamplerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    samplerEngine.prepare(sampleRate);
    arpeggiator.prepare(sampleRate);
    mainFilter.prepare(sampleRate, samplesPerBlock);
    fxChain.prepareToPlay(sampleRate);
    filterAdsr.setSampleRate(sampleRate);

    smoothedCutoff.reset(sampleRate, 0.02); // 20ms スムージングランプ
    smoothedReso.reset(sampleRate, 0.02);
    smoothedGain.reset(sampleRate, 0.02);
}

void PicoSamplerAudioProcessor::releaseResources() {}

bool PicoSamplerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void PicoSamplerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const int modeVal = (int)getParamFloat("samplerMode", 0.0f);
    const int activeSlotIdx = (int)getParamFloat("activeSlot", 0.0f);

    SamplerEngine::Params engineParams;
    engineParams.mode = static_cast<SamplerEngine::PlaybackMode>(modeVal);
    engineParams.activeSlot = activeSlotIdx;
    engineParams.outGainDb = getParamFloat("outGain", 0.0f);
    engineParams.masterHpfHz = getParamFloat("masterHPF", 20.0f);
    engineParams.masterLpfHz = getParamFloat("masterLPF", 20000.0f);
    engineParams.ceilingDb = getParamFloat("ceiling", 0.0f);
    engineParams.limReleaseMs = getParamFloat("limRelease", 50.0f);
    engineParams.is24dBFilter = (getParamFloat("filterSlope", 0.0f) > 0.5f);

    // 1. アルペジエーター パラメータ & MIDI処理
    Arpeggiator::Params arpParams;
    arpParams.enable     = getParamFloat("arpEnable", 0.0f) > 0.5f;
    arpParams.latch      = getParamFloat("arpLatch", 0.0f) > 0.5f;
    arpParams.sync       = getParamFloat("arpSync", 1.0f) > 0.5f;
    arpParams.pattern    = (int)getParamFloat("arpPattern", 0.0f);
    arpParams.rateSync   = (int)getParamFloat("arpRateSync", 6.0f);
    arpParams.rateFreeHz = getParamFloat("arpRateFree", 4.0f);
    arpParams.octaves    = (int)getParamFloat("arpOctaves", 1.0f);
    arpParams.offset     = (int)getParamFloat("arpOffset", 0.0f);
    arpParams.repeat     = (int)getParamFloat("arpRepeat", 1.0f);
    arpParams.accent     = getParamFloat("arpAccent", 0.0f);
    arpParams.swing      = getParamFloat("arpSwing", 0.0f);
    arpParams.gatePct    = getParamFloat("arpGate", 0.8f);
    arpParams.key        = (int)getParamFloat("key", 0.0f);
    arpParams.scale      = (int)getParamFloat("scale", 0.0f);

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm()) arpParams.bpm = *pos->getBpm();
        }
    }

    juce::MidiBuffer processedMidi;
    if (arpParams.enable)
    {
        processedMidi.addEvents(midiMessages, 0, numSamples, 0);
        arpeggiator.process(processedMidi, numSamples, arpParams);
    }

    // 2. ModMatrix パラメータ取得 ＆ 計算
    ModMatrix::Params modParams;
    modParams.bpm = arpParams.bpm;
    for (int i = 0; i < ModMatrix::kNumLfos; ++i)
    {
        const juce::String s = juce::String(i + 1);
        modParams.lfo[(size_t)i].wave = (int)getParamFloat("lfo" + s + "Wave", 0.0f);
        modParams.lfo[(size_t)i].rateHz = getParamFloat("lfo" + s + "Rate", 1.0f);
        modParams.lfo[(size_t)i].sync = getParamFloat("lfo" + s + "Sync", 0.0f) > 0.5f;
        modParams.lfo[(size_t)i].rateSync = (int)getParamFloat("lfo" + s + "SyncRate", 6.0f);
    }
    for (int i = 0; i < ModMatrix::kNumEnvs; ++i)
    {
        const juce::String s = juce::String(i + 1);
        modParams.env[(size_t)i].attack = getParamFloat("env" + s + "Attack", 0.05f);
        modParams.env[(size_t)i].decay = getParamFloat("env" + s + "Decay", 0.5f);
        modParams.env[(size_t)i].sustain = getParamFloat("env" + s + "Sustain", 1.0f);
        modParams.env[(size_t)i].release = getParamFloat("env" + s + "Release", 0.3f);
        modParams.env[(size_t)i].loop = getParamFloat("env" + s + "Loop", 0.0f) > 0.5f;
    }
    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        const juce::String s = juce::String(i + 1);
        modParams.slot[(size_t)i].src = (int)getParamFloat("mod" + s + "Src", 0.0f);
        modParams.slot[(size_t)i].dst = (int)getParamFloat("mod" + s + "Dst", 0.0f);
        modParams.slot[(size_t)i].amt = getParamFloat("mod" + s + "Amt", 0.0f);
        modParams.slot[(size_t)i].uni = getParamFloat("mod" + s + "Uni", 0.0f) > 0.5f;
    }

    const auto& activeMidiForMod = arpParams.enable ? processedMidi : midiMessages;
    modMatrix.handleMidi(activeMidiForMod);
    modMatrix.processBlock(numSamples, modParams);

    // ARP変調追加適用
    arpParams.rateFreeHz = juce::jlimit(1.0f, 30.0f, arpParams.rateFreeHz + modMatrix.get(ModMatrix::DstArpRate) * 15.0f);
    arpParams.octaves    = juce::jlimit(1, 4, (int)(arpParams.octaves + modMatrix.get(ModMatrix::DstArpOctaves) * 3.0f));
    arpParams.offset     = juce::jlimit(-12, 12, (int)(arpParams.offset + modMatrix.get(ModMatrix::DstArpOffset) * 12.0f));
    arpParams.repeat     = juce::jlimit(1, 4, (int)(arpParams.repeat + modMatrix.get(ModMatrix::DstArpRepeat) * 3.0f));
    arpParams.accent     = juce::jlimit(-1.0f, 1.0f, arpParams.accent + modMatrix.get(ModMatrix::DstArpAccent));
    arpParams.swing      = juce::jlimit(0.0f, 0.75f, arpParams.swing + modMatrix.get(ModMatrix::DstArpSwing));
    arpParams.gatePct    = juce::jlimit(0.1f, 1.0f, arpParams.gatePct + modMatrix.get(ModMatrix::DstArpGate));

    // 3. サンプラーエンジンパラメータ構築 (Mod変調含む)
    const float rawMasterPitch = getParamFloat("masterPitch", 0.0f) + modMatrix.get(ModMatrix::DstMasterPitch) * 24.0f;
    const int keyVal = (int)getParamFloat("key", 0.0f);
    const int scaleVal = (int)getParamFloat("scale", 0.0f);

    float effectiveMasterPitch = rawMasterPitch;
    if (scaleVal > 0)
    {
        const int rootNote = (keyVal >= 1) ? (keyVal - 1) : 0;
        const float quantizedTarget = ScaleQuantizer::quantize(60.0f + (float)rootNote + rawMasterPitch, rootNote, scaleVal);
        effectiveMasterPitch = quantizedTarget - (60.0f + (float)rootNote);
    }

    for (int i = 0; i < 8; ++i)
    {
        const juce::String s = juce::String(i);
        auto& sp = engineParams.slotParams[(size_t)i];
        sp.attack  = getParamFloat("attack_" + s, 0.01f);
        sp.decay   = getParamFloat("decay_" + s, 0.3f);
        sp.sustain = getParamFloat("sustain_" + s, 1.0f);
        sp.release = getParamFloat("release_" + s, 0.3f);

        sp.octave   = (int)getParamFloat("octave_" + s, 0.0f);
        sp.semitone = (int)getParamFloat("pitchSt_" + s, 0.0f) + (int)std::round(effectiveMasterPitch);
        sp.fineTune = getParamFloat("fineTune_" + s, 0.0f) + (effectiveMasterPitch - std::round(effectiveMasterPitch)) * 100.0f;
        sp.pan      = getParamFloat("pan_" + s, 0.0f);
        sp.slotGainDb = getParamFloat("slotGain_" + s, 0.0f);

        const int dstBase = ModMatrix::DstS1Start + i * 5;
        sp.sampleStartRatio = juce::jlimit(0.0f, 1.0f, getParamFloat("sampleStart_" + s, 0.0f) + modMatrix.get(dstBase + 0));
        sp.sampleEndRatio   = juce::jlimit(0.01f, 1.0f, getParamFloat("sampleEnd_" + s, 1.0f) + modMatrix.get(dstBase + 1));
        sp.loopStartRatio   = juce::jlimit(0.0f, 1.0f, getParamFloat("loopStart_" + s, 0.2f) + modMatrix.get(dstBase + 2));
        sp.loopEndRatio     = juce::jlimit(0.01f, 1.0f, getParamFloat("loopEnd_" + s, 0.7f) + modMatrix.get(dstBase + 3));
        sp.crossfadeRatio   = juce::jlimit(0.0f, 0.5f, getParamFloat("crossfade_" + s, 0.05f) + modMatrix.get(dstBase + 4));

        sp.isLooping = getParamFloat("isLooping_" + s, 0.0f) > 0.5f;
        sp.isStretchMode = getParamFloat("isStretchMode_" + s, 0.0f) > 0.5f;
        sp.isReverse = getParamFloat("isReverse_" + s, 0.0f) > 0.5f;
        sp.isFilterBypass = getParamFloat("filterBypass_" + s, 0.0f) > 0.5f;
        sp.isFxBypass = getParamFloat("fxBypass_" + s, 0.0f) > 0.5f;
        sp.rootKeyOverride = (int)getParamFloat("rootKey_" + s, -1.0f);
        sp.lowNote = (int)getParamFloat("slotLowNote_" + s, 0.0f);
        sp.highNote = (int)getParamFloat("slotHighNote_" + s, 127.0f);
    }

    if (arpParams.enable)
        samplerEngine.handleMidi(processedMidi, engineParams);
    else
        samplerEngine.handleMidi(midiMessages, engineParams);

    // Filter ADSR トリガー制御
    const auto& activeMidi = arpParams.enable ? processedMidi : midiMessages;
    for (const auto metadata : activeMidi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) filterAdsr.noteOn();
        else if (msg.isNoteOff()) filterAdsr.noteOff();
    }

    juce::ADSR::Parameters adsrP;
    adsrP.attack  = getParamFloat("fltEnvAttack", 0.01f);
    adsrP.decay   = getParamFloat("fltEnvDecay", 0.3f);
    adsrP.sustain = getParamFloat("fltEnvSustain", 1.0f);
    adsrP.release = getParamFloat("fltEnvRelease", 0.3f);
    filterAdsr.setParameters(adsrP);

    const float envVal = filterAdsr.getNextSample();
    const float envAmt = getParamFloat("fltEnvAmt", 0.0f);

    const int numChannels = buffer.getNumChannels();
    juce::AudioBuffer<float> fltBypassBuffer(numChannels, numSamples);
    juce::AudioBuffer<float> fxBypassBuffer(numChannels, numSamples);
    juce::AudioBuffer<float> bothBypassBuffer(numChannels, numSamples);

    fltBypassBuffer.clear();
    fxBypassBuffer.clear();
    bothBypassBuffer.clear();

    // 1. 各スロットのボイスをルーティングに応じて各バッファにレンダリング
    samplerEngine.renderNextBlock(buffer, fltBypassBuffer, fxBypassBuffer, bothBypassBuffer, engineParams, &visualizerData);

    // 2. PicoFilter (CleanSVF, Vowel, Comb) 適用 (FLT BYPASSがオフのスロット音声に適用)
    PicoFilter::Params fltParams;
    fltParams.enable  = getParamFloat("fltEnable", 0.0f) > 0.5f;
    fltParams.model   = (int)getParamFloat("fltModel", 0.0f);

    const float baseCutoff = getParamFloat("fltCutoff", 2000.0f);
    float targetCutoff = baseCutoff;
    if (std::abs(envAmt) > 0.001f || std::abs(modMatrix.get(ModMatrix::DstFltCutoff)) > 0.001f)
    {
        const float octaveShift = envVal * envAmt * 4.0f + modMatrix.get(ModMatrix::DstFltCutoff) * 4.0f;
        targetCutoff = baseCutoff * std::pow(2.0f, octaveShift);
    }
    targetCutoff = juce::jlimit(20.0f, 20000.0f, targetCutoff);
    smoothedCutoff.setTargetValue(targetCutoff);

    const float targetReso = juce::jlimit(0.1f, 10.0f, getParamFloat("fltRes", 0.707f) + modMatrix.get(ModMatrix::DstFltReso) * 5.0f);
    smoothedReso.setTargetValue(targetReso);

    fltParams.cutoff  = smoothedCutoff.getNextValue();
    fltParams.res     = smoothedReso.getNextValue();
    fltParams.type    = (int)getParamFloat("fltType", 0.0f);
    fltParams.slope   = (int)getParamFloat("fltSlope", 0.0f);
    fltParams.formant = juce::jlimit(0.0f, 1.0f, getParamFloat("fltFormant", 0.0f) + modMatrix.get(ModMatrix::DstFltFormant));
    fltParams.combMix = juce::jlimit(0.0f, 1.0f, getParamFloat("fltCombMix", 0.5f) + modMatrix.get(ModMatrix::DstFltCombMix));

    if (fltParams.enable)
    {
        mainFilter.process(buffer, fltParams);
        mainFilter.process(fxBypassBuffer, fltParams);
    }

    // 3. Filter Bypass音 (fltBypassBuffer) を FX前バッファ (buffer) に合流
    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffer.addFrom(ch, 0, fltBypassBuffer, ch, 0, numSamples);
    }

    // 4. FX Rack 適用 (FX Bypassがオフの音声: buffer)
    FxChain::Params fxP;
    for (int i = 0; i < 5; ++i)
    {
        const juce::String s = juce::String(i + 1);
        fxP.type[(size_t)i]   = (int)getParamFloat("fx" + s + "Type", 0.0f);
        fxP.amount[(size_t)i] = juce::jlimit(0.0f, 1.0f, getParamFloat("fx" + s + "Amount", 0.0f) + modMatrix.get(ModMatrix::DstFx1Amount + i));
    }
    fxP.bpm        = arpParams.bpm;
    fxP.satAlgo    = (int)getParamFloat("satAlgo", 0.0f);
    fxP.satDrive   = juce::jlimit(1.0f, 12.0f, getParamFloat("satDrive", 2.0f) + modMatrix.get(ModMatrix::DstSatDrive) * 6.0f);
    fxP.satPreHz   = juce::jlimit(20.0f, 2000.0f, getParamFloat("satPreHz", 20.0f) + modMatrix.get(ModMatrix::DstSatPreHz) * 1000.0f);
    fxP.satTrimDb  = juce::jlimit(-12.0f, 12.0f, getParamFloat("satTrim", 0.0f) + modMatrix.get(ModMatrix::DstSatTrim) * 12.0f);
    fxP.choRate    = juce::jlimit(0.05f, 4.0f, getParamFloat("choRate", 0.8f) + modMatrix.get(ModMatrix::DstChoRate) * 2.0f);
    fxP.choDepth   = juce::jlimit(0.0f, 1.0f, getParamFloat("choDepth", 0.5f) + modMatrix.get(ModMatrix::DstChoDepth));
    fxP.choWidth   = juce::jlimit(0.0f, 1.0f, getParamFloat("choWidth", 1.0f) + modMatrix.get(ModMatrix::DstChoWidth));
    fxP.dlyTime    = (int)getParamFloat("dlyTime", 4.0f);
    fxP.dlyFeedback= juce::jlimit(0.0f, 0.95f, getParamFloat("dlyFeedback", 0.45f) + modMatrix.get(ModMatrix::DstDlyFeedback));
    fxP.dlyDuck    = juce::jlimit(0.0f, 1.0f, getParamFloat("dlyDuck", 0.5f) + modMatrix.get(ModMatrix::DstDlyDuck));
    fxP.dlyDamp    = juce::jlimit(0.0f, 1.0f, getParamFloat("dlyDamp", 0.3f) + modMatrix.get(ModMatrix::DstDlyDamp));
    fxP.frzSize    = juce::jlimit(20.0f, 1000.0f, getParamFloat("frzSize", 100.0f) + modMatrix.get(ModMatrix::DstFrzSize) * 500.0f);
    fxP.frzFeedback= juce::jlimit(0.0f, 0.99f, getParamFloat("frzFeedback", 0.9f) + modMatrix.get(ModMatrix::DstFrzFeedback));
    fxP.frzDamp    = juce::jlimit(0.0f, 1.0f, getParamFloat("frzDamp", 0.2f) + modMatrix.get(ModMatrix::DstFrzDamp));
    fxP.revDecay   = juce::jlimit(0.0f, 1.0f, getParamFloat("revDecay", 0.7f) + modMatrix.get(ModMatrix::DstRevDecay));
    fxP.revShimmer = juce::jlimit(0.0f, 1.0f, getParamFloat("revShimmer", 0.4f) + modMatrix.get(ModMatrix::DstRevShimmer));
    fxP.revDamp    = juce::jlimit(0.0f, 1.0f, getParamFloat("revDamp", 0.3f) + modMatrix.get(ModMatrix::DstRevDamp));
    fxP.revMod     = juce::jlimit(0.0f, 1.0f, getParamFloat("revMod", 0.4f) + modMatrix.get(ModMatrix::DstRevMod));

    fxChain.process(buffer, fxP);

    // 5. FX Bypass音 (fxBypassBuffer, bothBypassBuffer) をマスター出力に合流
    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffer.addFrom(ch, 0, fxBypassBuffer, ch, 0, numSamples);
        buffer.addFrom(ch, 0, bothBypassBuffer, ch, 0, numSamples);
    }
}

void PicoSamplerAudioProcessor::reanalyzeSlot(int slotIdx)
{
    if (slotIdx < 0 || slotIdx >= 8) return;
    const juce::String s = juce::String(slotIdx);
    const int rootOverride = (int)getParamFloat("rootKey_" + s, -1.0f);
    const int matMode = (int)getParamFloat("analysisEngine", 0.0f);

    samplerEngine.getSlot(slotIdx).reanalyze(matMode, rootOverride);
}

void PicoSamplerAudioProcessor::clearSlot(int slotIdx)
{
    if (slotIdx < 0 || slotIdx >= 8) return;
    samplerEngine.getSlot(slotIdx).clear();
}

juce::AudioProcessorEditor* PicoSamplerAudioProcessor::createEditor()
{
    return new PicoSamplerAudioProcessorEditor(*this);
}

void PicoSamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement xmlState("PicoSamplerState");

    auto apvtsXml = apvts.copyState().createXml();
    if (apvtsXml != nullptr)
    {
        xmlState.addChildElement(apvtsXml.release());
    }

    auto* slotsXml = new juce::XmlElement("LoadedSlots");
    for (int i = 0; i < 8; ++i)
    {
        auto* sXml = new juce::XmlElement("Slot");
        sXml->setAttribute("index", i);
        sXml->setAttribute("path", samplerEngine.getSlot(i).getMetadata().filePath);
        slotsXml->addChildElement(sXml);
    }
    xmlState.addChildElement(slotsXml);

    copyXmlToBinary(xmlState, destData);
}

void PicoSamplerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr) return;

    if (xmlState->hasTagName("PicoSamplerState"))
    {
        if (auto* paramXml = xmlState->getChildByName(apvts.state.getType().toString()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*paramXml));
        }

        if (auto* slotsXml = xmlState->getChildByName("LoadedSlots"))
        {
            const juce::ScopedLock lock(jobLock);
            pendingJobs.clear();

            for (auto* sXml : slotsXml->getChildIterator())
            {
                if (sXml->hasTagName("Slot"))
                {
                    int idx = sXml->getIntAttribute("index", -1);
                    juce::String path = sXml->getStringAttribute("path");

                    if (idx >= 0 && idx < 8 && path.isNotEmpty())
                    {
                        juce::File file(path);
                        if (file.existsAsFile())
                        {
                            pendingJobs.add({ idx, file });
                        }
                    }
                }
            }
        }
    }
    else if (xmlState->hasTagName(apvts.state.getType().toString()))
    {
        // 互換性フォールバック (直に PARAMETERTREE が保存されている旧プリセット対応)
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

void PicoSamplerAudioProcessor::run()
{
    while (!threadShouldExit())
    {
        AsyncLoadJob job;
        bool hasJob = false;

        {
            const juce::ScopedLock lock(jobLock);
            if (!pendingJobs.isEmpty())
            {
                job = pendingJobs.removeAndReturn(0);
                hasJob = true;
            }
        }

        if (hasJob)
        {
            samplerEngine.getSlot(job.slotIndex).loadFromFile(job.file);
        }
        else
        {
            wait(50);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PicoSamplerAudioProcessor();
}
