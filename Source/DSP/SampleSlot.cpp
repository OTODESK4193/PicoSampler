// ==========================================
// File: SampleSlot.cpp
// スロット管理実装 (SignalsmithStretch高音質化 & 49アンカー配置)
// ==========================================
#include "SampleSlot.h"
#include <thread>
#include "PitchAnalyzer.h"

void SampleSlot::beginBufferWrite() noexcept
{
    // まず新規の読み手を締め出す
    ready.store(false, std::memory_order_release);

    // 既に読み始めている手が抜けるのを待つ。
    // 実際には 1 オーディオブロック分 (数ミリ秒) で必ず抜けるため、
    // ここでの待機は一瞬。念のため上限を設けて無限ループは避ける。
    for (int spin = 0; spin < 2000; ++spin)
    {
        if (readerCount.load(std::memory_order_acquire) == 0)
            return;

        if (spin < 64) std::this_thread::yield();
        else           juce::Thread::sleep(1);
    }

    jassertfalse; // 読み手が抜けない = どこかでガードを持ち逃げしている
}

bool SampleSlot::loadFromFile(const juce::File& file, int stretchAlgo)
{
    if (!file.existsAsFile()) return false;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    return finishLoad(std::move(reader), file.getFullPathName(), file.getFileName(), stretchAlgo);
}

bool SampleSlot::finishLoad(std::unique_ptr<juce::AudioFormatReader> reader,
                            const juce::String& pathForMetadata,
                            const juce::String& nameForMetadata,
                            int stretchAlgo)
{
    if (reader == nullptr) return false;

    const int numSamples = (int)reader->lengthInSamples;
    const int numCh = std::min(2, (int)reader->numChannels);

    if (numSamples < 4 || numCh < 1) return false;

    beginBufferWrite();
    analyzing.store(true, std::memory_order_release);

    originalBuffer.setSize(numCh, numSamples);
    reader->read(&originalBuffer, 0, numSamples, 0, true, true);

    metadata.filePath = pathForMetadata;
    metadata.fileName = nameForMetadata;
    metadata.fileSampleRate = reader->sampleRate;

    // Pitch & Feature Analysis
    auto analysis = PitchAnalyzer::analyzeOmni(originalBuffer, reader->sampleRate, PitchAnalyzer::Auto, metadata.fileName);
    metadata.rootKey = (analysis.rootNote >= 0) ? analysis.rootNote : 60;
    metadata.centsOffset = analysis.centsOffset;

    // SignalsmithStretch 高音質49音階アンカー事前生成 (ファイルSR使用)
    renderAnchors(stretchAlgo);

    // トランジェント検出
    calculateTransients(0.5f);

    analyzing.store(false, std::memory_order_release);
    ready.store(true, std::memory_order_release);
    return true;
}

void SampleSlot::reanalyze(int materialMode, int rootKeyOverride, int stretchAlgo)
{
    if (originalBuffer.getNumSamples() < 4) return;

    beginBufferWrite();
    analyzing.store(true, std::memory_order_release);

    if (rootKeyOverride >= 0)
    {
        metadata.rootKey = rootKeyOverride;
        metadata.centsOffset = 0.0f;
    }
    else
    {
        auto analysis = PitchAnalyzer::analyzeOmni(originalBuffer, metadata.fileSampleRate, materialMode, metadata.fileName);
        metadata.rootKey = (analysis.rootNote >= 0) ? analysis.rootNote : 60;
        metadata.centsOffset = analysis.centsOffset;
    }

    renderAnchors(stretchAlgo);

    analyzing.store(false, std::memory_order_release);
    ready.store(true, std::memory_order_release);
}

