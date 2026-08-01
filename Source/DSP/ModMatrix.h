// ==========================================
// File: ModMatrix.h
// モジュレーションマトリクス（PicoSampler専用・ブロックレート処理）
//
//  Sources : LFO×4 (テンポ同期/フリー, Sine/Tri/Saw/Sqr/S&H/Chaos)
//            ENV×3 (ADSR + Loop)
//            Velocity, Note, ModWheel, Random(ノート毎S&H)
//  Slots   : 16 ( Source × Amount(-1..+1) → Destination, Uni/Bipolar )
//  Dests   : 各スロットSample全ノブ(S1..S8 Start/End/L-Start/L-End/X-Fade),
//            Master Pitch, ARP全ノブ, Filter全ノブ(ADSR除く), FX全ノブ
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

class ModMatrix
{
public:
    static constexpr int kNumLfos = 4;
    static constexpr int kNumEnvs = 3;
    static constexpr int kNumSlots = 16;

    enum Src
    {
        SrcNone = 0,
        SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4,
        SrcEnv1, SrcEnv2, SrcEnv3,
        SrcVelocity, SrcNote, SrcModWheel, SrcRandom,
        NumSrcs
    };

    enum Dst
    {
        DstNone = 0,
        
        // Slot 1..8 Sample エリア (5ノブ×8 = 40個)
        DstS1Start, DstS1End, DstS1LStart, DstS1LEnd, DstS1XFade,
        DstS2Start, DstS2End, DstS2LStart, DstS2LEnd, DstS2XFade,
        DstS3Start, DstS3End, DstS3LStart, DstS3LEnd, DstS3XFade,
        DstS4Start, DstS4End, DstS4LStart, DstS4LEnd, DstS4XFade,
        DstS5Start, DstS5End, DstS5LStart, DstS5LEnd, DstS5XFade,
        DstS6Start, DstS6End, DstS6LStart, DstS6LEnd, DstS6XFade,
        DstS7Start, DstS7End, DstS7LStart, DstS7LEnd, DstS7XFade,
        DstS8Start, DstS8End, DstS8LStart, DstS8LEnd, DstS8XFade,

        // Master
        DstMasterPitch,

        // ARP 全ノブ (7個)
        DstArpOctaves, DstArpRate, DstArpGate, DstArpOffset, DstArpSwing, DstArpRepeat, DstArpAccent,

        // Filter 全ノブ (ADSR除く 4個)
        DstFltCutoff, DstFltReso, DstFltFormant, DstFltCombMix,

        // FX1..5 Amount (5個)
        DstFx1Amount, DstFx2Amount, DstFx3Amount, DstFx4Amount, DstFx5Amount,

        // FX 詳細パラメータ (16個)
        DstSatDrive, DstSatPreHz, DstSatTrim,
        DstChoRate, DstChoDepth, DstChoWidth,
        DstDlyFeedback, DstDlyDuck, DstDlyDamp,
        DstFrzSize, DstFrzFeedback, DstFrzDamp,
        DstRevDecay, DstRevShimmer, DstRevDamp, DstRevMod,

        // ---------------------------------------------------------------
        // Slot 1..8 Pan (8個)
        //
        // 【重要】新しい行き先は必ず NumDsts の直前に追記すること。
        // Dst の値はプリセットや DAW セッションに「番号」として保存される。
        // 途中に挿入すると既存プロジェクトのアサイン先が全部ズレる。
        // (そのため Pan は S1..S8 の他のノブとは離れた位置にある)
        // ---------------------------------------------------------------
        DstS1Pan, DstS2Pan, DstS3Pan, DstS4Pan,
        DstS5Pan, DstS6Pan, DstS7Pan, DstS8Pan,

        // LFO 1..4 Rate (Hz) — LFO同士のクロスモジュレーション用。
        // (末尾追記。上の注意書き参照)
        DstLfo1Rate, DstLfo2Rate, DstLfo3Rate, DstLfo4Rate,

        // Filter Envelope (ADSR)。元は「ADSR除く」で意図的に外していたが、
        // MODアサイン先として使いたいという要望により追加。(末尾追記)
        DstFltEnvAttack, DstFltEnvDecay, DstFltEnvSustain, DstFltEnvRelease,

