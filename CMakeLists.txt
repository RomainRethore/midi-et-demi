cmake_minimum_required(VERSION 3.22)

project(MidiEtDemi VERSION 0.1.0 LANGUAGES C CXX)

# --- Vérification du sous-module JUCE ------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE est introuvable dans external/JUCE.\n"
        "Lance :  git submodule update --init --recursive")
endif()

add_subdirectory(external/JUCE)

# --- Application graphique -----------------------------------------------------
juce_add_gui_app(MidiEtDemi
    PRODUCT_NAME "Midi et demi"
    COMPANY_NAME "RomainRethore"
    BUNDLE_ID    "com.romainrethore.midietdemi")

juce_generate_juce_header(MidiEtDemi)

target_sources(MidiEtDemi PRIVATE
    src/app/Main.cpp
    src/ui/MainComponent.cpp
    src/engine/AudioEngine.cpp
    src/engine/PluginHost.cpp
    src/engine/Track.cpp)

# Permet les inclusions du type  #include "engine/AudioEngine.h"
target_include_directories(MidiEtDemi PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_features(MidiEtDemi PRIVATE cxx_std_17)

target_compile_definitions(MidiEtDemi PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1          # héberger des plugins VST3
    JUCE_PLUGINHOST_AU=1            # héberger des Audio Units (macOS)
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MidiEtDemi,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MidiEtDemi,JUCE_VERSION>")

target_link_libraries(MidiEtDemi PRIVATE
    juce::juce_audio_utils                 # tire audio_basics/devices/formats/processors + GUI
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# --- Tests unitaires de la couche domaine (C++ pur, sans JUCE) -----------------
enable_testing()

add_executable(transport_tests tests/transport_tests.cpp)
target_include_directories(transport_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(transport_tests PRIVATE cxx_std_17)
add_test(NAME transport_tests COMMAND transport_tests)

add_executable(loopclip_tests tests/loopclip_tests.cpp)
target_include_directories(loopclip_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(loopclip_tests PRIVATE cxx_std_17)
add_test(NAME loopclip_tests COMMAND loopclip_tests)
