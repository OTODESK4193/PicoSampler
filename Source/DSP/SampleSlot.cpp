// ==========================================
// File: SampleSlot.cpp
// スロット管理実装 (SignalsmithStretch高音質化 & 49アンカー遅延生成)
// ==========================================
#include "SampleSlot.h"
#include <new>
#include <thread>
#include "PitchAnalyzer.h"

std::atomic<int64_t> SampleSlot::globalAnchorBytes { 0 };

bool SampleSlot::beginBufferWrite() noexcept
{
    // まず新規の読み手を締め出す
    ready.store(false, std::memory_order_release);

    // 既に読み始めている手が抜けるのを待つ。
    // 実際には 1 オーディオブロック分 (数ミリ秒) で必ず抜けるため、
    // ここでの待機は一瞬。念のため上限を設けて無限ループは避ける。
    for (int spin = 0; spin < 2000; ++spin)
    {
        if (readerCount.load(std::memory_order_acquire) == 0)
            return true;

        if (spin < 64) std::this_thread::yield();
        else           juce::Thread::sleep(1);
    }

    // 読み手が抜けない = どこかでガードを持ち逃げしている。
    // ここで書き込みを強行すると解放済みメモリを読ませてしまうため、
    // 「書けなかった」と返して呼び出し側にあきらめさせる。
    jassertfalse;
    return false;
}

void SampleSlot::releaseAnchors() noexcept
{
    for (int i = 0; i < kNumAnchors; ++i)
    {
        if (anchorReady[(size_t)i].exchange(false, std::memory_order_acq_rel))
            globalAnchorBytes.fetch_sub(anchorBytes[(size_t)i], std::memory_order_relaxed);

        anchorRequested[(size_t)i].store(false, std::memory_order_release);
        anchorBytes[(size_t)i] = 0;
        anchorBuffers[(size_t)i].setSize(0, 0);
    }
}

bool SampleSlot::loadFromFile(const juce::File& file, int stretchAlgo, std::function<bool()> shouldAbort)
{
    if (!file.existsAsFile()) return false;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    return finishLoad(std::move(reader), file.getFullPathName(), file.getFileName(), stretchAlgo, shouldAbort);
}

bool SampleSlot::finishLoad(std::unique_ptr<juce::AudioFormatReader> reader,
                            const juce::String& pathForMetadata,
                            const juce::String& nameForMetadata,
                            int stretchAlgo,
                            const std::function<bool()>& shouldAbort)
{
    if (reader == nullptr) return false;

    const double fileSR = reader->sampleRate > 1000.0 ? reader->sampleRate : 44100.0;

    // 長大ファイルの安全弁。上限を超えた分は切り詰める。
    const int64_t maxSamples = (int64_t)(kMaxLoadSeconds * fileSR);
    const int64_t lenInFile  = (int64_t)reader->lengthInSamples;
    const int numSamples = (int)(lenInFile < maxSamples ? lenInFile : maxSamples);
    const int numCh = juce::jmin(2, (int)reader->numChannels);

    if (numSamples < 4 || numCh < 1) return false;

    // 書き手同士の排他。clear() / reanalyze() / アンカー生成と同時に走らせない。
    const WritePriority priority(writeWanted);
    const juce::ScopedLock writeGuard(writeLock);

    if (!beginBufferWrite()) return false;
    analyzing.store(true, std::memory_order_release);

    // 旧サンプルのアンカーはこの時点で無効。必ず先に解放してメモリを空ける。
    releaseAnchors();
    bakedStretchAlgo = stretchAlgo;

    try
    {
        originalBuffer.setSize(numCh, numSamples);
    }
    catch (const std::bad_alloc&)
    {
        // メモリ確保に失敗。落とさずに「空スロット」として続行する。
        originalBuffer.setSize(0, 0);
        analyzing.store(false, std::memory_order_release);
        return false;
    }

    reader->read(&originalBuffer, 0, numSamples, 0, true, true);

    setMetadataStrings(pathForMetadata, nameForMetadata);
    metadata.fileSampleRate = fileSR;

    // Pitch & Feature Analysis
    auto analysis = PitchAnalyzer::analyzeOmni(originalBuffer, fileSR, PitchAnalyzer::Auto, nameForMetadata);
    metadata.rootKey = (analysis.rootNote >= 0) ? analysis.rootNote : 60;
    metadata.centsOffset = analysis.centsOffset;

    // ---------------------------------------------------------------
    // 【重要】 プラグイン終了などで中断要求が来ていたら、ここで打ち切る。
    // analyzing/ready はどちらも false のままにしておく (中断時は「未完了」扱い)。
    // ---------------------------------------------------------------
    if (shouldAbort && shouldAbort())
    {
        analyzing.store(false, std::memory_order_release);
        return false;
    }

    // トランジェント検出
    calculateTransients(0.5f);

    // アンカー (49本) はここでは作らない。
    // Stretch モードのボイスが実際に必要とした半音だけ、
    // requestAnchor() → renderPendingAnchor() で後から生成する。

    analyzing.store(false, std::memory_order_release);
    ready.store(true, std::memory_order_release);
    return true;
}

