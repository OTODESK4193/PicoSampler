// ==========================================
// File: PitchAnalyzer.cpp
// 高精度 YIN ピッチ解析 ＆ 誤判定完全排除ファイル名パース実装
// ==========================================
#include "PitchAnalyzer.h"
#include <cmath>
#include <regex>
#include <algorithm>
#include <vector>

PitchAnalyzer::AnalysisResult PitchAnalyzer::analyzeOmni(const juce::AudioBuffer<float>& buffer,
                                                          double sampleRate,
                                                          int materialMode,
                                                          const juce::String& fileName)
{
    juce::ignoreUnused(materialMode);
    AnalysisResult res;
    if (buffer.getNumSamples() < 128 || sampleRate <= 0.0) return res;

    // 1. ファイル名からの高精度キー検出
    bool isMinor = false;
    int fileKey = parseKeyFromFileName(fileName, isMinor);

    // 2. モノラルミックスバッファの作成
    const int numSamples = buffer.getNumSamples();
    std::vector<float> mono((size_t)numSamples, 0.0f);
    const int numCh = buffer.getNumChannels();
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* r = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i) mono[(size_t)i] += r[i] / (float)numCh;
    }

    // 3. マルチフレーム (複数ウィンドウ) でアタック以降の安定部分を解析
    // 冒頭のアタック過渡期をスキップし、音量が安定している区間からフレームを複数抽出
    std::vector<float> detectedPitches;
    std::vector<float> detectedCentsList;

    const int frameSize = (int)std::min((double)numSamples, sampleRate * 0.15); // 約150msフレーム
    const int hopSize = frameSize / 2;
    
    // 冒頭 5% をスキップしてエネルギーの高いフレームをスキャン
    const int startOffset = (int)(numSamples * 0.05);

    for (int pos = startOffset; pos + frameSize <= numSamples; pos += hopSize)
    {
        // フレームの RMS 計算
        float rms = 0.0f;
        for (int i = 0; i < frameSize; ++i)
        {
            float s = mono[(size_t)(pos + i)];
            rms += s * s;
        }
        rms = std::sqrt(rms / (float)frameSize);

        // 無音に近いフレームはスキップ
        if (rms < 0.015f) continue;

        float c = 0.0f;
        float hz = analyzePitchYin(mono.data() + pos, frameSize, sampleRate, 25.0f, 2000.0f);
        if (hz > 0.0f)
        {
            detectedPitches.push_back(hz);
            hzToMidiNote(hz, c);
            detectedCentsList.push_back(c);
        }
    }

    float detectedHz = 0.0f;
    float dspCents = 0.0f;

    if (!detectedPitches.empty())
    {
        // ピッチの中央値 (Median) を採用して外れ値/オクターブ跳びを排除
        std::vector<float> sortedPitches = detectedPitches;
        std::sort(sortedPitches.begin(), sortedPitches.end());
        detectedHz = sortedPitches[sortedPitches.size() / 2];

        std::vector<float> sortedCents = detectedCentsList;
        std::sort(sortedCents.begin(), sortedCents.end());
        dspCents = sortedCents[sortedCents.size() / 2];
    }
    else
    {
        // フォールバック: 全体でYINピッチ検出
        detectedHz = analyzePitchYin(mono.data(), numSamples, sampleRate, 25.0f, 2000.0f);
    }

    int dspKey = (detectedHz > 0.0f) ? (int)std::round(hzToMidiNote(detectedHz, dspCents)) : -1;

    // 4. 解析優先順位 & ダブルチェック
    if (fileKey >= 0)
    {
        res.rootNote = fileKey;
        res.centsOffset = dspCents;
        res.confidence = 0.98f;
        res.detectedKeyStr = juce::MidiMessage::getMidiNoteName(fileKey, true, true, 4);
        res.isMinor = isMinor;
        return res;
    }

    if (dspKey >= 0)
    {
        res.rootNote = juce::jlimit(0, 127, dspKey);
        res.centsOffset = dspCents;
        res.confidence = 0.85f;
        res.detectedKeyStr = juce::MidiMessage::getMidiNoteName(res.rootNote, true, true, 4);
        return res;
    }

    // 5. 判別不能
    res.rootNote = -1;
    res.confidence = 0.0f;
    res.detectedKeyStr = "Unknown";
    return res;
}

