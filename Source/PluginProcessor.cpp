// ==========================================
// File: PluginProcessor.cpp
// PicoSampler メインプロセッサ実装 (isSnap & limRelease パラメータ追加)
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

PicoSamplerAudioProcessor::PicoSamplerAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      Thread("PicoSamplerIngestThread"),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    startThread();
}

PicoSamplerAudioProcessor::~PicoSamplerAudioProcessor()
{
    stopThread(5000);
}

juce::AudioProcessorValueTreeState::ParameterLayout PicoSamplerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Mode & Master
    params.push_back(std::make_unique<juce::AudioParameterChoice>("samplerMode", "Sampler Mode", juce::StringArray{ "Single", "Layer", "Random" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("activeSlot", "Active Slot", 0, 7, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterHPF", "Master HPF", juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterLPF", "Master LPF", juce::NormalisableRange<float>(200.0f, 20000.0f, 1.0f, 0.3f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Master Gain", juce::NormalisableRange<float>(-36.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("ceiling", "Limiter Ceiling", juce::NormalisableRange<float>(-12.0f, 0.0f, 0.1f), -0.1f));

    // 8スロット別パラメータ
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
        params.push_back(std::make_unique<juce::AudioParameterInt>("rootKey_" + s, "Root Key " + s, -1, 127, -1));

        params.push_back(std::make_unique<juce::AudioParameterInt>("slotLowNote_" + s, "Low Note " + s, 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>("slotHighNote_" + s, "High Note " + s, 0, 127, 127));
    }

    // Arpeggiator
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpEnable", "Arp Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpLatch", "Arp Latch", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpSync", "Arp Sync", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpPattern", "Arp Pattern", Arpeggiator::getPatternNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpRateSync", "Arp Rate Sync", Arpeggiator::getSyncRateNames(), 6));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("arpRateFree", "Arp Rate Free", 0.1f, 20.0f, 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("arpOctaves", "Arp Octaves", 1, 4, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("arpGate", "Arp Gate", 0.1f, 1.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("key", "Root Key", juce::StringArray{ "Auto", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("scale", "Scale", ScaleQuantizer::getScaleNames(), 0));

    // FX 5スロット
    for (int i = 1; i <= 5; ++i)
    {
        const juce::String s = juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterChoice>("fx" + s + "Type", "FX" + s + " Type", FxChain::getTypeNames(), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("fx" + s + "Amount", "FX" + s + " Amount", 0.0f, 1.0f, 0.0f));
    }

    // Config
    params.push_back(std::make_unique<juce::AudioParameterChoice>("materialMode", "Material Mode", juce::StringArray{ "Auto", "Crisp", "Smooth", "Formant" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("filterSlope", "Filter Slope", juce::StringArray{ "12dB", "24dB" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("colorTheme", "Color Theme", juce::StringArray{ "Midnight", "Sakura", "Ocean", "Forest", "Sunset", "Mono" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("poly", "Polyphony", 1, 32, 32));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("limRelease", "Limiter Release", juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f, 0.3f), 50.0f));

    // ModMatrix 16スロット
    for (int i = 1; i <= 16; ++i)
    {
        const juce::String s = juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterChoice>("mod" + s + "Src", "Mod" + s + " Src", ModMatrix::getSourceNames(), 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>("mod" + s + "Dst", "Mod" + s + " Dst", ModMatrix::getDestNames(), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("mod" + s + "Amt", "Mod" + s + " Amt", -1.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterBool>("mod" + s + "Uni", "Mod" + s + " Uni", false));
    }

    return { params.begin(), params.end() };
}

void PicoSamplerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    samplerEngine.prepare(sampleRate);
    arpeggiator.prepare(sampleRate);
    modMatrix.prepare(sampleRate);
    fxChain.prepareToPlay(sampleRate);
}

void PicoSamplerAudioProcessor::releaseResources()
{
    samplerEngine.reset();
}

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
    const int numSamples = buffer.getNumSamples();

    // 1. Host BPM 取得
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue()) bpm = *pos->getBpm();
        }
    }

    auto getParamFloat = [this](const juce::String& name, float defVal) {
        if (auto* p = apvts.getRawParameterValue(name)) return p->load();
        return defVal;
    };

    // 2. Arpeggiator 処理
    Arpeggiator::Params arpP;
    arpP.enable = getParamFloat("arpEnable", 0.0f) > 0.5f;
    arpP.latch  = getParamFloat("arpLatch", 0.0f) > 0.5f;
    arpP.sync   = getParamFloat("arpSync", 1.0f) > 0.5f;
    arpP.pattern = (int)getParamFloat("arpPattern", 0.0f);
    arpP.rateSync = (int)getParamFloat("arpRateSync", 6.0f);
    arpP.rateFreeHz = getParamFloat("arpRateFree", 4.0f);
    arpP.octaves = (int)getParamFloat("arpOctaves", 1.0f);
    arpP.gatePct = getParamFloat("arpGate", 0.8f);
    arpP.key = (int)getParamFloat("key", 0.0f);
    arpP.scale = (int)getParamFloat("scale", 0.0f);
    arpP.bpm = bpm;

    arpeggiator.process(midiMessages, numSamples, arpP);

    // 3. ModMatrix 処理
    modMatrix.handleMidi(midiMessages);
    ModMatrix::Params modP;
    modP.bpm = bpm;
    modMatrix.processBlock(numSamples, modP);

    // 4. Sampler Engine 処理
    SamplerEngine::Params engineP;
    engineP.mode = (SamplerEngine::PlaybackMode)(int)getParamFloat("samplerMode", 0.0f);
    engineP.activeSlot = (int)getParamFloat("activeSlot", 0.0f);
    engineP.masterHpfHz = getParamFloat("masterHPF", 20.0f);
    engineP.masterLpfHz = getParamFloat("masterLPF", 20000.0f);
    engineP.is24dBFilter = (int)getParamFloat("filterSlope", 0.0f) > 0;
    engineP.outGainDb = getParamFloat("outGain", 0.0f);
    engineP.ceilingDb = getParamFloat("ceiling", -0.1f);
    engineP.limReleaseMs = getParamFloat("limRelease", 50.0f);
    engineP.polyphonyLimit = (int)getParamFloat("poly", 32.0f);

    for (int i = 0; i < 8; ++i)
    {
        const juce::String s = juce::String(i);
        auto& sp = engineP.slotParams[(size_t)i];
        sp.attack = getParamFloat("attack_" + s, 0.01f);
        sp.decay = getParamFloat("decay_" + s, 0.3f);
        sp.sustain = getParamFloat("sustain_" + s, 1.0f);
        sp.release = getParamFloat("release_" + s, 0.3f);
        sp.octave = (int)getParamFloat("octave_" + s, 0.0f);
        sp.semitone = (int)getParamFloat("pitchSt_" + s, 0.0f);
        sp.fineTune = getParamFloat("fineTune_" + s, 0.0f);
        sp.pan = getParamFloat("pan_" + s, 0.0f);
        sp.slotGainDb = getParamFloat("slotGain_" + s, 0.0f);
        sp.sampleStartRatio = getParamFloat("sampleStart_" + s, 0.0f);
        sp.sampleEndRatio = getParamFloat("sampleEnd_" + s, 1.0f);
        sp.loopStartRatio = getParamFloat("loopStart_" + s, 0.2f);
        sp.loopEndRatio = getParamFloat("loopEnd_" + s, 0.7f);
        sp.crossfadeRatio = getParamFloat("crossfade_" + s, 0.05f);
        sp.isLooping = getParamFloat("isLooping_" + s, 0.0f) > 0.5f;
        sp.isStretchMode = getParamFloat("isStretchMode_" + s, 0.0f) > 0.5f;
        sp.isReverse = getParamFloat("isReverse_" + s, 0.0f) > 0.5f;
        sp.rootKeyOverride = (int)getParamFloat("rootKey_" + s, -1.0f);
        sp.lowNote = (int)getParamFloat("slotLowNote_" + s, 0.0f);
        sp.highNote = (int)getParamFloat("slotHighNote_" + s, 127.0f);
    }

    buffer.clear();
    samplerEngine.handleMidi(midiMessages, engineP);
    samplerEngine.renderNextBlock(buffer, engineP, &visualizerData);

    // 5. FX Chain 処理
    FxChain::Params fxP;
    fxP.bpm = bpm;
    for (int i = 1; i <= 5; ++i)
    {
        const juce::String s = juce::String(i);
        fxP.type[(size_t)(i - 1)] = (int)getParamFloat("fx" + s + "Type", 0.0f);
        fxP.amount[(size_t)(i - 1)] = getParamFloat("fx" + s + "Amount", 0.0f);
    }
    fxChain.process(buffer, fxP);
}

juce::AudioProcessorEditor* PicoSamplerAudioProcessor::createEditor()
{
    return new PicoSamplerAudioProcessorEditor(*this);
}

void PicoSamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    if (xml != nullptr)
    {
        auto* slotsXml = xml->createNewChildElement("SAMPLE_SLOTS");
        for (int i = 0; i < 8; ++i)
        {
            const auto& meta = samplerEngine.getSlot(i).getMetadata();
            auto* sXml = slotsXml->createNewChildElement("SLOT");
            sXml->setAttribute("index", i);
            sXml->setAttribute("filePath", meta.filePath);
            sXml->setAttribute("rootKey", meta.rootKey);
        }
    }

    copyXmlToBinary(*xml, destData);
}

void PicoSamplerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

        if (auto* slotsXml = xmlState->getChildByName("SAMPLE_SLOTS"))
        {
            for (auto* sXml : slotsXml->getChildIterator())
            {
                const int idx = sXml->getIntAttribute("index", -1);
                const juce::String path = sXml->getStringAttribute("filePath");

                if (idx >= 0 && idx < 8 && path.isNotEmpty())
                {
                    juce::File file(path);
                    if (file.existsAsFile())
                    {
                        const juce::ScopedLock lock(jobLock);
                        pendingJobs.add({ idx, file });
                    }
                }
            }
        }
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