void SampleSlot::reanalyze(int materialMode, int rootKeyOverride, int stretchAlgo, std::function<bool()> shouldAbort)
{
    const WritePriority priority(writeWanted);
    const juce::ScopedLock writeGuard(writeLock);

    if (originalBuffer.getNumSamples() < 4) return;

    if (!beginBufferWrite()) return;
    analyzing.store(true, std::memory_order_release);

    if (rootKeyOverride >= 0)
    {
        metadata.rootKey = rootKeyOverride;
        metadata.centsOffset = 0.0f;
    }
    else
    {
        const juce::String nameCopy = getFileNameSafe();
        auto analysis = PitchAnalyzer::analyzeOmni(originalBuffer, metadata.fileSampleRate, materialMode, nameCopy);
        metadata.rootKey = (analysis.rootNote >= 0) ? analysis.rootNote : 60;
        metadata.centsOffset = analysis.centsOffset;
    }

    if (shouldAbort && shouldAbort())
    {
        analyzing.store(false, std::memory_order_release);
        return;
    }

    // Stretch アルゴリズムを焼き直す。既存アンカーは破棄し、
    // 次に鳴った半音から新しいアルゴリズムで作り直される。
    releaseAnchors();
    bakedStretchAlgo = stretchAlgo;

    analyzing.store(false, std::memory_order_release);
    ready.store(true, std::memory_order_release);
}

// ======================================================================
// アンカー (遅延生成)
// ======================================================================

const juce::AudioBuffer<float>* SampleSlot::getAnchorBuffer(int stOffset) const noexcept
{
    const int idx = juce::jlimit(0, kNumAnchors - 1, stOffset + 24);

    if (!anchorReady[(size_t)idx].load(std::memory_order_acquire))
        return nullptr;

    return &anchorBuffers[(size_t)idx];
}

void SampleSlot::requestAnchor(int stOffset) const noexcept
{
    const int idx = juce::jlimit(0, kNumAnchors - 1, stOffset + 24);

    if (!anchorReady[(size_t)idx].load(std::memory_order_relaxed))
        anchorRequested[(size_t)idx].store(true, std::memory_order_release);
}

bool SampleSlot::hasPendingAnchor() const noexcept
{
    if (!ready.load(std::memory_order_relaxed)) return false;

    for (int i = 0; i < kNumAnchors; ++i)
    {
        if (anchorRequested[(size_t)i].load(std::memory_order_relaxed)
            && !anchorReady[(size_t)i].load(std::memory_order_relaxed))
            return true;
    }
    return false;
}

bool SampleSlot::renderPendingAnchor(const std::function<bool()>& shouldAbort)
{
    // 書きたい人が待っているなら譲る (メッセージスレッドを固めない)
    if (writeWanted.load(std::memory_order_acquire)) return false;

    const juce::ScopedTryLock writeGuard(writeLock);
    if (!writeGuard.isLocked()) return false;

    if (!ready.load(std::memory_order_acquire)) return false;
    if (originalBuffer.getNumSamples() < 4) return false;

    int idx = -1;
    for (int i = 0; i < kNumAnchors; ++i)
    {
        if (anchorRequested[(size_t)i].load(std::memory_order_acquire)
            && !anchorReady[(size_t)i].load(std::memory_order_acquire))
        {
            idx = i;
            break;
        }
    }

    if (idx < 0) return false;

    // 予約は先に降ろす。生成に失敗しても無限リトライにならないようにする。
    anchorRequested[(size_t)idx].store(false, std::memory_order_release);

    const int64_t needBytes = (int64_t)originalBuffer.getNumSamples()
                            * (int64_t)originalBuffer.getNumChannels()
                            * (int64_t)sizeof(float);

    // メモリ予算超過。生成をあきらめ、呼び出し側は原音リサンプリングを続ける。
    if (globalAnchorBytes.load(std::memory_order_relaxed) + needBytes > kAnchorMemoryBudgetBytes)
        return true;

    if (!renderSingleAnchor(idx, bakedStretchAlgo, shouldAbort))
        return true;

    anchorBytes[(size_t)idx] = needBytes;
    globalAnchorBytes.fetch_add(needBytes, std::memory_order_relaxed);
    anchorReady[(size_t)idx].store(true, std::memory_order_release);

    return true;
}

