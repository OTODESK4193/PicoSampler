// ==========================================
// File: SampleSlot.h
// スロット管理 (アンカー49・遅延生成 & Stretch音質大幅向上)
//
// v1.1.0 での安定性強化:
//   1. writeLock       : 書き手 (メッセージスレッド / ローダースレッド) 同士を排他
//   2. 遅延アンカー生成 : ロード時に49本作らず、実際に鳴った半音だけ後から生成
//   3. メモリ予算       : 全スロット合計のアンカー使用量に上限を設ける
//   4. metaLock        : juce::String メンバの読み書きレースを防ぐ
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include "signalsmith-stretch/signalsmith-stretch.h"

class SampleSlot
{
public:
    // -24 ~ +24 半音 (全4オクターブ) のアンカーバッファでリサンプリング速度変化を完全追放
    static constexpr int kNumAnchors = 49;

    // 1ファイルあたりの取り込み上限 (秒)。
    // これを超える分は切り詰める。長大なファイルでメモリを食い潰して
    // std::bad_alloc → プロセス強制終了、という事故を防ぐための安全弁。
    static constexpr double kMaxLoadSeconds = 120.0;

    // 全スロット合計のアンカーメモリ上限 (バイト)。
    // 超えた場合、新規アンカーの生成をあきらめて「原音リサンプリング」に
    // フォールバックする (音は出続ける。Stretch の質だけ落ちる)。
    static constexpr int64_t kAnchorMemoryBudgetBytes = 768ll * 1024 * 1024;

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
    // フック。未指定時は常にfalse (中断しない)。
    bool loadFromFile(const juce::File& file, int stretchAlgo = 3,
                       std::function<bool()> shouldAbort = {});

    // 再解析。ピッチ解析をやり直し、既存アンカーを破棄して「次に鳴ったとき
    // 新しい Stretch アルゴリズムで作り直す」状態に戻す。
    // (v1.1.0 以前は49本を同期的に作り直していたため数十秒かかっていた)
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
    const Metadata& getMetadata() const noexcept { return metadata; }
    Metadata& getMetadata() noexcept { return metadata; }

    // ------------------------------------------------------------------
    // juce::String はコピー時に参照カウントを触るため、書き換え中に
    // 別スレッドから読むと解放済みメモリを掴む。文字列だけは専用の
    // SpinLock で保護し、必ずコピーを返す。
    // (GUI / プリセット保存 / 状態保存から呼ばれる)
    // ------------------------------------------------------------------
    juce::String getFilePathSafe() const noexcept
    {
        const juce::SpinLock::ScopedLockType sl(metaLock);
        return metadata.filePath;
    }

    juce::String getFileNameSafe() const noexcept
    {
        const juce::SpinLock::ScopedLockType sl(metaLock);
        return metadata.fileName;
    }

    double getFileSampleRate() const noexcept { return metadata.fileSampleRate; }

    // ==================================================================
    // アンカーバッファ (遅延生成)
    //
    // 【背景】
    // 旧実装はロード時に「原音と同じ長さのバッファ × 49本」を必ず作っていた。
    // 8秒ステレオ48kHzなら1スロット約150MB、8スロット埋めれば約1.2GB。
    // juce::AudioBuffer::setSize は確保失敗時に std::bad_alloc を投げるため、
    // 連続ドラッグ&ドロップでプロセスごと落ちる原因になっていた。
    //
    // 【現行】
    // ロード時にはアンカーを作らない。Stretch モードのボイスが実際に必要な
    // 半音を requestAnchor() で予約し、ローダースレッドが1本ずつ生成する。
    // 生成が済むまでは getAnchorBuffer() が nullptr を返すので、
    // 呼び出し側は原音リサンプリングにフォールバックする (音は途切れない)。
    // ==================================================================

    // 準備できていなければ nullptr。オーディオスレッドから安全に呼べる。
    const juce::AudioBuffer<float>* getAnchorBuffer(int stOffset) const noexcept;

