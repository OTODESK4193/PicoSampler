// ==========================================
// File: ScaleQuantizer.h
// スケール量子化モジュール (Granularより移植)
// 17種類のスケールに対応し、ピッチ値を構成音にスナップ
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>

class ScaleQuantizer
{
public:
    enum ScaleType
    {
        Free = 0, Chromatic, Major, NaturalMinor, MajorPentatonic, MinorPentatonic,
        Dorian, Lydian, Mixolydian, Phrygian, HarmonicMinor, MelodicMinor,
        WholeTone, OctavesFifths, Quartal, SusCloud, Diminished,
        NumScales
    };

    static juce::StringArray getScaleNames()
    {
        return { "Free (Off)", "Chromatic", "Major", "Natural Minor",
                 "Major Pentatonic", "Minor Pentatonic", "Dorian", "Lydian",
                 "Mixolydian", "Phrygian", "Harmonic Minor", "Melodic Minor",
                 "Whole Tone", "Octaves & Fifths", "Quartal", "Sus2/4 Cloud", "Diminished" };
    }

    static float quantize(float targetAbsSemis, int rootNote, int scaleType) noexcept
    {
        if (scaleType <= Free || scaleType >= NumScales) return targetAbsSemis;

        const auto& intervals = getIntervals(scaleType);
        if (intervals.empty()) return targetAbsSemis;

        const float relSemis = targetAbsSemis - (float)rootNote;
        const int octave = (int)std::floor(relSemis / 12.0f);
        const float noteInOct = relSemis - (float)(octave * 12);

        float bestDiff = 999.0f;
        float bestVal = noteInOct;

        for (int octOffset = -1; octOffset <= 1; ++octOffset)
        {
            for (int iv : intervals)
            {
                const float cand = (float)(iv + octOffset * 12);
                const float diff = std::abs(noteInOct - cand);
                if (diff < bestDiff)
                {
                    bestDiff = diff;
                    bestVal = cand;
                }
            }
        }

        return (float)rootNote + (float)(octave * 12) + bestVal;
    }

private:
    static std::vector<int> getIntervals(int scaleType)
    {
        switch (scaleType)
        {
        case Chromatic:       return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        case Major:           return { 0, 2, 4, 5, 7, 9, 11 };
        case NaturalMinor:    return { 0, 2, 3, 5, 7, 8, 10 };
        case MajorPentatonic: return { 0, 2, 4, 7, 9 };
        case MinorPentatonic: return { 0, 3, 5, 7, 10 };
        case Dorian:          return { 0, 2, 3, 5, 7, 9, 10 };
        case Lydian:          return { 0, 2, 4, 6, 7, 9, 11 };
        case Mixolydian:      return { 0, 2, 4, 5, 7, 9, 10 };
        case Phrygian:        return { 0, 1, 3, 5, 7, 8, 10 };
        case HarmonicMinor:   return { 0, 2, 3, 5, 7, 8, 11 };
        case MelodicMinor:    return { 0, 2, 3, 5, 7, 9, 11 };
        case WholeTone:       return { 0, 2, 4, 6, 8, 10 };
        case OctavesFifths:   return { 0, 7 };
        case Quartal:         return { 0, 5, 10 };
        case SusCloud:        return { 0, 2, 5, 7 };
        case Diminished:      return { 0, 2, 3, 5, 6, 8, 9, 11 };
        default:              return {};
        }
    }
};