bool SampleSlot::renderSingleAnchor(int idx, int stretchAlgo, const std::function<bool()>& shouldAbort)
{
    const int numSamples = originalBuffer.getNumSamples();
    const int numCh = originalBuffer.getNumChannels();
    const double sr = metadata.fileSampleRate > 1000.0 ? metadata.fileSampleRate : 44100.0;

    if (numSamples < 4 || numCh < 1) return false;

    // 中断条件: 明示的な中断要求 or 書き込み待ちの相手がいる
    auto abort = [this, &shouldAbort]() noexcept
    {
        if (writeWanted.load(std::memory_order_acquire)) return true;
        return shouldAbort && shouldAbort();
    };

    if (abort()) return false;

    const int stOffset = idx - 24; // -24 ~ +24 半音 (全4オクターブ)

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

    try
    {
        anchorBuffers[(size_t)idx].setSize(numCh, numSamples);
    }
    catch (const std::bad_alloc&)
    {
        anchorBuffers[(size_t)idx].setSize(0, 0);
        return false;
    }

    anchorBuffers[(size_t)idx].clear();

    const int blockSize = 256;
    int readPos = 0;
    int chunkCounter = 0;

    std::vector<const float*> inputPtrs((size_t)numCh);
    std::vector<float*> outputPtrs((size_t)numCh);

    while (readPos < numSamples)
    {
        // 長尺サンプルは1アンカーの処理だけでも時間がかかりうるため、
        // 一定チャンクごとに中断要求をチェックする (約256*256=65536サンプルおき)。
        if ((++chunkCounter & 0xFF) == 0 && abort())
        {
            anchorBuffers[(size_t)idx].setSize(0, 0);
            return false;
        }

        const int currentBlock = std::min(blockSize, numSamples - readPos);

        for (int ch = 0; ch < numCh; ++ch)
        {
            inputPtrs[(size_t)ch] = originalBuffer.getReadPointer(ch, readPos);
            outputPtrs[(size_t)ch] = anchorBuffers[(size_t)idx].getWritePointer(ch, readPos);
        }

        stretch.process(inputPtrs.data(), currentBlock, outputPtrs.data(), currentBlock);
        readPos += currentBlock;
    }

    return true;
}

void SampleSlot::clear()
{
    const WritePriority priority(writeWanted);
    const juce::ScopedLock writeGuard(writeLock);

    if (!beginBufferWrite()) return;

    analyzing.store(false, std::memory_order_release);
    originalBuffer.setSize(0, 0);
    onsetSamples.clear();
    releaseAnchors();

    setMetadataStrings({}, {});
}

void SampleSlot::copyFrom(const SampleSlot& other)
{
    if (this == &other) return;   // 自己代入は何もしない

    // コピー元が他スレッドから消されないよう読み手として登録しておく。
    // (掴めなければコピー元は無効なので何もしない)
    const ReadGuard srcGuard(other);
    if (!srcGuard.isValid()) return;

    const WritePriority priority(writeWanted);
    const juce::ScopedLock writeGuard(writeLock);

    if (!beginBufferWrite()) return;
    analyzing.store(true, std::memory_order_release);

    releaseAnchors();

    const juce::String pathCopy = other.getFilePathSafe();
    const juce::String nameCopy = other.getFileNameSafe();

    metadata.rootKey        = other.metadata.rootKey;
    metadata.centsOffset    = other.metadata.centsOffset;
    metadata.fileSampleRate = other.metadata.fileSampleRate;
    setMetadataStrings(pathCopy, nameCopy);

    engineSampleRate = other.engineSampleRate;
    bakedStretchAlgo = other.bakedStretchAlgo;
    onsetSamples = other.onsetSamples;

    try
    {
        originalBuffer.makeCopyOf(other.originalBuffer);
    }
    catch (const std::bad_alloc&)
    {
        originalBuffer.setSize(0, 0);
        analyzing.store(false, std::memory_order_release);
        return;
    }

    // アンカーはコピーしない。必要になった時点でこのスロット用に生成する。
    // (AutoSlice で8スロットぶん一気にコピーすると旧実装は約1.2GB消費していた)

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
    if (numCh < 1) return;

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
