cmake_minimum_required(VERSION 3.24)
project(EchoTrails VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(ECHO_BUILD_PLUGIN "Build the native After Effects plug-in" ON)
option(ECHO_BUILD_TESTS "Build DSP and host-callback tests" ON)

add_library(EchoDSP STATIC src/EchoDSP.cpp src/EchoDSP.h)
target_include_directories(EchoDSP PUBLIC src)
target_compile_options(EchoDSP PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(EchoDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(ECHO_BUILD_PLUGIN)
    if(NOT APPLE)
        message(FATAL_ERROR "The plug-in packaging targets macOS. DSP tests can run with -DECHO_BUILD_PLUGIN=OFF.")
    endif()
    set(AE_SDK_ROOT "$ENV{HOME}/Developer/AfterEffectsSDK" CACHE PATH "Adobe After Effects SDK root (contains Examples)")
    set(AE_HEADERS "${AE_SDK_ROOT}/Examples/Headers")
    set(AE_UTIL "${AE_SDK_ROOT}/Examples/Util")
    if(NOT EXISTS "${AE_HEADERS}/AE_Effect.h")
        message(FATAL_ERROR "Set AE_SDK_ROOT to the Adobe SDK directory containing Examples/Headers/AE_Effect.h")
    endif()
    add_library(EchoTrails MODULE src/EchoTrails.cpp src/Parameters.h src/PluginMetadata.h)
    target_include_directories(EchoTrails SYSTEM PRIVATE "${AE_HEADERS}" "${AE_HEADERS}/SP" "${AE_UTIL}")
    target_link_libraries(EchoTrails PRIVATE EchoDSP)
    target_compile_options(EchoTrails PRIVATE -Wall -Wextra -Wno-unused-parameter)
    set_target_properties(EchoTrails PROPERTIES
        BUNDLE TRUE BUNDLE_EXTENSION plugin PREFIX ""
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist.in"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.signalforge.effects.echotrails"
        XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES NO
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/products/$<CONFIG>"
    )
    execute_process(COMMAND xcrun --find Rez OUTPUT_VARIABLE REZ OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    set(PIPL "${CMAKE_CURRENT_BINARY_DIR}/EchoTrails.rsrc")
    add_custom_command(OUTPUT "${PIPL}"
        COMMAND "${REZ}" -d __x86_64__=1 -useDF -script Roman
            -i "${AE_HEADERS}" -i "${AE_HEADERS}/SP" -i "${AE_SDK_ROOT}/Examples/Resources" -i "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/EchoTrailsPiPL.r" -o "${PIPL}"
        DEPENDS resources/EchoTrailsPiPL.r src/PluginMetadata.h VERBATIM)
    target_sources(EchoTrails PRIVATE "${PIPL}")
    set_source_files_properties("${PIPL}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_custom_command(TARGET EchoTrails POST_BUILD
        COMMAND /usr/bin/codesign --force --sign - --timestamp=none "$<TARGET_BUNDLE_DIR:EchoTrails>"
        COMMENT "Ad-hoc signing the local development plug-in" VERBATIM)
endif()

if(ECHO_BUILD_TESTS)
    enable_testing()
    add_executable(EchoDSPTests tests/DSPTests.cpp)
    target_link_libraries(EchoDSPTests PRIVATE EchoDSP)
    add_test(NAME EchoDSPTests COMMAND EchoDSPTests)
    if(ECHO_BUILD_PLUGIN)
        add_executable(EchoHostTests tests/HostTests.cpp src/EchoTrails.cpp)
        target_include_directories(EchoHostTests SYSTEM PRIVATE "${AE_HEADERS}" "${AE_HEADERS}/SP" "${AE_UTIL}")
        target_link_libraries(EchoHostTests PRIVATE EchoDSP)
        add_test(NAME EchoHostTests COMMAND EchoHostTests)
    endif()
endif()