        // Slot 1..8 Amp ADSR (各スロットの音量エンベロープ。32個。末尾追記)
        DstS1AmpAttack, DstS1AmpDecay, DstS1AmpSustain, DstS1AmpRelease,
        DstS2AmpAttack, DstS2AmpDecay, DstS2AmpSustain, DstS2AmpRelease,
        DstS3AmpAttack, DstS3AmpDecay, DstS3AmpSustain, DstS3AmpRelease,
        DstS4AmpAttack, DstS4AmpDecay, DstS4AmpSustain, DstS4AmpRelease,
        DstS5AmpAttack, DstS5AmpDecay, DstS5AmpSustain, DstS5AmpRelease,
        DstS6AmpAttack, DstS6AmpDecay, DstS6AmpSustain, DstS6AmpRelease,
        DstS7AmpAttack, DstS7AmpDecay, DstS7AmpSustain, DstS7AmpRelease,
        DstS8AmpAttack, DstS8AmpDecay, DstS8AmpSustain, DstS8AmpRelease,

        NumDsts
    };

    static juce::StringArray getSourceNames()
    {
        return { "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
                 "ENV 1", "ENV 2", "ENV 3",
                 "Velocity", "Note", "Mod Wheel", "Random" };
    }

    static juce::StringArray getDestNames()
    {
        juce::StringArray list;
        list.add("None");

        for (int i = 1; i <= 8; ++i)
        {
            const juce::String s = "S" + juce::String(i) + "/";
            list.add(s + "Start");
            list.add(s + "End");
            list.add(s + "L-Start");
            list.add(s + "L-End");
            list.add(s + "X-Fade");
        }

        list.add("Master Pitch");

        list.add("Arp Octaves");
        list.add("Arp Rate");
        list.add("Arp Gate");
        list.add("Arp Offset");
        list.add("Arp Swing");
        list.add("Arp Repeat");
        list.add("Arp Accent");

        list.add("Filter Cutoff");
        list.add("Filter Reso");
        list.add("Filter Formant");
        list.add("Filter CombMix");

        list.add("FX1 Amount");
        list.add("FX2 Amount");
        list.add("FX3 Amount");
        list.add("FX4 Amount");
        list.add("FX5 Amount");

        list.add("Sat Drive");
        list.add("Sat Pre-HPF");
        list.add("Sat Trim");
        list.add("Chorus Rate");
        list.add("Chorus Depth");
        list.add("Chorus Width");
        list.add("Delay Feedback");
        list.add("Delay Duck");
        list.add("Delay Damp");
        list.add("Freeze Size");
        list.add("Freeze Feedback");
        list.add("Freeze Damp");
        list.add("Reverb Decay");
        list.add("Reverb Shimmer");
        list.add("Reverb Damp");
        list.add("Reverb Mod");

        // Dst enum の並びと 1:1 で対応させること (末尾に追記)
        for (int i = 1; i <= 8; ++i)
            list.add("S" + juce::String(i) + "/Pan");

        for (int i = 1; i <= 4; ++i)
            list.add("LFO " + juce::String(i) + " Rate");

        list.add("Filter Env Attack");
        list.add("Filter Env Decay");
        list.add("Filter Env Sustain");
        list.add("Filter Env Release");

        for (int i = 1; i <= 8; ++i)
        {
            const juce::String s = "S" + juce::String(i) + " Amp ";
            list.add(s + "Attack");
            list.add(s + "Decay");
            list.add(s + "Sustain");
            list.add(s + "Release");
        }

        return list;
    }

    static juce::StringArray getWaveNames()
    {
        return { "Sine", "Triangle", "Saw", "Square", "S&H", "Chaos" };
    }

    static juce::StringArray getSyncRateNames()
    {
        return { "1/1", "1/2", "1/2T", "1/4", "1/4.", "1/4T",
                 "1/8", "1/8.", "1/8T", "1/16", "1/16.", "1/16T", "1/32" };
    }

