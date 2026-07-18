// ==========================================
// File: PitchAnalyzer.cpp
// インテリジェントピッチ・素材解析実装
// ==========================================
#include "PitchAnalyzer.h"
#include <cmath>
#include <regex>

PitchAnalyzer::AnalysisResult PitchAnalyzer::analyzeOmni(const juce::AudioBuffer<float>& buffer,
                                                          double sampleRate,
                                                          int materialMode,
                                                          const juce::String& fileName)
{
    AnalysisResult res;
    if (buffer.getNumSamples() < 128 || sampleRate <= 0.0) return res;

    // 1. ファイル名からの検出試行
    bool isMinor = false;
    int fileKey = parseKeyFromFileName(fileName, isMinor);
    if (fileKey >= 0)
    {
        res.rootNote = fileKey;
        res.confidence = 0.95f;
        res.detectedKeyStr = juce::MidiMessage::getMidiNoteName(fileKey, true, true, 4);
        res.isMinor = isMinor;
        return res;
    }

    // 2. モノラルミックスバッファの作成
    const int numSamples = buffer.getNumSamples();
    std::vector<float> mono((size_t)numSamples, 0.0f);
    const int numCh = buffer.getNumChannels();
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* r = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i) mono[(size_t)i] += r[i] / (float)numCh;
    }

    // 3. ルート判定と処理ルートの選択
    float detectedHz = 0.0f;
    const juce::String lowerName = fileName.toLowerCase();

    bool isKick = lowerName.contains("kick") || lowerName.contains("bd");
    bool isHat  = lowerName.contains("hat") || lowerName.contains("cymbal") || lowerName.contains("snare") || lowerName.contains("top");
    bool isLoop = lowerName.contains("loop");

    if (materialMode == Crisp || isKick)
    {
        detectedHz = analyzePitchKickFFT(mono.data(), numSamples, sampleRate);
    }
    else if (isHat)
    {
        detectedHz = analyzePitchHatCym(mono.data(), numSamples, sampleRate);
    }
    else if (materialMode == Smooth || isLoop)
    {
        const int subLen = std::min(numSamples, (int)(sampleRate * 0.3));
        detectedHz = analyzePitch(mono.data(), subLen, sampleRate, 40.0f);
    }
    else if (materialMode == Formant || lowerName.contains("bass") || lowerName.contains("piano"))
    {
        detectedHz = analyzePitchCepstrum(mono.data(), numSamples, sampleRate, 30.0f, 1500.0f);
    }

    if (detectedHz <= 0.0f)
    {
        detectedHz = analyzePitch(mono.data(), numSamples, sampleRate, 40.0f);
    }

    if (detectedHz > 0.0f)
    {
        res.rootNote = (int)std::round(hzToMidiNote(detectedHz, res.centsOffset));
        res.rootNote = juce::jlimit(0, 127, res.rootNote);
        res.confidence = 0.8f;
        res.detectedKeyStr = juce::MidiMessage::getMidiNoteName(res.rootNote, true, true, 4);
    }

    return res;
}

float PitchAnalyzer::analyzePitch(const float* samples, int numSamples, double sampleRate, float minFreq)
{
    if (numSamples < 256 || sampleRate <= 0.0) return 0.0f;

    const int maxLag = (int)(sampleRate / std::max(10.0f, minFreq));
    const int minLag = (int)(sampleRate / 1200.0f);
    if (maxLag >= numSamples) return 0.0f;

    // パケット化された自己相関 (NSDF)
    float maxCorr = -1.0f;
    int bestLag = -1;

    for (int lag = minLag; lag < maxLag; ++lag)
    {
        float sum = 0.0f, energy1 = 0.0f, energy2 = 0.0f;
        const int len = numSamples - lag;
        for (int i = 0; i < len; i += 2) // 2ステップスキップ高速化
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
            if (nsdf > maxCorr && nsdf > 0.35f)
            {
                maxCorr = nsdf;
                bestLag = lag;
            }
        }
    }

    if (bestLag <= 0) return 0.0f;

    // 放物線補間による精密ラグ計算
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

float PitchAnalyzer::analyzePitchCepstrum(const float* samples, int numSamples, double sampleRate, float minFreq, float maxFreq)
{
    // 簡略化された擬似ケプストラム (自己相関フォールバック)
    return analyzePitch(samples, numSamples, sampleRate, minFreq);
}

float PitchAnalyzer::analyzePitchKickFFT(const float* samples, int numSamples, double sampleRate)
{
    // アタック(先頭40ms)スキップ後のピーク分析
    const int skip = (int)(sampleRate * 0.04);
    if (numSamples <= skip + 512) return analyzePitch(samples, numSamples, sampleRate, 30.0f);
    return analyzePitch(samples + skip, numSamples - skip, sampleRate, 30.0f);
}

float PitchAnalyzer::analyzePitchHatCym(const float* samples, int numSamples, double sampleRate)
{
    return analyzePitch(samples, numSamples, sampleRate, 1000.0f);
}

int PitchAnalyzer::parseKeyFromFileName(const juce::String& fileName, bool& outIsMinor)
{
    outIsMinor = false;
    if (fileName.isEmpty()) return -1;

    std::string str = fileName.toStdString();
    std::regex keyRegex(R"((?:^|[\s_\-\(\)\[\]])([A-G][#b]?)(m|maj|min|minor|major)?(?:\_|\-|\s|\.|\d|\)))", std::regex::icase);
    std::smatch match;

    if (std::regex_search(str, match, keyRegex))
    {
        std::string noteName = match[1].str();
        std::string scaleType = match[2].str();

        // ノート名の標準化
        if (noteName.length() >= 1) noteName[0] = (char)std::toupper(noteName[0]);
        if (noteName.length() >= 2 && noteName[1] == 'b')
        {
            // Flat -> Sharp 変換
            switch (noteName[0]) {
                case 'D': noteName = "C#"; break;
                case 'E': noteName = "D#"; break;
                case 'G': noteName = "F#"; break;
                case 'A': noteName = "G#"; break;
                case 'B': noteName = "A#"; break;
            }
        }

        static const std::vector<std::string> notes = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int noteIndex = -1;
        for (size_t i = 0; i < notes.size(); ++i) {
            if (notes[i] == noteName) { noteIndex = (int)i; break; }
        }

        if (noteIndex >= 0)
        {
            std::transform(scaleType.begin(), scaleType.end(), scaleType.begin(), ::tolower);
            if (scaleType == "m" || scaleType == "min" || scaleType == "minor") outIsMinor = true;

            // デフォルト C4 (60) 周辺のオクターブにマッピング
            return 60 + noteIndex;
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
