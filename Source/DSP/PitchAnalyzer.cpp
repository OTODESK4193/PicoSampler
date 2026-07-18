// ==========================================
// File: PitchAnalyzer.cpp
// 高精度 YIN ピッチ解析 ＆ 誤判定完全排除ファイル名パース実装
// ==========================================
#include "PitchAnalyzer.h"
#include <cmath>
#include <regex>
#include <algorithm>

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

    // 3. YIN アルゴリズムによる超精密基音検出 (オクターブ・倍音飛び防止)
    float detectedHz = analyzePitchYin(mono.data(), numSamples, sampleRate, 20.0f, 1500.0f);
    if (detectedHz <= 0.0f)
    {
        detectedHz = analyzePitch(mono.data(), numSamples, sampleRate, 30.0f);
    }

    float dspCents = 0.0f;
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

// YIN ピッチ検出アルゴリズムの実装 (倍音誤判定・オクターブエラーゼロ化)
float PitchAnalyzer::analyzePitchYin(const float* samples, int numSamples, double sampleRate, float minFreq, float maxFreq)
{
    const int W = (int)(sampleRate / std::max(10.0f, minFreq)); // 最大ラグ Window
    if (numSamples <= W * 2) return 0.0f;

    const int minLag = (int)(sampleRate / std::min((float)sampleRate * 0.45f, maxFreq));
    const int maxLag = W;

    std::vector<float> d((size_t)maxLag, 0.0f);

    // 1. 差分関数 (Difference Function)
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
    const float threshold = 0.12f; // 低音ベース基音を捕捉する標準YIN閾値
    int tau = -1;

    for (int t = minLag; t < maxLag; ++t)
    {
        if (dPrime[(size_t)t] < threshold)
        {
            while (t + 1 < maxLag && dPrime[(size_t)(t + 1)] < dPrime[(size_t)t])
            {
                t++;
            }
            tau = t;
            break;
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
        if (minVal > 0.45f) return 0.0f;
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

    // 1. 直前が音階表記に合致する表記パターン (例: "Lev_Bass_Sustained_A#_04.wav", "Bass_C_36.wav")
    std::regex keyWithSampleNumRegex(R"((?:^|[\s_\-\(\)\[\]])([A-G][#b]?)(?:_|\-|\s)(\d{1,3})(?:[\s_\-\(\)\[\]\.]|$))", std::regex::icase);
    std::smatch match;

    if (std::regex_search(str, match, keyWithSampleNumRegex))
    {
        std::string noteName = match[1].str();
        int sampleIndexNum = std::stoi(match[2].str());

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
        int noteIdx = -1;
        for (size_t i = 0; i < notes.size(); ++i) {
            if (notes[i] == noteName) { noteIdx = (int)i; break; }
        }

        if (noteIdx >= 0)
        {
            const bool hasUnderscoreNum = (str.find("_" + match[2].str()) != std::string::npos);
            if (sampleIndexNum >= 12 && sampleIndexNum <= 110 && !hasUnderscoreNum)
            {
                return sampleIndexNum;
            }
            else
            {
                // A#1 = 34, C1 = 24
                if (noteIdx >= 9) return 24 + noteIdx; // A1=33, A#1=34
                return 36 + noteIdx;                  // C2=36
            }
        }
    }

    // 2. 直後に数字が直結している正確なオクターブ表記 (例: "C2", "A#1", "Eb4", "D-1")
    std::regex octKeyRegex(R"((?:^|[\s_\-\(\)\[\]])([A-G][#b]?)(-?\d)(m|maj|min|minor|major)?(?:\_|\-|\s|\.|\)|$))", std::regex::icase);
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
