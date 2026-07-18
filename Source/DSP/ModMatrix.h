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
                if (ep.loop)
                {
                    st.value -= blockSec / juce::jmax(0.001f, ep.decay);
                    if (st.value <= 0.0f) { st.value = 0.0f; st.stage = EnvState::Attack; }
                }
                else
                {
                    st.value -= (1.0f - sus) * blockSec / juce::jmax(0.001f, ep.decay);
                    if (st.value <= sus) { st.value = sus; st.stage = EnvState::Sustain; }
                }
                break;

            case EnvState::Sustain:
                st.value = sus;
                break;

            case EnvState::Release:
                st.value -= st.releaseStart * blockSec / juce::jmax(0.001f, ep.release);
                if (st.value <= 0.0f) { st.value = 0.0f; st.stage = EnvState::Idle; }
                break;

            default:
                st.value = 0.0f;
                break;
            }
        }

        src[SrcVelocity] = velocity;
        src[SrcNote]     = noteNorm;
        src[SrcModWheel] = modWheel;
        src[SrcRandom]   = randomSH;

        destAccum.fill(0.0f);
        rangeMin.fill(0.0f);
        rangeMax.fill(0.0f);

        for (const auto& s : p.slot)
        {
            if (s.src <= 0 || s.src >= NumSrcs || s.dst <= 0 || s.dst >= NumDsts) continue;
            if (std::abs(s.amt) < 0.0001f) continue;

            const bool srcBip = isBipolarSource(s.src);

            float v = src[s.src];
            if (s.uni) { if (srcBip) v = (v + 1.0f) * 0.5f; }
            else       { if (!srcBip) v = v * 2.0f - 1.0f; }
            destAccum[(size_t)s.dst] += v * s.amt;

            const float lo = s.uni ? 0.0f : -1.0f;
            const float hi = 1.0f;
            const float c1 = lo * s.amt, c2 = hi * s.amt;
            rangeMin[(size_t)s.dst] += juce::jmin(c1, c2);
            rangeMax[(size_t)s.dst] += juce::jmax(c1, c2);
        }
    }

    float get(int dst) const noexcept
    {
        return destAccum[(size_t)juce::jlimit(0, (int)NumDsts - 1, dst)];
    }

    float getRangeMin(int dst) const noexcept { return rangeMin[(size_t)juce::jlimit(0, (int)NumDsts - 1, dst)]; }
    float getRangeMax(int dst) const noexcept { return rangeMax[(size_t)juce::jlimit(0, (int)NumDsts - 1, dst)]; }

    static bool isBipolarSource(int s) noexcept { return s >= SrcLfo1 && s <= SrcLfo4; }

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
};
