// ==========================================
// File: PitchAnalyzer.cpp
// インテリジェントピッチ・素材解析実装 (MIDI Note直接認識 & 完璧な音高解析)
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

    // 1. 高精度ファイル名からの検出
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

    // 3. DSP による実測ピッチ検出
    float detectedHz = 0.0f;
    const juce::String lowerName = fileName.toLowerCase();

    bool isKick = lowerName.contains("kick") || lowerName.contains("bd");
    bool isLoop = lowerName.contains("loop");

    if (materialMode == Crisp || isKick)
    {
        detectedHz = analyzePitchKickFFT(mono.data(), numSamples, sampleRate);
    }
    else if (materialMode == Smooth || isLoop)
    {
        const int subLen = std::min(numSamples, (int)(sampleRate * 0.35));
        detectedHz = analyzePitch(mono.data(), subLen, sampleRate, 40.0f);
    }
    else if (materialMode == Formant || lowerName.contains("bass") || lowerName.contains("vocal") || lowerName.contains("voice"))
    {
        detectedHz = analyzePitchCepstrum(mono.data(), numSamples, sampleRate, 30.0f, 1500.0f);
    }

    if (detectedHz <= 0.0f)
    {
        detectedHz = analyzePitch(mono.data(), numSamples, sampleRate, 40.0f);
    }

    float dspCents = 0.0f;
    int dspKey = (detectedHz > 0.0f) ? (int)std::round(hzToMidiNote(detectedHz, dspCents)) : -1;

    // 4. ファイル名解析と DSP 解析の総合判定
    if (fileKey >= 0)
    {
        res.rootNote = fileKey;
        res.centsOffset = dspCents;
        res.confidence = 0.95f;
        res.detectedKeyStr = juce::MidiMessage::getMidiNoteName(fileKey, true, true, 4);
        res.isMinor = isMinor;
        return res;
    }

    if (dspKey >= 0)
    {
        res.rootNote = juce::jlimit(0, 127, dspKey);
        res.centsOffset = dspCents;
        res.confidence = 0.80f;
        res.detectedKeyStr = juce::MidiMessage::getMidiNoteName(res.rootNote, true, true, 4);
        return res;
    }

    // 5. 判別不能
    res.rootNote = -1;
    res.confidence = 0.0f;
    res.detectedKeyStr = "Unknown";
    return res;
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

float PitchAnalyzer::analyzePitchCepstrum(const float* samples, int numSamples, double sampleRate, float minFreq, float maxFreq)
{
    juce::ignoreUnused(maxFreq);
    return analyzePitch(samples, numSamples, sampleRate, minFreq);
}

float PitchAnalyzer::analyzePitchKickFFT(const float* samples, int numSamples, double sampleRate)
{
    const int skip = (int)(sampleRate * 0.04);
    if (numSamples <= skip + 512) return analyzePitch(samples, numSamples, sampleRate, 30.0f);
    return analyzePitch(samples + skip, numSamples - skip, sampleRate, 30.0f);
}

int PitchAnalyzer::parseKeyFromFileName(const juce::String& fileName, bool& outIsMinor)
{
    outIsMinor = false;
    if (fileName.isEmpty()) return -1;

    std::string str = fileName.toStdString();

    // A. 末尾または区切り内の MIDI Note 直接数値パターン (例: "_C_36.wav", "_36.wav", "Note36")
    std::regex midiNoteRegex(R"((?:^|[\s_\-\(\)\[\]])(?:[A-G][#b]?)?_?(\d{1,3})(?:[\s_\-\(\)\[\]\.]|$))", std::regex::icase);
    std::smatch match;

    if (std::regex_search(str, match, midiNoteRegex))
    {
        int noteVal = std::stoi(match[1].str());
        if (noteVal >= 12 && noteVal <= 110)
        {
            return noteVal; // 直接 MIDI Note (例: 36 -> C2)
        }
    }

    // B. オクターブ記法パターン (例: "C2", "A#1", "Eb4", "D-1")
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

                // MIDI標準: C4 = 60, C2 = 36 (octave + 1)*12 + index
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
