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
    
    void autoSliceFile(const juce::File& file, int stretchAlgo, float sensitivity);
    
    void requestLoadFile(int slotIdx, const juce::File& file, bool autoSlice = false);

    void reanalyzeSlot(int slotIdx); // ★ スロットの再解析・再配置
    void clearSlot(int slotIdx);     // ★ スロットの全消去・初期化

    // ---- プリセット ----
    // プリセットは APVTS の全パラメータ + 各スロットのサンプルファイルパスを
    // XML で保存する。音声データ自体は埋め込まないため、元ファイルを移動/削除
    // すると読み込み時にそのスロットだけ空になる (その旨を呼び出し側へ返す)。
    static juce::File getPresetRootDirectory();
    static juce::StringArray getPresetCategories();

    juce::File makePresetFile(const juce::String& category, const juce::String& name) const;

    bool savePreset(const juce::String& category, const juce::String& name, juce::String& errorOut);

    // 戻り値: 読み込み成功可否。missingFilesOut に見つからなかったサンプル名が入る。
    bool loadPreset(const juce::File& presetFile, juce::StringArray& missingFilesOut, juce::String& errorOut);

    // 全スロットのサンプルを破棄し、全パラメータを既定値へ戻す
    void resetToInitState();

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    SamplerEngine& getSamplerEngine() { return samplerEngine; }
    ModMatrix& getModMatrix() { return modMatrix; }
    SampleVisualizerData& getVisualizerData() { return visualizerData; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float getParamFloat(const juce::String& paramId, float defaultVal = 0.0f) const;

    juce::AudioProcessorValueTreeState apvts;
    SamplerEngine samplerEngine;
    Arpeggiator arpeggiator;
    PicoFilter mainFilter;
    PicoFilter fxBypassFilter;
    juce::ADSR filterAdsr;
    double currentSampleRate = 44100.0;
    ModMatrix modMatrix;
    FxChain fxChain;
    SampleVisualizerData visualizerData;

    struct CachedSlotParams {
        std::atomic<float>* attack;
        std::atomic<float>* decay;
        std::atomic<float>* sustain;
        std::atomic<float>* release;
        std::atomic<float>* octave;
        std::atomic<float>* pitchSt;
        std::atomic<float>* fineTune;
        std::atomic<float>* pan;
        std::atomic<float>* slotGain;
        std::atomic<float>* sampleStart;
        std::atomic<float>* sampleEnd;
        std::atomic<float>* loopStart;
        std::atomic<float>* loopEnd;
        std::atomic<float>* crossfade;
        std::atomic<float>* isLooping;
        std::atomic<float>* isStretchMode;
        std::atomic<float>* isReverse;
        std::atomic<float>* isSnap;
        std::atomic<float>* filterBypass;
        std::atomic<float>* fxBypass;
        std::atomic<float>* rootKeyOverride;
        std::atomic<float>* slotLowNote;
        std::atomic<float>* slotHighNote;
    } slotParamsCache[8];

    struct CachedArpParams {
        std::atomic<float>* enable;
        std::atomic<float>* latch;
        std::atomic<float>* sync;
        std::atomic<float>* pattern;
        std::atomic<float>* rateSync;
        std::atomic<float>* rateFree;
        std::atomic<float>* octaves;
        std::atomic<float>* offset;
        std::atomic<float>* repeat;
        std::atomic<float>* accent;
        std::atomic<float>* swing;
        std::atomic<float>* gate;
        std::atomic<float>* key;
        std::atomic<float>* scale;
    } arpParamsCache;

    struct CachedFilterParams {
        std::atomic<float>* enable;
        std::atomic<float>* cutoff;
        std::atomic<float>* res;
        std::atomic<float>* type;
        std::atomic<float>* slope;
        std::atomic<float>* formant;
        std::atomic<float>* combMix;
        std::atomic<float>* envAttack;
        std::atomic<float>* envDecay;
        std::atomic<float>* envSustain;
        std::atomic<float>* envRelease;
        std::atomic<float>* envAmt;
    } filterParamsCache;

    struct CachedFxParams {
        std::atomic<float>* type[5];
        std::atomic<float>* amount[5];
        std::atomic<float>* satAlgo;
        std::atomic<float>* satDrive;
        std::atomic<float>* satPreHz;
        std::atomic<float>* satTrim;
        std::atomic<float>* choRate;
        std::atomic<float>* choDepth;
        std::atomic<float>* choWidth;
        std::atomic<float>* dlyTime;
        std::atomic<float>* dlyFeedback;
        std::atomic<float>* dlyDuck;
        std::atomic<float>* dlyDamp;
        std::atomic<float>* frzSize;
        std::atomic<float>* frzFeedback;
        std::atomic<float>* frzDamp;
        std::atomic<float>* revDecay;
        std::atomic<float>* revShimmer;
        std::atomic<float>* revDamp;
        std::atomic<float>* revMod;
    } fxParamsCache;

    struct CachedModParams {
        std::atomic<float>* lfoWave[4];
        std::atomic<float>* lfoRate[4];
        std::atomic<float>* lfoSync[4];
        std::atomic<float>* lfoSyncRate[4];
        std::atomic<float>* envAttack[3];
        std::atomic<float>* envDecay[3];
        std::atomic<float>* envSustain[3];
        std::atomic<float>* envRelease[3];
        std::atomic<float>* envLoop[3];
        std::atomic<float>* modSrc[16];
        std::atomic<float>* modDst[16];
        std::atomic<float>* modAmt[16];
        std::atomic<float>* modUni[16];
    } modParamsCache;

    std::atomic<float>* pSamplerMode;
    std::atomic<float>* pPolyphony;
    std::atomic<float>* pActiveSlot;
    std::atomic<float>* pOutGain;
    std::atomic<float>* pMasterPitch;
    std::atomic<float>* pMasterHPF;
    std::atomic<float>* pMasterLPF;
    std::atomic<float>* pCeiling;
    std::atomic<float>* pLimRelease;
    std::atomic<float>* pFilterSlope;
    
    std::atomic<float>* pPortaEnable;
    std::atomic<float>* pPortaTime;
    std::atomic<float>* pAutoSliceEnable;
    std::atomic<float>* pSliceSensitivity;
    std::atomic<float>* pStretchMode;
    std::atomic<float>* pEdgeFadeIn  = nullptr;
    std::atomic<float>* pEdgeFadeOut = nullptr;

    void initializeParameterCache();

    juce::LinearSmoothedValue<float> smoothedCutoff { 2000.0f };
    juce::LinearSmoothedValue<float> smoothedReso   { 0.707f };
    juce::LinearSmoothedValue<float> smoothedGain   { 1.0f };

    // ローダースレッドへ渡す仕事。
    // Reanalyze は 49 本のアンカーバッファを作り直すため数秒かかる。
    // メッセージスレッドで実行すると GUI が固まるので必ずここへ積む。
    enum class JobType { Load, AutoSlice, Reanalyze };

    struct AsyncLoadJob
    {
        JobType type = JobType::Load;
        int slotIndex = 0;
        juce::File file;
        float sensitivity = 0.5f;
        int materialMode = 0;
        int rootOverride = -1;
    };
    juce::Array<AsyncLoadJob> pendingJobs;
    juce::CriticalSection jobLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PicoSamplerAudioProcessor)
};
