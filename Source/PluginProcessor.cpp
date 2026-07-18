// ==========================================
// File: PluginProcessor.cpp
// PicoSampler メインプロセッサ実装 (DAWプロジェクト完全保存復元 & StretchMode対応)
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
        params.push_back(std::make_unique<juce::AudioParameterFloat>("loopLength_" + s, "Loop Length " + s, 0.01f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("crossfade_" + s, "Crossfade " + s, 0.0f, 0.5f, 0.05f));
        params.push_back(std::make_unique<juce::AudioParameterBool>("isLooping_" + s, "Looping " + s, false));
        params.push_back(std::make_unique<juce::AudioParameterBool>("isStretchMode_" + s, "Stretch Mode " + s, false));

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
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (pos->getBpm().hasValue()) bpm = *pos->getBpm();
        }
    }

    // 2. Arpeggiator 処理
    Arpeggiator::Params arpP;
    arpP.enable = apvts.getRawParameterValue("arpEnable")->load() > 0.5f;
    arpP.latch  = apvts.getRawParameterValue("arpLatch")->load() > 0.5f;
    arpP.sync   = apvts.getRawParameterValue("arpSync")->load() > 0.5f;
    arpP.pattern = (int)apvts.getRawParameterValue("arpPattern")->load();
    arpP.rateSync = (int)apvts.getRawParameterValue("arpRateSync")->load();
    arpP.rateFreeHz = apvts.getRawParameterValue("arpRateFree")->load();
    arpP.octaves = (int)apvts.getRawParameterValue("arpOctaves")->load();
    arpP.gatePct = apvts.getRawParameterValue("arpGate")->load();
    arpP.key = (int)apvts.getRawParameterValue("key")->load();
    arpP.scale = (int)apvts.getRawParameterValue("scale")->load();
    arpP.bpm = bpm;

    arpeggiator.process(midiMessages, numSamples, arpP);

    // 3. ModMatrix 処理
    modMatrix.handleMidi(midiMessages);
    ModMatrix::Params modP;
    modP.bpm = bpm;
    modMatrix.processBlock(numSamples, modP);

    // 4. Sampler Engine 処理
    SamplerEngine::Params engineP;
    engineP.mode = (SamplerEngine::PlaybackMode)(int)apvts.getRawParameterValue("samplerMode")->load();
    engineP.activeSlot = (int)apvts.getRawParameterValue("activeSlot")->load();
    engineP.masterHpfHz = apvts.getRawParameterValue("masterHPF")->load();
    engineP.masterLpfHz = apvts.getRawParameterValue("masterLPF")->load();
    engineP.is24dBFilter = (int)apvts.getRawParameterValue("filterSlope")->load() > 0;
    engineP.outGainDb = apvts.getRawParameterValue("outGain")->load();
    engineP.polyphonyLimit = (int)apvts.getRawParameterValue("poly")->load();

    for (int i = 0; i < 8; ++i)
    {
        const juce::String s = juce::String(i);
        auto& sp = engineP.slotParams[(size_t)i];
        sp.attack = apvts.getRawParameterValue("attack_" + s)->load();
        sp.decay = apvts.getRawParameterValue("decay_" + s)->load();
        sp.sustain = apvts.getRawParameterValue("sustain_" + s)->load();
        sp.release = apvts.getRawParameterValue("release_" + s)->load();
        sp.octave = (int)apvts.getRawParameterValue("octave_" + s)->load();
        sp.semitone = (int)apvts.getRawParameterValue("pitchSt_" + s)->load();
        sp.fineTune = apvts.getRawParameterValue("fineTune_" + s)->load();
        sp.pan = apvts.getRawParameterValue("pan_" + s)->load();
        sp.slotGainDb = apvts.getRawParameterValue("slotGain_" + s)->load();
        sp.sampleStartRatio = apvts.getRawParameterValue("sampleStart_" + s)->load();
        sp.sampleEndRatio = apvts.getRawParameterValue("sampleEnd_" + s)->load();
        sp.loopStartRatio = apvts.getRawParameterValue("loopStart_" + s)->load();
        sp.loopLengthRatio = apvts.getRawParameterValue("loopLength_" + s)->load();
        sp.crossfadeRatio = apvts.getRawParameterValue("crossfade_" + s)->load();
        sp.isLooping = apvts.getRawParameterValue("isLooping_" + s)->load() > 0.5f;
        sp.isStretchMode = apvts.getRawParameterValue("isStretchMode_" + s)->load() > 0.5f;
        sp.lowNote = (int)apvts.getRawParameterValue("slotLowNote_" + s)->load();
        sp.highNote = (int)apvts.getRawParameterValue("slotHighNote_" + s)->load();
    }

    buffer.clear();
    samplerEngine.handleMidi(midiMessages, engineP);
    samplerEngine.renderNextBlock(buffer, engineP, &visualizerData);

    // 5. FX Chain 処理
    FxChain::Params fxP;
    fxP.bpm = bpm;
    for (int i = 0; i < 5; ++i)
    {
        const juce::String s = juce::String(i + 1);
        fxP.type[(size_t)i] = (int)apvts.getRawParameterValue("fx" + s + "Type")->load();
        fxP.amount[(size_t)i] = apvts.getRawParameterValue("fx" + s + "Amount")->load();
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
                const int rKey = sXml->getIntAttribute("rootKey", 60);

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
