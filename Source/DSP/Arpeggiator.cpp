// ==========================================
// File: Arpeggiator.cpp
// 13パターン アルペジエイター実装 (堅牢化 ＆ バグ完全撃滅版)
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
    rebuildSequence(p.pattern, p.octaves, p.offset, p.key, p.scale);
    if (playSequence.empty()) return;

    const int stepSamples = calculateStepSamples(p, numSamples);
    const int gateSamples = calculateGateSamples(p, stepSamples);

    int samplesProcessed = 0;
    while (samplesProcessed < numSamples)
    {
        if (stepSampleCounter <= 0)
        {
            stepSampleCounter = stepSamples;

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

            // 全ての鍵盤を離した後に新しく弾いた場合、Latchバッファを自動リセット
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

    // 鍵盤が押された瞬間に即時0サンプル遅延でスタート
    if (wasEmpty && !heldNotes.empty())
    {
        stepSampleCounter = 0;
        sequenceIndex = 0;
        walkIndex = 0;
    }
}

void Arpeggiator::rebuildSequence(int pattern, int octaves, int offset, int key, int scale) noexcept
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
        secPerStep = 1.0 / (double)juce::jmax(0.1f, p.rateFreeHz);
    }

    return juce::jmax(1, (int)(secPerStep * sr));
}

int Arpeggiator::calculateGateSamples(const Params& p, int stepSamples) const noexcept
{
    if (p.durMode == GateLength)
    {
        return juce::jlimit(1, stepSamples, (int)(stepSamples * p.gatePct));
    }

    static const float fixedBeats[4] = { 0.125f, 0.25f, 0.5f, 1.0f };
    const double bpm = p.bpm > 1.0 ? p.bpm : 120.0;
    const double sec = (60.0 / bpm) * fixedBeats[juce::jlimit(0, 3, p.durMode - 1)];
    return juce::jlimit(1, stepSamples, (int)(sec * sr));
}

void Arpeggiator::stopAllActiveNotes(juce::MidiBuffer& outMidi, int sampleOffset) noexcept
{
    for (const auto& a : activeOutputs)
    {
        outMidi.addEvent(juce::MidiMessage::noteOff(1, a.noteNumber, 0.0f), sampleOffset);
    }
    activeOutputs.clear();
}