    struct Params
    {
        struct Lfo { float rateHz = 1.0f; bool sync = false; int rateSync = 6; int wave = 0; };
        struct Env { float attack = 0.05f; float decay = 0.5f; float sustain = 1.0f; float release = 0.3f; bool loop = false; };
        struct Slot { int src = 0; int dst = 0; float amt = 0.0f; bool uni = false; };

        std::array<Lfo, kNumLfos> lfo;
        std::array<Env, kNumEnvs> env;
        std::array<Slot, kNumSlots> slot;
        double bpm = 120.0;
    };

    void prepare(double sr)
    {
        sampleRate = sr > 1000.0 ? sr : 44100.0;
        reset();
    }

    void reset()
    {
        for (auto& l : lfoState) l = {};
        for (auto& e : envState) e = {};
        velocity = 0.8f;
        noteNorm = 0.5f;
        modWheel = 0.0f;
        randomSH = 0.5f;
        heldNotes = 0;
        gate = false;
        destAccum.fill(0.0f);
        rangeMin.fill(0.0f);
        rangeMax.fill(0.0f);
    }

    void handleMidi(const juce::MidiBuffer& midi)
    {
        for (const auto& m : midi)
        {
            const auto msg = m.getMessage();
            if (msg.isNoteOn())
            {
                velocity = msg.getFloatVelocity();
                noteNorm = (float)msg.getNoteNumber() / 127.0f;
                randomSH = rng.nextFloat();
                ++heldNotes;
                gate = true;
                for (auto& e : envState) e.stage = EnvState::Attack;
            }
            else if (msg.isNoteOff())
            {
                if (--heldNotes <= 0) { heldNotes = 0; gate = false; }
            }
            else if (msg.isControllerOfType(1))
            {
                modWheel = (float)msg.getControllerValue() / 127.0f;
            }
        }
    }

    void processBlock(int numSamples, const Params& p)
    {
        static const double beatsTable[13] = {
            4.0, 2.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0,
            0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125 };

        std::array<float, NumDsts> localAccum {};
        std::array<float, NumDsts> localMin {};
        std::array<float, NumDsts> localMax {};

        float src[NumSrcs] = {};

        // LFO
        for (int i = 0; i < kNumLfos; ++i)
        {
            const auto& lp = p.lfo[(size_t)i];
            auto& st = lfoState[(size_t)i];

            double freq = (double)lp.rateHz;
            if (lp.sync)
            {
                const double bpm = p.bpm > 1.0 ? p.bpm : 120.0;
                freq = bpm / (60.0 * beatsTable[juce::jlimit(0, 12, lp.rateSync)]);
            }

            // LFO Rate へのMOD適用 (LFO→LFOのクロスモジュレーション用)。
            // destAccum はこの processBlock 呼び出しの直前 = 前ブロックまでの
            // 集計値を保持しているので、ここで参照すれば「自分自身のRateを
            // 自分でMODする」ような自己参照でも1ブロック遅延で安全に成立する。
            const float rateMod = destAccum[(size_t)(DstLfo1Rate + i)];
            if (std::abs(rateMod) > 0.0001f)
                freq = juce::jlimit(0.05, 30.0, freq + (double)rateMod * 15.0);

            src[SrcLfo1 + i] = lfoValue(st, lp.wave);

            const double inc = freq * (double)numSamples / sampleRate;
            st.phase += inc;
            st.phase2 += inc * 1.41421356;
            while (st.phase >= 1.0)
            {
                st.phase -= 1.0;
                st.shValue = rng.nextFloat() * 2.0f - 1.0f;
            }
            while (st.phase2 >= 1.0) st.phase2 -= 1.0;
        }

        // ENV
        for (int i = 0; i < kNumEnvs; ++i)
        {
            const auto& ep = p.env[(size_t)i];
            auto& st = envState[(size_t)i];
            const float sus = juce::jlimit(0.0f, 1.0f, ep.sustain);

            src[SrcEnv1 + i] = st.value;

            const float blockSec = (float)((double)numSamples / sampleRate);

            if (!ep.loop && !gate && st.stage != EnvState::Idle && st.stage != EnvState::Release)
            {
                st.stage = EnvState::Release;
                st.releaseStart = juce::jmax(0.0001f, st.value);
            }

            switch (st.stage)
            {
            case EnvState::Attack:
                st.value += blockSec / juce::jmax(0.001f, ep.attack);
                if (st.value >= 1.0f) { st.value = 1.0f; st.stage = EnvState::Decay; }
                break;

            case EnvState::Decay:
                st.value -= blockSec / juce::jmax(0.001f, ep.decay) * (1.0f - sus);
                if (st.value <= sus) { st.value = sus; st.stage = EnvState::Sustain; }
                break;

            case EnvState::Sustain:
                st.value = sus;
                if (ep.loop) st.stage = EnvState::Attack;
                break;

            case EnvState::Release:
                st.value -= blockSec / juce::jmax(0.001f, ep.release) * st.releaseStart;
                if (st.value <= 0.0f) { st.value = 0.0f; st.stage = EnvState::Idle; }
                break;

            default:
                break;
            }
        }

        src[SrcVelocity] = velocity;
        src[SrcNote]     = noteNorm;
        src[SrcModWheel] = modWheel;
        src[SrcRandom]   = randomSH;

        // Slot Matrix Accumulation
        for (const auto& s : p.slot)
        {
            if (s.src <= SrcNone || s.src >= NumSrcs || s.dst <= DstNone || s.dst >= NumDsts) continue;
            if (std::abs(s.amt) < 0.0001f) continue;

            const bool srcBip = isBipolarSource(s.src);

            float v = src[s.src];
            if (s.uni) { if (srcBip) v = (v + 1.0f) * 0.5f; }
            else       { if (!srcBip) v = v * 2.0f - 1.0f; }
            localAccum[(size_t)s.dst] += v * s.amt;

            const float lo = s.uni ? 0.0f : -1.0f;
            const float hi = 1.0f;
            const float c1 = lo * s.amt, c2 = hi * s.amt;
            localMin[(size_t)s.dst] += juce::jmin(c1, c2);
            localMax[(size_t)s.dst] += juce::jmax(c1, c2);
        }

        destAccum = localAccum;
        rangeMin = localMin;
        rangeMax = localMax;

        for (size_t d = 0; d < NumDsts; ++d)
        {
            guiDestAccum[d].store(localAccum[d], std::memory_order_relaxed);
            guiRangeMin[d].store(localMin[d], std::memory_order_relaxed);
            guiRangeMax[d].store(localMax[d], std::memory_order_relaxed);
        }
    }

