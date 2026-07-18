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

    const float rawMasterPitch = getParamFloat("masterPitch", 0.0f);
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

        sp.sampleStartRatio = getParamFloat("sampleStart_" + s, 0.0f);
        sp.sampleEndRatio   = getParamFloat("sampleEnd_" + s, 1.0f);
        sp.loopStartRatio   = getParamFloat("loopStart_" + s, 0.2f);
        sp.loopEndRatio     = getParamFloat("loopEnd_" + s, 0.7f);
        sp.crossfadeRatio   = getParamFloat("crossfade_" + s, 0.05f);

        sp.isLooping = getParamFloat("isLooping_" + s, 0.0f) > 0.5f;
        sp.isStretchMode = getParamFloat("isStretchMode_" + s, 0.0f) > 0.5f;
        sp.isReverse = getParamFloat("isReverse_" + s, 0.0f) > 0.5f;
        sp.rootKeyOverride = (int)getParamFloat("rootKey_" + s, -1.0f);
        sp.lowNote = (int)getParamFloat("slotLowNote_" + s, 0.0f);
        sp.highNote = (int)getParamFloat("slotHighNote_" + s, 127.0f);
    }

    // アルペジエーター処理
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
        samplerEngine.handleMidi(processedMidi, engineParams);
    }
    else
    {
        samplerEngine.handleMidi(midiMessages, engineParams);
    }

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

    samplerEngine.renderNextBlock(buffer, engineParams, &visualizerData);

    // PicoFilter (CleanSVF, Vowel, Comb) 適用
    PicoFilter::Params fltParams;
    fltParams.enable  = getParamFloat("fltEnable", 0.0f) > 0.5f;
    fltParams.model   = (int)getParamFloat("fltModel", 0.0f);

    const float baseCutoff = getParamFloat("fltCutoff", 2000.0f);
    float modCutoff = baseCutoff;
    if (std::abs(envAmt) > 0.001f)
    {
        const float octaveShift = envVal * envAmt * 4.0f; // 最大4オクターブ変調
        modCutoff = baseCutoff * std::pow(2.0f, octaveShift);
        modCutoff = juce::jlimit(20.0f, 20000.0f, modCutoff);
    }
    fltParams.cutoff  = modCutoff;
    fltParams.res     = getParamFloat("fltRes", 0.707f);
    fltParams.type    = (int)getParamFloat("fltType", 0.0f);
    fltParams.slope   = (int)getParamFloat("fltSlope", 0.0f);
    fltParams.formant = getParamFloat("fltFormant", 0.0f);
    fltParams.combMix = getParamFloat("fltCombMix", 0.5f);

    mainFilter.process(buffer, fltParams);

    // FX 5スロット & 詳細パラメータ適用
    FxChain::Params fxP;
    for (int i = 0; i < 5; ++i)
    {
        const juce::String s = juce::String(i + 1);
        fxP.type[(size_t)i]   = (int)getParamFloat("fx" + s + "Type", 0.0f);
        fxP.amount[(size_t)i] = getParamFloat("fx" + s + "Amount", 0.0f);
    }
    fxP.bpm        = arpParams.bpm;
    fxP.satAlgo    = (int)getParamFloat("satAlgo", 0.0f);
    fxP.satDrive   = getParamFloat("satDrive", 2.0f);
    fxP.satPreHz   = getParamFloat("satPreHz", 20.0f);
    fxP.satTrimDb  = getParamFloat("satTrim", 0.0f);
    fxP.choRate    = getParamFloat("choRate", 0.8f);
    fxP.choDepth   = getParamFloat("choDepth", 0.5f);
    fxP.choWidth   = getParamFloat("choWidth", 1.0f);
    fxP.dlyTime    = (int)getParamFloat("dlyTime", 4.0f);
    fxP.dlyFeedback= getParamFloat("dlyFeedback", 0.45f);
    fxP.dlyDuck    = getParamFloat("dlyDuck", 0.5f);
    fxP.dlyDamp    = getParamFloat("dlyDamp", 0.3f);
    fxP.frzSize    = getParamFloat("frzSize", 100.0f);
    fxP.frzFeedback= getParamFloat("frzFeedback", 0.9f);
    fxP.frzDamp    = getParamFloat("frzDamp", 0.2f);
    fxP.revDecay   = getParamFloat("revDecay", 0.7f);
    fxP.revShimmer = getParamFloat("revShimmer", 0.4f);
    fxP.revDamp    = getParamFloat("revDamp", 0.3f);
    fxP.revMod     = getParamFloat("revMod", 0.4f);

    fxChain.process(buffer, fxP);
}

void PicoSamplerAudioProcessor::reanalyzeSlot(int slotIdx)
{
    if (slotIdx < 0 || slotIdx >= 8) return;
    const juce::String s = juce::String(slotIdx);
    const int rootOverride = (int)getParamFloat("rootKey_" + s, -1.0f);
    const int matMode = (int)getParamFloat("analysisEngine", 0.0f);

    samplerEngine.getSlot(slotIdx).reanalyze(matMode, rootOverride);
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
