// ==========================================
// File: ColorPalette.h
// 10テーマ カラーシステム (Granularベース)
// 8スロット用パステルグラデーションカラー定義
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>

namespace PicoColors
{
    inline juce::Colour bgDk        = juce::Colour(0xff16181A);
    inline juce::Colour panel       = juce::Colour(0xff222528);
    inline juce::Colour track       = juce::Colour(0xff0C0C0C);
    inline juce::Colour knobTrack   = juce::Colour(0xff2A2D32);
    inline juce::Colour panelLine   = juce::Colour(0xff32363C);
    inline juce::Colour text        = juce::Colour(0xffEEEEEE);
    inline juce::Colour textDim     = juce::Colour(0xff888888);

    // パステルアクセント (Granular準拠)
    inline juce::Colour mint        = juce::Colour(0xff98FFB3);
    inline juce::Colour pink        = juce::Colour(0xffFF9EBB);
    inline juce::Colour lavender    = juce::Colour(0xffD6A4FF);
    inline juce::Colour peach       = juce::Colour(0xffFFB886);
    inline juce::Colour babyBlue    = juce::Colour(0xff94E2FF);
    inline juce::Colour sage        = juce::Colour(0xffB8F5D5);
    inline juce::Colour rose        = juce::Colour(0xffFFA3B5);
    inline juce::Colour lilac       = juce::Colour(0xffE2B5FF);

    // 8スロット定義カラー
    inline const std::array<juce::Colour, 8> slotColors = {
        juce::Colour(0xff98FFB3), // 0: Mint
        juce::Colour(0xffFF9EBB), // 1: Pink
        juce::Colour(0xff94E2FF), // 2: Baby Blue
        juce::Colour(0xffFFB886), // 3: Peach
        juce::Colour(0xffD6A4FF), // 4: Lavender
        juce::Colour(0xffFFEA79), // 5: Yellow
        juce::Colour(0xff75F3C2), // 6: Emerald
        juce::Colour(0xffFFA3B5)  // 7: Rose
    };

    inline int currentTheme = 0;
    inline juce::Colour waveGradStart = juce::Colour(0xff94E2FF);
    inline juce::Colour waveGradEnd   = juce::Colour(0xffFF9EBB);

    inline const std::array<juce::Colour, 8> monoSlotColors = {
        juce::Colour(0xffFFFFFF), // 0: Pure White
        juce::Colour(0xffDDDDDD), // 1: Silver
        juce::Colour(0xffBBBBBB), // 2: Light Gray
        juce::Colour(0xff999999), // 3: Mid Gray
        juce::Colour(0xffE5E5E5), // 4: Platinum
        juce::Colour(0xffCCCCCC), // 5: Ash
        juce::Colour(0xffB0B0B0), // 6: Steel
        juce::Colour(0xffFFFFFF)  // 7: White
    };

    inline juce::Colour getSlotColor(int slotIdx) noexcept
    {
        if (currentTheme == 5) // Monoテーマ時
            return monoSlotColors[(size_t)juce::jlimit(0, 7, slotIdx)];
        return slotColors[(size_t)juce::jlimit(0, 7, slotIdx)];
    }

    inline void setTheme(int themeIdx) noexcept
    {
        currentTheme = themeIdx;
        switch (themeIdx)
        {
        case 1: // Sakura
            bgDk = juce::Colour(0xff1C1417); panel = juce::Colour(0xff2A1E24);
            waveGradStart = juce::Colour(0xffFFB8D2); waveGradEnd = juce::Colour(0xffFF659A); break;
        case 2: // Ocean
            bgDk = juce::Colour(0xff101A20); panel = juce::Colour(0xff182730);
            waveGradStart = juce::Colour(0xff80E5FF); waveGradEnd = juce::Colour(0xff0088CC); break;
        case 3: // Forest
            bgDk = juce::Colour(0xff121C16); panel = juce::Colour(0xff1A2A20);
            waveGradStart = juce::Colour(0xff98FFC4); waveGradEnd = juce::Colour(0xff22AA66); break;
        case 4: // Sunset
            bgDk = juce::Colour(0xff201412); panel = juce::Colour(0xff301D1A);
            waveGradStart = juce::Colour(0xffFFC494); waveGradEnd = juce::Colour(0xffFF5533); break;
        case 5: // Mono
            bgDk = juce::Colour(0xff121212); panel = juce::Colour(0xff202020);
            waveGradStart = juce::Colour(0xffFFFFFF); waveGradEnd = juce::Colour(0xff777777); break;
        default: // Midnight
            bgDk = juce::Colour(0xff16181A); panel = juce::Colour(0xff222528);
            waveGradStart = juce::Colour(0xff94E2FF); waveGradEnd = juce::Colour(0xffFF9EBB); break;
        }
    }
}