void SampleSlot::renderAnchors(int stretchAlgo)
{
    const int numSamples = originalBuffer.getNumSamples();
    const int numCh = originalBuffer.getNumChannels();
    const double sr = metadata.fileSampleRate > 1000.0 ? metadata.fileSampleRate : 44100.0;

    for (int i = 0; i < kNumAnchors; ++i)
    {
        const int stOffset = i - 24; // -24 ~ +24 半音 (全4オクターブ)

        signalsmith::stretch::SignalsmithStretch<float> stretch;
        
        int blockSamples = static_cast<int>(sr * 0.06);   // 約60ms
        int intervalSamples = static_cast<int>(sr * 0.015); // 約15ms

        if (stretchAlgo == 0) // Beat
        {
            blockSamples = static_cast<int>(sr * 0.03);
            intervalSamples = static_cast<int>(sr * 0.01);
        }
        else if (stretchAlgo == 1) // Tone
        {
            blockSamples = static_cast<int>(sr * 0.08);
            intervalSamples = static_cast<int>(sr * 0.02);
        }
        else if (stretchAlgo == 2) // Texture
        {
            blockSamples = static_cast<int>(sr * 0.12);
            intervalSamples = static_cast<int>(sr * 0.03);
        }

        stretch.configure(numCh, blockSamples, intervalSamples, false);
        stretch.setTransposeSemitones((float)stOffset, 0.35f);

        anchorBuffers[(size_t)i].setSize(numCh, numSamples);
        anchorBuffers[(size_t)i].clear();

        const int blockSize = 256;
        int readPos = 0;

        while (readPos < numSamples)
        {
            const int currentBlock = std::min(blockSize, numSamples - readPos);
            std::vector<const float*> inputPtrs((size_t)numCh);
            std::vector<float*> outputPtrs((size_t)numCh);

            for (int ch = 0; ch < numCh; ++ch)
            {
                inputPtrs[(size_t)ch] = originalBuffer.getReadPointer(ch, readPos);
                outputPtrs[(size_t)ch] = anchorBuffers[(size_t)i].getWritePointer(ch, readPos);
            }

            stretch.process(inputPtrs.data(), currentBlock, outputPtrs.data(), currentBlock);
            readPos += currentBlock;
        }
    }
}

const juce::AudioBuffer<float>* SampleSlot::getAnchorBuffer(int stOffset) const noexcept
{
    const int anchorIdx = juce::jlimit(0, kNumAnchors - 1, stOffset + 24);
    return &anchorBuffers[(size_t)anchorIdx];
}

void SampleSlot::clear()
{
    beginBufferWrite();
    analyzing.store(false, std::memory_order_release);
    originalBuffer.setSize(0, 0);
    for (auto& b : anchorBuffers) b.setSize(0, 0);
}

void SampleSlot::copyFrom(const SampleSlot& other)
{
    if (this == &other) return;   // 自己代入は何もしない

    beginBufferWrite();
    analyzing.store(true, std::memory_order_release);

    metadata = other.metadata;
    engineSampleRate = other.engineSampleRate;
    onsetSamples = other.onsetSamples;
    
    originalBuffer.makeCopyOf(other.originalBuffer);
    for (int i = 0; i < kNumAnchors; ++i)
    {
        anchorBuffers[(size_t)i].makeCopyOf(other.anchorBuffers[(size_t)i]);
    }

    analyzing.store(false, std::memory_order_release);
    ready.store(true, std::memory_order_release);
}

void SampleSlot::calculateTransients(float sensitivity)
{
    onsetSamples.clear();
    const int numSamples = originalBuffer.getNumSamples();
    if (numSamples < 100) return;

    onsetSamples.push_back(0); 
    const int windowSize = 512;
    const int numCh = originalBuffer.getNumChannels();
    
    float maxEnergy = 0.0f;
    for (int i = 0; i < numSamples - windowSize; i += windowSize)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < numCh; ++ch) {
            const float* data = originalBuffer.getReadPointer(ch, i);
            for (int s = 0; s < windowSize; ++s) sum += data[s] * data[s];
        }
        maxEnergy = std::max(maxEnergy, sum / (windowSize * numCh));
    }

    const float threshold = maxEnergy * juce::jmap(sensitivity, 0.0f, 1.0f, 0.3f, 0.01f);
    
    float currentEnergy = 0.0f;
    float prevEnergy = 0.0f;
    
    for (int i = 0; i < numSamples - windowSize; i += windowSize / 2)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* data = originalBuffer.getReadPointer(ch, i);
            for (int s = 0; s < windowSize; ++s) sum += data[s] * data[s];
        }
        currentEnergy = sum / (windowSize * numCh);
        
        if (currentEnergy > prevEnergy + threshold && currentEnergy > maxEnergy * 0.05f)
        {
            if (onsetSamples.empty() || i - onsetSamples.back() > 4410)
            {
                onsetSamples.push_back(i);
            }
        }
        prevEnergy = currentEnergy;
    }
}
