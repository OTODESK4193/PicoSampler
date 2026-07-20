// ==========================================
// File: SampleSlot.cpp
// スロット管理実装 (SignalsmithStretch高音質化 & 49アンカー配置)
// ==========================================
#include "SampleSlot.h"
#include "PitchAnalyzer.h"

bool SampleSlot::loadFromFile(const juce::File& file, int stretchAlgo)
{
    if (!file.existsAsFile()) return false;

    ready.store(false, std::memory_order_release);
    analyzing.store(true, std::memory_order_release);

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader)
    {
        analyzing.store(false, std::memory_order_release);
        return false;
    }

    const int numSamples = (int)reader->lengthInSamples;
    const int numCh = std::min(2, (int)reader->numChannels);

    originalBuffer.setSize(numCh, numSamples);
    reader->read(&originalBuffer, 0, numSamples, 0, true, true);

    metadata.filePath = file.getFullPathName();
    metadata.fileName = file.getFileName();
    metadata.fileSampleRate = reader->sampleRate;

    // Pitch & Feature Analysis
    auto analysis = PitchAnalyzer::analyzeOmni(originalBuffer, reader->sampleRate, PitchAnalyzer::Auto, metadata.fileName);
    metadata.rootKey = (analysis.rootNote >= 0) ? analysis.rootNote : 60;
    metadata.centsOffset = analysis.centsOffset;

    // SignalsmithStretch 高音質49音階アンカー事前生成 (ファイルSR使用)
    renderAnchors(stretchAlgo);

    analyzing.store(false, std::memory_order_release);
    ready.store(true, std::memory_order_release);
    return true;
}

void SampleSlot::reanalyze(int materialMode, int rootKeyOverride, int stretchAlgo)
{
    if (originalBuffer.getNumSamples() < 4) return;

    ready.store(false, std::memory_order_release);
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
    ready.store(false, std::memory_order_release);
    analyzing.store(false, std::memory_order_release);
    originalBuffer.setSize(0, 0);
    for (auto& b : anchorBuffers) b.setSize(0, 0);
}

void SampleSlot::copyFrom(const SampleSlot& other)
{
    ready.store(false, std::memory_order_release);
    analyzing.store(true, std::memory_order_release);

    metadata = other.metadata;
    engineSampleRate = other.engineSampleRate;
    
    originalBuffer.makeCopyOf(other.originalBuffer);
    for (int i = 0; i < kNumAnchors; ++i)
    {
        anchorBuffers[(size_t)i].makeCopyOf(other.anchorBuffers[(size_t)i]);
    }

    analyzing.store(false, std::memory_order_release);
    ready.store(true, std::memory_order_release);
}