    // 「この半音のアンカーが欲しい」とローダースレッドへ予約する。
    // オーディオスレッドから呼ばれるので、アトミック操作のみで完結させる。
    void requestAnchor(int stOffset) const noexcept;

    bool hasPendingAnchor() const noexcept;

    // ローダースレッド専用。予約済みアンカーを1本だけ生成する。
    // 戻り値: 何か処理した = true (呼び出し側は続けて呼ぶ)。
    bool renderPendingAnchor(const std::function<bool()>& shouldAbort);

    static int64_t getAnchorMemoryUsed() noexcept
    {
        return globalAnchorBytes.load(std::memory_order_relaxed);
    }

    // ==================================================================
    // オーディオスレッド用リードガード
    //
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
    // ------------------------------------------------------------------
    // 書き込み優先要求。
    // アンカー1本の生成には数百ミリ秒かかりうる。その間 writeLock を
    // 握られたままだと clear() 等を呼ぶメッセージスレッドが固まるため、
    // 「書きたい人が待っている」ことをフラグで伝えて生成側を即中断させる。
    // ------------------------------------------------------------------
    struct WritePriority
    {
        explicit WritePriority(std::atomic<bool>& f) noexcept : flag(f)
        {
            flag.store(true, std::memory_order_release);
        }
        ~WritePriority() noexcept { flag.store(false, std::memory_order_release); }
        std::atomic<bool>& flag;
    };

    // アンカー1本を生成して anchorBuffers[idx] に格納する。
    // 呼び出し時に writeLock を保持していること。
    bool renderSingleAnchor(int idx, int stretchAlgo, const std::function<bool()>& shouldAbort);

    // 全アンカーを破棄してメモリ計上を戻す。writeLock 保持前提。
    void releaseAnchors() noexcept;

    // loadFromFile の後処理 (Reader からバッファ構築 + 解析)
    bool finishLoad(std::unique_ptr<juce::AudioFormatReader> reader,
                    const juce::String& pathForMetadata,
                    const juce::String& nameForMetadata,
                    int stretchAlgo,
                    const std::function<bool()>& shouldAbort);

    // バッファ書き換え前に呼ぶ。ready を落とし、読み手が抜けるまで待機する。
    // 戻り値 false = 読み手が抜けなかった。この場合は書き込んではいけない。
    // (メッセージ/ローダースレッド専用。オーディオスレッドから呼んではいけない)
    bool beginBufferWrite() noexcept;

    void setMetadataStrings(const juce::String& path, const juce::String& name) noexcept
    {
        const juce::SpinLock::ScopedLockType sl(metaLock);
        metadata.filePath = path;
        metadata.fileName = name;
    }

    std::atomic<bool> ready { false };
    std::atomic<bool> analyzing { false };
    mutable std::atomic<int> readerCount { 0 };

    // 書き手同士 (メッセージスレッド ⇔ ローダースレッド) の排他。
    // ReadGuard/readerCount はオーディオスレッドの読み手向けであり、
    // こちらとは役割が別。両方必要。
    juce::CriticalSection writeLock;
    mutable std::atomic<bool> writeWanted { false };

    mutable juce::SpinLock metaLock;

    double engineSampleRate = 44100.0;

    // ロード / Reanalyze 時に確定した Stretch アルゴリズム。
    // 以降のアンカー生成はすべてこの値で行い、スロット内で音色が揃うようにする。
    int bakedStretchAlgo = 3;

    std::vector<int> onsetSamples;
    juce::AudioBuffer<float> originalBuffer;
    std::array<juce::AudioBuffer<float>, kNumAnchors> anchorBuffers;
    std::array<int64_t, kNumAnchors> anchorBytes {};
    std::array<std::atomic<bool>, kNumAnchors> anchorReady {};
    mutable std::array<std::atomic<bool>, kNumAnchors> anchorRequested {};

    Metadata metadata;

    static std::atomic<int64_t> globalAnchorBytes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleSlot)
};
