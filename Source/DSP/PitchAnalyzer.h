// ==========================================
// File: PitchAnalyzer.h
// インテリジェントピッチ・素材解析モジュール (高精度ファイル名＆DSP複合解析)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>

class PitchAnalyzer
{
public:
    enum MaterialMode
    {
        Auto = 0, Crisp, Smooth, Formant
    };

    struct AnalysisResult
    {
        int rootNote = -1;          // 検出MIDIノート (0-127), 不明は -1
        float centsOffset = 0.0f;   // セント偏位 (-50 ~ +50)
        float confidence = 0.0f;    // 信頼度 (0-1)
        juce::String detectedKeyStr = "Unknown";
        bool isMinor = false;
        bool hasLoopHint = false;
    };

    // メイン解析エントリポイント
    static AnalysisResult analyzeOmni(const juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      int materialMode = Auto,
                                      const juce::String& fileName = "");

    // 音声バッファから周波数 (Hz) を自己相関で検出
    static float analyzePitch(const float* samples, int numSamples, double sampleRate, float minFreq = 40.0f);

    // ケプストラム法
    static float analyzePitchCepstrum(const float* samples, int numSamples, double sampleRate, float minFreq = 30.0f, float maxFreq = 1500.0f);

    // キック/ドラム用 FFTピーク検出
    static float analyzePitchKickFFT(const float* samples, int numSamples, double sampleRate);

    // 高精度ファイル名からのキー検出
    static int parseKeyFromFileName(const juce::String& fileName, bool& outIsMinor);

private:
    static float hzToMidiNote(float hz, float& outCents) noexcept;
};
