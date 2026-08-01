// ==========================================
// File: Arpeggiator.cpp
// 13パターン アルペジエイター実装 (Sync OFF時の極小周波数停止防止 & スムーズ追従)
// ==========================================
#include "Arpeggiator.h"

void Arpeggiator::prepare(double sampleRate) noexcept
{
    sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
    reset();
}

void Arpeggiator::reset() noexcept
{
    heldNotes.clear();
    latchedNotes.clear();
    playSequence.clear();
    activeOutputs.clear();
    nextOrderId = 0;
    stepSampleCounter = 0;
    sequenceIndex = 0;
    directionUp = true;
    walkIndex = 0;
    prevSyncState = true;
    prevRateFree = 4.0f;
    prevRateSync = 6;
}

void Arpeggiator::process(juce::MidiBuffer& midi, int numSamples, const Params& p) noexcept
{
    if (!p.enable)
    {
        reset();
        return;
    }

    // 1. 入力MIDI (ユーザーの鍵盤押下・離鍵) をまず解析
    juce::MidiBuffer incomingMidi = midi;
    midi.clear(); // ユーザーの原音MIDIを出力から消去し、Arpノートに差し替える

    handleIncomingMidi(incomingMidi, p.latch);

    // 2. アクティブ出力ノートのゲート長を減衰カウントダウン
    for (auto it = activeOutputs.begin(); it != activeOutputs.end();)
    {
        it->samplesRemaining -= numSamples;
        if (it->samplesRemaining <= 0)
        {
            midi.addEvent(juce::MidiMessage::noteOff(1, it->noteNumber, 0.0f), std::max(0, numSamples - 1));
            it = activeOutputs.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 3. 発音対象ノートの確認
    const auto& sourceNotes = p.latch && heldNotes.empty() ? latchedNotes : heldNotes;

    if (sourceNotes.empty())
    {
        playSequence.clear();
        stopAllActiveNotes(midi, 0);
        stepSampleCounter = 0;
        sequenceIndex = 0;
        return;
    }

    // 4. アルペジオシーケンスのビルド
    rebuildSequence(p.pattern, p.octaves, p.offset, p.key, p.scale, p.repeat, p.accent);
    if (playSequence.empty()) return;

    const int stepSamples = calculateStepSamples(p, numSamples);
    const int gateSamples = calculateGateSamples(p, stepSamples);

    // SyncモードまたはRate値がリアルタイム変更された場合、カウントダウンを即座に安全適応
    if (p.sync != prevSyncState || p.rateFreeHz != prevRateFree || p.rateSync != prevRateSync)
    {
        prevSyncState = p.sync;
        prevRateFree = p.rateFreeHz;
        prevRateSync = p.rateSync;

        // 新しいステップ長より長く待っている場合だけ切り詰める。
        //
        // 旧実装はここで 0 にしていたため「その場で即発音」していた。
        // Rate に LFO をアサインすると値がブロックごとに変わり、
        // 速くなる方向へ動くたびにこの分岐へ入って連打状態になっていた。
        // 上限を新ステップ長に丸めるだけなら、リズムを崩さずに追従できる。
        stepSampleCounter = juce::jmin(stepSampleCounter, stepSamples);
    }

    int samplesProcessed = 0;
    while (samplesProcessed < numSamples)
    {
        if (stepSampleCounter <= 0)
        {
            // スイング (裏拍のタイミング遅延) 計算
            int currentStepSamples = stepSamples;
            if (p.swing > 0.01f && (sequenceIndex % 2 == 1))
            {
                currentStepSamples = (int)((float)stepSamples * (1.0f + juce::jlimit(0.0f, 0.75f, p.swing)));
            }

            stepSampleCounter = currentStepSamples;

            if (p.pattern == Chord)
            {
                // コードモード: 全ノート同時発音
                for (const auto& n : playSequence)
                {
                    midi.addEvent(juce::MidiMessage::noteOn(1, n.noteNumber, n.velocity), samplesProcessed);
                    activeOutputs.push_back({ n.noteNumber, gateSamples });
                }
            }
            else
            {
                if (sequenceIndex >= playSequence.size()) sequenceIndex = 0;
                const auto& n = playSequence[sequenceIndex];

                midi.addEvent(juce::MidiMessage::noteOn(1, n.noteNumber, n.velocity), samplesProcessed);
                activeOutputs.push_back({ n.noteNumber, gateSamples });

                if (p.pattern == RandomPattern)
                {
                    sequenceIndex = (size_t)rng.nextInt((int)playSequence.size());
                }
                else if (p.pattern == RandomWalk)
                {
                    int step = rng.nextBool() ? 1 : -1;
                    const int maxIdx = std::max(0, (int)playSequence.size() - 1);
                    walkIndex = juce::jlimit(0, maxIdx, walkIndex + step);
                    sequenceIndex = (size_t)walkIndex;
                }
                else
                {
                    sequenceIndex = (sequenceIndex + 1) % playSequence.size();
                }
            }
        }

        const int chunk = std::min(stepSampleCounter, numSamples - samplesProcessed);
        stepSampleCounter -= chunk;
        samplesProcessed += chunk;
    }
}

void Arpeggiator::handleIncomingMidi(const juce::MidiBuffer& midi, bool latch) noexcept
{
    const bool wasEmpty = heldNotes.empty();

    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const float vel = msg.getFloatVelocity();

            if (latch && wasEmpty)
            {
                latchedNotes.clear();
            }

            auto it = std::find_if(heldNotes.begin(), heldNotes.end(),
                                   [note](const NoteInfo& n) { return n.noteNumber == note; });
            if (it != heldNotes.end())
            {
                it->velocity = vel;
            }
            else
            {
                heldNotes.push_back({ note, vel, nextOrderId++ });
            }

            if (latch)
            {
                auto lit = std::find_if(latchedNotes.begin(), latchedNotes.end(),
                                        [note](const NoteInfo& n) { return n.noteNumber == note; });
                if (lit == latchedNotes.end())
                    latchedNotes.push_back({ note, vel, nextOrderId });
            }
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            heldNotes.erase(std::remove_if(heldNotes.begin(), heldNotes.end(),
                                           [note](const NoteInfo& n) { return n.noteNumber == note; }),
                            heldNotes.end());
        }
        else if (msg.isAllNotesOff())
        {
            heldNotes.clear();
            if (!latch) latchedNotes.clear();
        }
    }

    if (wasEmpty && !heldNotes.empty())
    {
        stepSampleCounter = 0;
        sequenceIndex = 0;
        walkIndex = 0;
    }
}

