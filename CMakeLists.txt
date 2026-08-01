cmake_minimum_required(VERSION 3.22)

# ============================================================================
#  PicoSampler  -  32-Voice Multi-Mode Sampler with SignalStretch (OTODESK)
#  JUCE 8.0 系 / VST3 + Standalone
#  Single / Layer / Random Mode / ArcDial GUI / 5-FX Chain / ModMatrix
#  最適化: AVX2 SIMD, /fp:fast
# ============================================================================
project(PicoSampler VERSION 1.1.0 LANGUAGES C CXX)

# ----------------------------------------------------------------------------
#  C++20
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
#  JUCE の場所
# ----------------------------------------------------------------------------
set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to the JUCE checkout")
add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)

# ----------------------------------------------------------------------------
#  プラグイン定義
# ----------------------------------------------------------------------------
juce_add_plugin(PicoSampler
    COMPANY_NAME             "OTODESK"
    PLUGIN_MANUFACTURER_CODE "Otdk"
    PLUGIN_CODE              "PcSm"
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "PicoSampler"
    PLUGIN_DESCRIPTION       "32-voice Multi-Mode Sampler with SignalStretch"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_AUTO_MANIFEST       FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    VERSION                  "1.1.0"
)

# ----------------------------------------------------------------------------
#  ソース (Sourceフォルダ経由)
# ----------------------------------------------------------------------------
target_sources(PicoSampler PRIVATE
    # --- プラグインコア ---
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp

    # --- DSP モジュール ---
    Source/DSP/SamplerEngine.h
    Source/DSP/SamplerEngine.cpp
    Source/DSP/SamplerVoice.h
    Source/DSP/SamplerVoice.cpp
    Source/DSP/SampleSlot.h
    Source/DSP/SampleSlot.cpp
    Source/DSP/PitchAnalyzer.h
    Source/DSP/PitchAnalyzer.cpp
    Source/DSP/ScaleQuantizer.h
    Source/DSP/SampleVisualizerData.h
    Source/DSP/Arpeggiator.h
    Source/DSP/Arpeggiator.cpp
    Source/DSP/PicoFilter.h
    Source/DSP/PicoFilter.cpp
    Source/DSP/FxChain.h
    Source/DSP/ModMatrix.h
    Source/DSP/TptSvfFilter.h

    # --- GUI モジュール ---
    Source/GUI/ColorPalette.h
    Source/GUI/ArcDial.h
    Source/GUI/ArcDial.cpp
    Source/GUI/WaveformDisplay.h
    Source/GUI/WaveformDisplay.cpp
    Source/GUI/KeyRangeMapComponent.h
    Source/GUI/KeyRangeMapComponent.cpp
    Source/GUI/FilterCurveComponent.h
    Source/GUI/FilterCurveComponent.cpp
    Source/GUI/MainPanel.h
    Source/GUI/MainPanel.cpp
    Source/GUI/ValueKnob.h
    Source/GUI/GlowToggle.h
    Source/GUI/ArpPanel.h
    Source/GUI/ArpPanel.cpp
    Source/GUI/ConfigPanel.h
    Source/GUI/ConfigPanel.cpp
    Source/GUI/FxPanel.h
    Source/GUI/FxPanel.cpp
    Source/GUI/ModPanel.h
    Source/GUI/ModPanel.cpp
    Source/GUI/PresetBrowser.h

    Source/Presets/PresetData.h
)

# ----------------------------------------------------------------------------
#  インクルードパス
# ----------------------------------------------------------------------------
target_include_directories(PicoSampler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/GUI
)

# サードパーティは SYSTEM 扱い (/external:I) にして警告対象から外す
target_include_directories(PicoSampler SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/third_party
)

# ----------------------------------------------------------------------------
#  JUCE モジュール
# ----------------------------------------------------------------------------
target_link_libraries(PicoSampler
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ----------------------------------------------------------------------------
#  コンパイル定義
# ----------------------------------------------------------------------------
target_compile_definitions(PicoSampler PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    PICOSAMPLER_VERSION="1.1.0"
)

# ----------------------------------------------------------------------------
#  JUCE ヘッダー生成
# ----------------------------------------------------------------------------
juce_generate_juce_header(PicoSampler)

# ----------------------------------------------------------------------------
#  MSVC コンパイルオプション (AVX2 SIMD 有効)
# ----------------------------------------------------------------------------
if(MSVC)
    target_compile_options(PicoSampler PRIVATE
        /arch:AVX2
        /fp:fast
        /utf-8
        /W3
        /wd4819
        /external:W0          # SYSTEMインクルード(third_party)の警告を抑制
        $<$<CONFIG:Release>:/O2>
    )
endif()
