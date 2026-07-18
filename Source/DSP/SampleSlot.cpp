// ==========================================
// File: SampleSlot.cpp
// スロット管理実装 (reanalyze 実装)
// ==========================================
#include "SampleSlot.h"
#include "PitchAnalyzer.h"

bool SampleSlot::loadFromFile(const juce::File& file)
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

    // SignalsmithStretch 24音階アンカー事前生成 (ファイルSR使用)
    renderAnchors();

    analyzing.store(false, std::memory_order_release);
    ready.store(true, std::memory_order_release);
    return true;
}

void SampleSlot::reanalyze(int materialMode, int rootKeyOverride)
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

    renderAnchors();

    analyzing.store(false, std::memory_order_release);
    ready.store(true, std::memory_order_release);
}

void SampleSlot::renderAnchors()
{
    const int numSamples = originalBuffer.getNumSamples();
    const int numCh = originalBuffer.getNumChannels();
    const double sr = metadata.fileSampleRate > 1000.0 ? metadata.fileSampleRate : 44100.0;

    for (int i = 0; i < kNumAnchors; ++i)
    {
        const int stOffset = i - 12; // -12 ~ +11 半音

        signalsmith::stretch::SignalsmithStretch<float> stretch;
        stretch.presetDefault(numCh, (float)sr);
        stretch.setTransposeSemitones((float)stOffset);

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
    const int anchorIdx = juce::jlimit(0, kNumAnchors - 1, stOffset + 12);
    return &anchorBuffers[(size_t)anchorIdx];
}

void SampleSlot::clear()
{
    ready.store(false, std::memory_order_release);
    analyzing.store(false, std::memory_order_release);
    originalBuffer.setSize(0, 0);
    for (auto& b : anchorBuffers) b.setSize(0, 0);
}