void Arpeggiator::rebuildSequence(int pattern, int octaves, int offset, int key, int scale, int repeat, float accent) noexcept
{
    const auto& src = heldNotes.empty() ? latchedNotes : heldNotes;
    if (src.empty()) { playSequence.clear(); return; }

    std::vector<NoteInfo> base = src;

    if (pattern != AsPlayed)
    {
        std::sort(base.begin(), base.end(), [](const NoteInfo& a, const NoteInfo& b) {
            return a.noteNumber < b.noteNumber;
        });
    }

    playSequence.clear();
    const int numOct = juce::jlimit(1, 4, octaves);
    const int rootNote = (key > 0) ? (key - 1) : 0;

    for (int oct = 0; oct < numOct; ++oct)
    {
        for (auto n : base)
        {
            int shiftedNote = n.noteNumber + (oct * 12) + offset;
            if (scale > 0)
            {
                shiftedNote = (int)ScaleQuantizer::quantize((float)shiftedNote, rootNote, scale);
            }
            n.noteNumber = juce::jlimit(0, 127, shiftedNote);
            playSequence.push_back(n);
        }
    }

    if (playSequence.empty()) return;

    const int seqSize = (int)playSequence.size();

    if (pattern == Down)
    {
        std::reverse(playSequence.begin(), playSequence.end());
    }
    else if (pattern == UpDown)
    {
        auto copy = playSequence;
        if (seqSize > 2)
        {
            for (int i = seqSize - 2; i > 0; --i)
                playSequence.push_back(copy[(size_t)i]);
        }
    }
    else if (pattern == DownUp)
    {
        std::reverse(playSequence.begin(), playSequence.end());
        auto copy = playSequence;
        if (seqSize > 2)
        {
            for (int i = seqSize - 2; i > 0; --i)
                playSequence.push_back(copy[(size_t)i]);
        }
    }
    else if (pattern == Converge)
    {
        std::vector<NoteInfo> conv;
        int left = 0, right = seqSize - 1;
        while (left <= right)
        {
            conv.push_back(playSequence[(size_t)left++]);
            if (left <= right) conv.push_back(playSequence[(size_t)right--]);
        }
        playSequence = conv;
    }
    else if (pattern == Diverge)
    {
        std::vector<NoteInfo> div;
        int mid = seqSize / 2;
        int left = mid, right = mid + 1;
        while (left >= 0 || right < seqSize)
        {
            if (left >= 0) div.push_back(playSequence[(size_t)left--]);
            if (right < seqSize) div.push_back(playSequence[(size_t)right++]);
        }
        playSequence = div;
    }
    else if (pattern == PedalLow)
    {
        if (seqSize > 1)
        {
            const auto pedal = playSequence[0];
            std::vector<NoteInfo> ped;
            for (size_t i = 1; i < playSequence.size(); ++i)
            {
                ped.push_back(pedal);
                ped.push_back(playSequence[i]);
            }
            playSequence = ped;
        }
    }
    else if (pattern == PedalHigh)
    {
        if (seqSize > 1)
        {
            const auto pedal = playSequence.back();
            std::vector<NoteInfo> ped;
            for (size_t i = 0; i < playSequence.size() - 1; ++i)
            {
                ped.push_back(playSequence[i]);
                ped.push_back(pedal);
            }
            playSequence = ped;
        }
    }

    if (playSequence.empty()) return;

    // Repeat (ステップリピート 1..4) の適用
    if (repeat > 1)
    {
        std::vector<NoteInfo> repSeq;
        for (const auto& n : playSequence)
        {
            for (int r = 0; r < repeat; ++r)
            {
                repSeq.push_back(n);
            }
        }
        playSequence = repSeq;
    }

    // Accent / Ramp (ベロシティグラデーション -1.0..+1.0) の適用
    if (std::abs(accent) > 0.01f && playSequence.size() > 1)
    {
        const float total = (float)playSequence.size();
        for (size_t idx = 0; idx < playSequence.size(); ++idx)
        {
            const float progress = (float)idx / std::max(1.0f, total - 1.0f);
            float factor = 1.0f;
            if (accent > 0.0f)
            {
                factor = 1.0f + progress * accent;
            }
            else
            {
                factor = 1.0f + (1.0f - progress) * accent;
            }
            playSequence[idx].velocity = juce::jlimit(0.05f, 1.0f, playSequence[idx].velocity * factor);
        }
    }
}