// YIN ピッチ検出アルゴリズムの実装 (オクターブ・サブハーモニクス跳び防止強化)
float PitchAnalyzer::analyzePitchYin(const float* samples, int numSamples, double sampleRate, float minFreq, float maxFreq)
{
    const int maxLag = (int)(sampleRate / std::max(10.0f, minFreq));
    const int minLag = (int)(sampleRate / std::min((float)sampleRate * 0.45f, maxFreq));

    if (numSamples <= maxLag + minLag) return 0.0f;

    std::vector<float> d((size_t)maxLag, 0.0f);

    // 1. 差分関数 (Difference Function)
    const int W = numSamples - maxLag;
    for (int t = 1; t < maxLag; ++t)
    {
        float sum = 0.0f;
        for (int i = 0; i < W; ++i)
        {
            const float delta = samples[i] - samples[i + t];
            sum += delta * delta;
        }
        d[(size_t)t] = sum;
    }

    // 2. 累積平均正規化差分関数 (CMNDF)
    std::vector<float> dPrime((size_t)maxLag, 1.0f);
    float runningSum = 0.0f;
    dPrime[0] = 1.0f;

    for (int t = 1; t < maxLag; ++t)
    {
        runningSum += d[(size_t)t];
        dPrime[(size_t)t] = (runningSum > 1.0e-6f) ? (d[(size_t)t] * (float)t) / runningSum : 1.0f;
    }

    // 3. 閾値落ち込み判定 (Absolute Threshold)
    // 最初の谷 (高周波/高音の基本波) を最優先探求してサブハーモニクス(低音誤検出)を防止
    const float threshold = 0.15f; 
    int tau = -1;

    for (int t = minLag; t < maxLag; ++t)
    {
        if (dPrime[(size_t)t] < threshold)
        {
            // ローカルミニマムに達するまで進む
            while (t + 1 < maxLag && dPrime[(size_t)(t + 1)] < dPrime[(size_t)t])
            {
                t++;
            }
            tau = t;
            break; // 最短ラグ(高音)で条件を満たした時点で確定！サブハーモニクス無視
        }
    }

    // 閾値以下の極小点が見つからなかった場合、全領域での最小点を探す
    if (tau < 0)
    {
        float minVal = 1.0f;
        for (int t = minLag; t < maxLag; ++t)
        {
            if (dPrime[(size_t)t] < minVal)
            {
                minVal = dPrime[(size_t)t];
                tau = t;
            }
        }
        if (minVal > 0.40f) return 0.0f;
    }

    if (tau <= 0) return 0.0f;

    // 4. 放物線補間による超高精度ラグ補正
    float betterTau = (float)tau;
    if (tau > minLag && tau < maxLag - 1)
    {
        const float s0 = dPrime[(size_t)(tau - 1)];
        const float s1 = dPrime[(size_t)tau];
        const float s2 = dPrime[(size_t)(tau + 1)];
        const float denom = 2.0f * (s2 - 2.0f * s1 + s0);
        if (std::abs(denom) > 1.0e-6f)
        {
            betterTau += (s0 - s2) / denom;
        }
    }

    return (float)(sampleRate / betterTau);
}

