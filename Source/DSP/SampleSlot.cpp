// ==========================================
// File: SampleSlot.cpp
// SampleSlot インジェスト＆SignalStretch アンカー生成実装
// ==========================================
#include "SampleSlot.h"
#include "PitchAnalyzer.h"

bool SampleSlot::loadFromFile(const juce::File& file, int rootKeyOverride)
{
    if (!file.existsAsFile())
    {
        status.store(Status::Failed);
        return false;
    }

    status.store(Status::Loading);
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader)
    {
        status.store(Status::Failed);
        return false;
    }

    meta.filePath = file.getFullPathName();
    meta.fileName = file.getFileName();
    meta.originalSampleRate = reader->sampleRate;
    meta.numChannels = (int)reader->numChannels;
    meta.lengthInSamples = (int)reader->lengthInSamples;

    // 1. デコード＆読み込み
    juce::AudioBuffer<float> rawBuffer((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&rawBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

    // 2. ホストサンプルレートへのリサンプリング（必要な場合）
    if (std::abs(reader->sampleRate - hostSampleRate) > 1.0)
    {
        const double ratio = reader->sampleRate / hostSampleRate;
        const int newLen = (int)((double)rawBuffer.getNumSamples() / ratio);
        originalBuffer.setSize((int)reader->numChannels, newLen);

        juce::LagrangeInterpolator resampler;
        for (int ch = 0; ch < (int)reader->numChannels; ++ch)
        {
            resampler.process(ratio, rawBuffer.getReadPointer(ch), originalBuffer.getWritePointer(ch), newLen);
        }
    }
    else
    {
        originalBuffer = rawBuffer;
    }

    meta.sampleStart = 0;
    meta.sampleEnd = originalBuffer.getNumSamples();
    meta.loopStart = (int)(originalBuffer.getNumSamples() * 0.2f);
    meta.loopLength = (int)(originalBuffer.getNumSamples() * 0.5f);

    // 3. ルートキー解析
    status.store(Status::Analyzing);
    if (rootKeyOverride >= 0)
    {
        meta.rootKey = rootKeyOverride;
    }
    else
    {
        auto analysis = PitchAnalyzer::analyzeOmni(originalBuffer, hostSampleRate, PitchAnalyzer::Auto, meta.fileName);
        meta.rootKey = analysis.rootNote;
        meta.centsOffset = analysis.centsOffset;
    }

    // 4. SignalStretch による24アンカーバッファの生成 (-12 ~ +11 半音)
    status.store(Status::Stretching);
    generateAnchorsSignalStretch();

    status.store(Status::Ready);
    return true;
}

void SampleSlot::generateAnchorsSignalStretch()
{
    const int numCh = originalBuffer.getNumChannels();
    const int numSamples = originalBuffer.getNumSamples();
    if (numSamples == 0 || numCh == 0) return;

    for (int i = 0; i < kNumAnchors; ++i)
    {
        const int semitones = i - 12; // -12 ~ +11

        if (semitones == 0)
        {
            // 0半音（ルート）はそのままコピー
            anchorBuffers[(size_t)i] = originalBuffer;
            continue;
        }

        // SignalStretch によるオフラインピッチシフト
        signalsmith::stretch::SignalsmithStretch<float> stretch;
        stretch.presetDefault(numCh, (float)hostSampleRate);

        const float pitchScale = std::pow(2.0f, (float)semitones / 12.0f);
        stretch.setTransposeFactor(pitchScale);

        // SignalStretch 入出力用ポインタ配列
        std::vector<const float*> inputs((size_t)numCh);
        for (int ch = 0; ch < numCh; ++ch) inputs[(size_t)ch] = originalBuffer.getReadPointer(ch);

        // 出力長の設定
        const int outLen = numSamples;
        anchorBuffers[(size_t)i].setSize(numCh, outLen);

        std::vector<float*> outputs((size_t)numCh);
        for (int ch = 0; ch < numCh; ++ch) outputs[(size_t)ch] = anchorBuffers[(size_t)i].getWritePointer(ch);

        stretch.process(inputs.data(), numSamples, outputs.data(), outLen);
    }
}
