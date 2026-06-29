cmake_minimum_required(VERSION 3.25)

project(TaskbarLyrics VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Taskbar Lyrics currently supports Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Taskbar Lyrics must be built for x64.")
endif()

add_library(taskbar_lyrics_proxy SHARED
    src/bridge/host_bridge.cpp
    src/bridge/cef_probe.cpp
    src/proxy/dllmain.cpp
    src/proxy/msimg32.def
    src/core/bootstrap.cpp
    src/core/config.cpp
    src/core/logging.cpp
    src/ui/lyric_window.cpp
    src/ui/taskbar_layout.cpp
)

target_compile_features(taskbar_lyrics_proxy PRIVATE cxx_std_20)
target_compile_definitions(taskbar_lyrics_proxy PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_include_directories(taskbar_lyrics_proxy PRIVATE
    src
    third_party/cef
)
target_link_libraries(taskbar_lyrics_proxy PRIVATE
    dwmapi
    gdi32
    gdiplus
    ole32
    uiautomationcore
    user32
)

set_target_properties(taskbar_lyrics_proxy PROPERTIES
    OUTPUT_NAME msimg32
    PREFIX ""
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

if(MSVC)
    target_compile_options(taskbar_lyrics_proxy PRIVATE
        /W4
        /permissive-
        /utf-8
        /guard:cf
    )
    target_link_options(taskbar_lyrics_proxy PRIVATE
        /DYNAMICBASE
        /NXCOMPAT
        /guard:cf
    )
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(proxy_smoke_test tests/proxy_smoke_test.cpp)
    target_compile_features(proxy_smoke_test PRIVATE cxx_std_20)
    target_compile_definitions(proxy_smoke_test PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

    add_test(
        NAME proxy_smoke_test
        COMMAND proxy_smoke_test $<TARGET_FILE:taskbar_lyrics_proxy>
    )
    set_tests_properties(proxy_smoke_test PROPERTIES
        ENVIRONMENT "TASKBARLYRICS_LOG_DIR=$<TARGET_FILE_DIR:proxy_smoke_test>/test-logs"
    )

    add_executable(host_bridge_test
        tests/host_bridge_test.cpp
        src/bridge/host_bridge.cpp
        src/core/logging.cpp
    )
    target_compile_features(host_bridge_test PRIVATE cxx_std_20)
    target_compile_definitions(host_bridge_test PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(host_bridge_test PRIVATE src)
    set_target_properties(host_bridge_test PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    add_test(NAME host_bridge_test COMMAND host_bridge_test)

    add_executable(window_preview WIN32
        tests/window_preview.cpp
        src/bridge/host_bridge.cpp
        src/core/config.cpp
        src/core/logging.cpp
        src/ui/lyric_window.cpp
        src/ui/taskbar_layout.cpp
    )
    target_compile_features(window_preview PRIVATE cxx_std_20)
    target_compile_definitions(window_preview PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(window_preview PRIVATE src)
    target_link_libraries(window_preview PRIVATE
        dwmapi
        gdi32
        gdiplus
        ole32
        uiautomationcore
        user32
    )
    set_target_properties(window_preview PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()
