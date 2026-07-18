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

    // パステルアクセント
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

    static juce::Colour getSlotColor(int slotIdx) noexcept
    {
        return slotColors[(size_t)juce::jlimit(0, 7, slotIdx)];
    }

    static void setTheme(int themeIdx) noexcept
    {
        switch (themeIdx)
        {
        case 1: // Sakura
            bgDk = juce::Colour(0xff1A1618); panel = juce::Colour(0xff282225); break;
        case 2: // Ocean
            bgDk = juce::Colour(0xff12181A); panel = juce::Colour(0xff1C2528); break;
        case 3: // Forest
            bgDk = juce::Colour(0xff141A16); panel = juce::Colour(0xff202822); break;
        case 4: // Sunset
            bgDk = juce::Colour(0xff1A1614); panel = juce::Colour(0xff282220); break;
        case 5: // Mono
            bgDk = juce::Colour(0xff141414); panel = juce::Colour(0xff222222); break;
        default: // Midnight
            bgDk = juce::Colour(0xff16181A); panel = juce::Colour(0xff222528); break;
        }
    }
}