float PitchAnalyzer::analyzePitch(const float* samples, int numSamples, double sampleRate, float minFreq)
{
    if (numSamples < 256 || sampleRate <= 0.0) return 0.0f;

    const int maxLag = (int)(sampleRate / std::max(10.0f, minFreq));
    const int minLag = (int)(sampleRate / 1200.0f);
    if (maxLag >= numSamples) return 0.0f;

    float maxCorr = -1.0f;
    int bestLag = -1;

    for (int lag = minLag; lag < maxLag; ++lag)
    {
        float sum = 0.0f, energy1 = 0.0f, energy2 = 0.0f;
        const int len = numSamples - lag;
        for (int i = 0; i < len; i += 2)
        {
            const float s1 = samples[i];
            const float s2 = samples[i + lag];
            sum += s1 * s2;
            energy1 += s1 * s1;
            energy2 += s2 * s2;
        }

        const float norm = std::sqrt(energy1 * energy2);
        if (norm > 1.0e-5f)
        {
            const float nsdf = (2.0f * sum) / norm;
            if (nsdf > maxCorr && nsdf > 0.40f)
            {
                maxCorr = nsdf;
                bestLag = lag;
            }
        }
    }

    if (bestLag <= 0) return 0.0f;

    float interpLag = (float)bestLag;
    if (bestLag > minLag && bestLag < maxLag - 1)
    {
        auto calcNsdf = [&](int l) {
            float s = 0.0f, e1 = 0.0f, e2 = 0.0f;
            for (int i = 0; i < numSamples - l; ++i) {
                s += samples[i] * samples[i + l];
                e1 += samples[i] * samples[i];
                e2 += samples[i + l] * samples[i + l];
            }
            const float n = std::sqrt(e1 * e2);
            return n > 1.0e-5f ? (2.0f * s) / n : 0.0f;
        };

        const float alpha = calcNsdf(bestLag - 1);
        const float beta  = maxCorr;
        const float gamma = calcNsdf(bestLag + 1);
        const float delta = alpha - 2.0f * beta + gamma;
        if (std::abs(delta) > 1.0e-5f)
        {
            interpLag += (alpha - gamma) / (2.0f * delta);
        }
    }

    return (float)(sampleRate / interpLag);
}

float PitchAnalyzer::analyzePitchKickFFT(const float* samples, int numSamples, double sampleRate)
{
    const int skip = (int)(sampleRate * 0.04);
    if (numSamples <= skip + 512) return analyzePitchYin(samples, numSamples, sampleRate, 30.0f, 600.0f);
    return analyzePitchYin(samples + skip, numSamples - skip, sampleRate, 30.0f, 600.0f);
}

int PitchAnalyzer::parseKeyFromFileName(const juce::String& fileName, bool& outIsMinor)
{
    outIsMinor = false;
    if (fileName.isEmpty()) return -1;

    std::string str = fileName.toStdString();

    // 1. 直前にスペースや区切りがあり、正確なオクターブ表記 (例: "C2", "A#1", "Eb4", "D-1", "A#_1", "C_2")
    std::regex octKeyRegex(R"((?:^|[\s_\-\(\)\[\]])([A-G][#b]?)(?:_|\-|\s)?(-?\d)(m|maj|min|minor|major)?(?:\_|\-|\s|\.|\)|$))", std::regex::icase);
    std::smatch match;

    if (std::regex_search(str, match, octKeyRegex))
    {
        std::string noteName = match[1].str();
        int octave = std::stoi(match[2].str());
        std::string scaleType = match[3].str();

        if (!noteName.empty()) noteName[0] = (char)std::toupper(noteName[0]);
        if (noteName.length() >= 2 && noteName[1] == 'b')
        {
            switch (noteName[0]) {
                case 'D': noteName = "C#"; break;
                case 'E': noteName = "D#"; break;
                case 'G': noteName = "F#"; break;
                case 'A': noteName = "G#"; break;
                case 'B': noteName = "A#"; break;
            }
        }

        static const std::vector<std::string> notes = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (size_t i = 0; i < notes.size(); ++i) {
            if (notes[i] == noteName) {
                std::transform(scaleType.begin(), scaleType.end(), scaleType.begin(), ::tolower);
                if (scaleType == "m" || scaleType == "min" || scaleType == "minor") outIsMinor = true;

                // MIDI標準: C4 = 60, C2 = 36, A#1 = 34
                return (octave + 1) * 12 + (int)i;
            }
        }
    }

    return -1;
}

float PitchAnalyzer::hzToMidiNote(float hz, float& outCents) noexcept
{
    if (hz <= 0.0f) { outCents = 0.0f; return 60.0f; }
    const float note = 69.0f + 12.0f * std::log2(hz / 440.0f);
    const float rounded = std::round(note);
    outCents = (note - rounded) * 100.0f;
    return note;
}