namespace
{
    // 音価の長い順 (遅い→速い) に並べた getSyncRateNames() のインデックス。
    //   1/1(4.0) 1/2(2.0) 1/4.(1.5) 1/2T(1.333) 1/4(1.0) 1/8.(0.75) 1/4T(0.667)
    //   1/8(0.5) 1/16.(0.375) 1/8T(0.333) 1/16(0.25) 1/16T(0.167) 1/32(0.125)
    constexpr int kSyncOrderSlowToFast[Arpeggiator::kNumSyncRates] =
        { 0, 1, 4, 2, 3, 7, 5, 6, 10, 8, 9, 11, 12 };
}

int Arpeggiator::syncRateToRank(int idx) noexcept
{
    idx = juce::jlimit(0, kNumSyncRates - 1, idx);
    for (int r = 0; r < kNumSyncRates; ++r)
        if (kSyncOrderSlowToFast[r] == idx)
            return r;
    return 0;
}

int Arpeggiator::rankToSyncRate(int rank) noexcept
{
    return kSyncOrderSlowToFast[juce::jlimit(0, kNumSyncRates - 1, rank)];
}

int Arpeggiator::calculateStepSamples(const Params& p, int) const noexcept
{
    static const double beatsTable[13] = {
        4.0, 2.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0,
        0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125
    };

    double secPerStep = 0.25;
    if (p.sync)
    {
        const double bpm = p.bpm > 1.0 ? p.bpm : 120.0;
        const double beats = beatsTable[juce::jlimit(0, 12, p.rateSync)];
        secPerStep = (60.0 / bpm) * beats;
    }
    else
    {
        // Sync OFF 時: rateFreeHz (1.0..30.0 Hz) で直感的なステップ間隔を算出
        // 最低周波数を 1.0 Hz (1秒) に下限制限して停止状態を防止
        const float freeHz = juce::jlimit(1.0f, 30.0f, p.rateFreeHz);
        secPerStep = 1.0 / (double)freeHz;
    }

    return juce::jmax(1, (int)(secPerStep * sr));
}

int Arpeggiator::calculateGateSamples(const Params& p, int stepSamples) const noexcept
{
    return juce::jlimit(1, stepSamples, (int)((float)stepSamples * juce::jlimit(0.1f, 1.0f, p.gatePct)));
}

void Arpeggiator::stopAllActiveNotes(juce::MidiBuffer& outMidi, int sampleOffset) noexcept
{
    for (const auto& a : activeOutputs)
    {
        outMidi.addEvent(juce::MidiMessage::noteOff(1, a.noteNumber, 0.0f), sampleOffset);
    }
    activeOutputs.clear();
}
