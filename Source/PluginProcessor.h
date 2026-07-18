// ==========================================
// File: PluginProcessor.h
// PicoSampler メインプロセッサ定義 (reanalyzeSlot メソッド追加)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "DSP/SamplerEngine.h"
#include "DSP/Arpeggiator.h"
#include "DSP/PicoFilter.h"
#include "DSP/ModMatrix.h"
#include "DSP/FxChain.h"
#include "DSP/SampleVisualizerData.h"

class PicoSamplerAudioProcessor : public juce::AudioProcessor, public juce::Thread
{
public:
    PicoSamplerAudioProcessor();
    ~PicoSamplerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void run() override; // スレッド処理 (ファイルロード)

    void reanalyzeSlot(int slotIdx); // ★ スロットの再解析・再配置

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    SamplerEngine& getSamplerEngine() { return samplerEngine; }
    SampleVisualizerData& getVisualizerData() { return visualizerData; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float getParamFloat(const juce::String& paramId, float defaultVal = 0.0f) const;

    juce::AudioProcessorValueTreeState apvts;
    SamplerEngine samplerEngine;
    Arpeggiator arpeggiator;
    PicoFilter mainFilter;
    ModMatrix modMatrix;
    FxChain fxChain;
    SampleVisualizerData visualizerData;

    struct AsyncLoadJob
    {
        int slotIndex = 0;
        juce::File file;
    };
    juce::Array<AsyncLoadJob> pendingJobs;
    juce::CriticalSection jobLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PicoSamplerAudioProcessor)
};
