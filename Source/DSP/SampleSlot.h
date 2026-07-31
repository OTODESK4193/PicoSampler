// ==========================================
// File: SampleSlot.h
// スロット管理 (アンカー49化 & Stretch音質大幅向上)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <functional>
#include "signalsmith-stretch/signalsmith-stretch.h"

class SampleSlot
{
public:
    // -24 ~ +24 半音 (全4オクターブ) のアンカーバッファでリサンプリング速度変化を完全追放
    static constexpr int kNumAnchors = 49;

    struct Metadata
    {
        juce::String filePath;
        juce::String fileName;
        int rootKey = 60;
        float centsOffset = 0.0f;
        double fileSampleRate = 44100.0;
        float sampleStartRatio = 0.0f;
        float sampleEndRatio = 1.0f;
        float loopStartRatio = 0.2f;
        float loopLengthRatio = 0.5f;
        float crossfadeRatio = 0.05f;
        bool isLooping = false;
        bool isReverse = false;
    };

    SampleSlot() = default;
    ~SampleSlot() = default;

    void prepare(double sr) noexcept { engineSampleRate = sr > 1000.0 ? sr : 44100.0; }

    // shouldAbort: ローダースレッドがプラグイン終了時などに即座に処理を打ち切れるようにする
    // フック。renderAnchors() は49アンカー×時間伸縮という重い処理のため、これが無いと
    // 「終了操作をしてからこの関数が戻るまで」ずっと処理し続けてしまい、その間に
    // 呼び出し元 (SamplerEngine/Processor) が破棄されると解放済みメモリへの書き込みで
    // クラッシュする。未指定時は常にfalse (中断しない=従来通り)。
    bool loadFromFile(const juce::File& file, int stretchAlgo = 3,
                       std::function<bool()> shouldAbort = {});

    void reanalyze(int materialMode = 0, int rootKeyOverride = -1, int stretchAlgo = 3,
                   std::function<bool()> shouldAbort = {});
    void copyFrom(const SampleSlot& other);
    void clear();

    void calculateTransients(float sensitivity = 0.5f);
    const std::vector<int>& getOnsetSamples() const noexcept { return onsetSamples; }

    bool isReady() const noexcept { return ready.load(std::memory_order_relaxed); }
    bool isAnalyzing() const noexcept { return analyzing.load(std::memory_order_relaxed); }
    void setAnalyzing(bool v) noexcept { analyzing.store(v, std::memory_order_release); }

    const juce::AudioBuffer<float>& getOriginalBuffer() const noexcept { return originalBuffer; }
    const juce::AudioBuffer<float>* getAnchorBuffer(int stOffset) const noexcept;
    const Metadata& getMetadata() const noexcept { return metadata; }
    Metadata& getMetadata() noexcept { return metadata; }

    double getFileSampleRate() const noexcept { return metadata.fileSampleRate; }

    // ==================================================================
    // オーディオスレッド用リードガード
    //
    // 【背景】
    // ボイスは isReady() を確認したあと originalBuffer / anchorBuffers の
    // 生ポインタを掴んだまま 1 ブロック分を読み続ける。
    // その最中にローダースレッドが clear() / loadFromFile() / copyFrom() を
    // 呼ぶとバッファが解放・再確保され、解放済みメモリを読んでクラッシュする。
    //
    // 【対策】
    // 読み手はガードで readerCount を増やしてから ready を再確認する。
    // 書き手は ready=false にした後、readerCount が 0 になるまで待ってから
    // バッファを触る。オーディオスレッドは待たない (掴めなければ即諦める)
    // ので、リアルタイム性は壊さない。
    // ==================================================================
    class ReadGuard
    {
    public:
        explicit ReadGuard(const SampleSlot& s) noexcept : slot(&s)
        {
            slot->readerCount.fetch_add(1, std::memory_order_acquire);
            valid = slot->ready.load(std::memory_order_acquire);
            if (!valid)
            {
                slot->readerCount.fetch_sub(1, std::memory_order_release);
                slot = nullptr;
            }
        }

        ~ReadGuard() noexcept
        {
            if (slot != nullptr)
                slot->readerCount.fetch_sub(1, std::memory_order_release);
        }

        bool isValid() const noexcept { return valid; }

        ReadGuard(const ReadGuard&) = delete;
        ReadGuard& operator=(const ReadGuard&) = delete;

    private:
        const SampleSlot* slot = nullptr;
        bool valid = false;
    };

private:
    void renderAnchors(int stretchAlgo, const std::function<bool()>& shouldAbort);

    // loadFromFile の後処理 (Reader からバッファ構築 + 解析)
    bool finishLoad(std::unique_ptr<juce::AudioFormatReader> reader,
                    const juce::String& pathForMetadata,
                    const juce::String& nameForMetadata,
                    int stretchAlgo,
                    const std::function<bool()>& shouldAbort);

    // バッファ書き換え前に呼ぶ。ready を落とし、読み手が抜けるまで待機する。
    // (メッセージ/ローダースレッド専用。オーディオスレッドから呼んではいけない)
    void beginBufferWrite() noexcept;

    std::atomic<bool> ready { false };
    std::atomic<bool> analyzing { false };
    mutable std::atomic<int> readerCount { 0 };

    double engineSampleRate = 44100.0;
    
    std::vector<int> onsetSamples;
    juce::AudioBuffer<float> originalBuffer;
    std::array<juce::AudioBuffer<float>, kNumAnchors> anchorBuffers;
    Metadata metadata;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleSlot)
};