    float get(int dst) const noexcept
    {
        const int idx = juce::jlimit(0, (int)NumDsts - 1, dst);
        return guiDestAccum[(size_t)idx].load(std::memory_order_relaxed);
    }

    float getRangeMin(int dst) const noexcept
    {
        const int idx = juce::jlimit(0, (int)NumDsts - 1, dst);
        return guiRangeMin[(size_t)idx].load(std::memory_order_relaxed);
    }

    float getRangeMax(int dst) const noexcept
    {
        const int idx = juce::jlimit(0, (int)NumDsts - 1, dst);
        return guiRangeMax[(size_t)idx].load(std::memory_order_relaxed);
    }

    static bool isBipolarSource(int s) noexcept { return s >= SrcLfo1 && s <= SrcLfo4; }

    // ==================================================================
    // MOD 量 (-1..+1) を「実際のパラメータ値」へ適用した結果を返す。
    //
    // 【なぜ必要か】
    // get() が返す変調量は -1..+1 の抽象値で、行き先ごとに固有の倍率を
    // 掛けてから加算される (Cutoff は ±4オクターブ、Master Pitch は ±24半音、
    // Sat Pre-HPF は ±1000Hz …)。
    // GUI 側で「変調レンジのアーク」を描くときにこの倍率を無視すると、
    // 帯の幅が実際の変調量とまったく違うものになってしまう。
    //
    // 【重要】 PluginProcessor::processBlock の適用式と 1 対 1 で対応させること。
    // 片方だけ倍率を変えるとアーク表示と実音がずれる。
    // クランプ (jlimit) はここでは行わない。呼び出し側でノブのレンジに
    // 収めればよく、GUI と DSP でクランプ方法を二重管理したくないため。
    // ==================================================================
    static double applyModToValue(int dst, double baseValue, double modAmount) noexcept
    {
        // Filter Cutoff だけは加算ではなく「オクターブ単位の指数変化」
        if (dst == DstFltCutoff)   return baseValue * std::pow(2.0, modAmount * 4.0);

        if (dst == DstMasterPitch) return baseValue + modAmount * 24.0;
        if (dst == DstFltReso)     return baseValue + modAmount * 5.0;

        if (dst == DstArpRate)     return baseValue + modAmount * 15.0;
        if (dst == DstArpOctaves)  return baseValue + modAmount * 3.0;
        if (dst == DstArpOffset)   return baseValue + modAmount * 12.0;
        if (dst == DstArpRepeat)   return baseValue + modAmount * 3.0;

        if (dst == DstSatDrive)    return baseValue + modAmount * 6.0;
        if (dst == DstSatPreHz)    return baseValue + modAmount * 1000.0;
        if (dst == DstSatTrim)     return baseValue + modAmount * 12.0;
        if (dst == DstChoRate)     return baseValue + modAmount * 2.0;
        if (dst == DstFrzSize)     return baseValue + modAmount * 500.0;

        if (dst >= DstLfo1Rate && dst <= DstLfo4Rate) return baseValue + modAmount * 15.0;

        // Filter Envelope (秒)
        if (dst == DstFltEnvAttack || dst == DstFltEnvDecay) return baseValue + modAmount * 2.5;
        if (dst == DstFltEnvRelease)                         return baseValue + modAmount * 5.0;
        if (dst == DstFltEnvSustain)                         return baseValue + modAmount;

        // 各スロットの Amp ADSR (A,D,S,R が4個ずつ並ぶ。秒)
        if (dst >= DstS1AmpAttack && dst <= DstS8AmpRelease)
        {
            switch ((dst - DstS1AmpAttack) % 4)
            {
            case 0: case 1: return baseValue + modAmount * 2.5;  // Attack / Decay
            case 2:         return baseValue + modAmount;        // Sustain (0..1)
            default:        return baseValue + modAmount * 5.0;  // Release
            }
        }

        // 残り (Start/End/L-Start/L-End/X-Fade, Pan, FX Amount, Depth/Width,
        //       Feedback/Duck/Damp, Formant/CombMix, Arp Gate/Swing/Accent …)
        // はすべて 0..1 もしくは -1..1 レンジで 1:1 加算。
        return baseValue + modAmount;
    }

private:
    struct LfoState
    {
        double phase = 0.0;
        double phase2 = 0.0;
        float shValue = 0.0f;
    };
    struct EnvState
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release };
        int stage = Idle;
        float value = 0.0f;
        float releaseStart = 1.0f;
    };

    float lfoValue(const LfoState& st, int wave) const noexcept
    {
        const float ph = (float)st.phase;
        switch (wave)
        {
        case 0: return std::sin(ph * juce::MathConstants<float>::twoPi);
        case 1: return 1.0f - 4.0f * std::abs(ph - 0.5f);
        case 2: return 2.0f * ph - 1.0f;
        case 3: return ph < 0.5f ? 1.0f : -1.0f;
        case 4: return st.shValue;
        case 5: return (std::sin(ph * juce::MathConstants<float>::twoPi)
                      + std::sin((float)st.phase2 * juce::MathConstants<float>::twoPi)) * 0.5f;
        default: return 0.0f;
        }
    }

    double sampleRate = 44100.0;
    std::array<LfoState, kNumLfos> lfoState;
    std::array<EnvState, kNumEnvs> envState;

    float velocity = 0.8f;
    float noteNorm = 0.5f;
    float modWheel = 0.0f;
    float randomSH = 0.5f;
    int   heldNotes = 0;
    bool  gate = false;

    std::array<float, NumDsts> destAccum {};
    std::array<float, NumDsts> rangeMin {};
    std::array<float, NumDsts> rangeMax {};

    juce::Random rng;

    mutable std::array<std::atomic<float>, NumDsts> guiDestAccum {};
    mutable std::array<std::atomic<float>, NumDsts> guiRangeMin {};
    mutable std::array<std::atomic<float>, NumDsts> guiRangeMax {};
};
