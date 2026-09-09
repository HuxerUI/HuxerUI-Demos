cmake_minimum_required(VERSION 3.20)

project(huxer_music VERSION 0.1.0 LANGUAGES NONE)

include("${CMAKE_CURRENT_SOURCE_DIR}/HuxerUIProject.cmake")
if (HUXERUI_PROJECT_PLAN_ONLY)
    return()
endif ()

file(GLOB_RECURSE APP_SOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)
if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    if (WIN32)
        list(APPEND APP_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/main.cpp")
    elseif (APPLE AND NOT IOS)
        list(APPEND APP_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/main.cpp")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND APP_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/main.cpp")
    endif ()
endif ()

huxerui_add_app(huxer_music
        SOURCES
            ${APP_SOURCE_FILES}
        RESOURCES
            resources
        RESOURCE_NAMESPACE
            app
        ${HUXERUI_APP_RESOURCE_OUTPUT_ARGUMENTS}
        BUNDLE_NAME
            "Huxer Music"
        BUNDLE_IDENTIFIER
            "com.huxerui.demos.huxermusic"
)

if (MSVC)
    target_compile_options(huxer_music PRIVATE /utf-8)
endif ()

# Keep dependency population paths within Windows toolchain path limits.
include(FetchContent)
string(SHA256 MUSIC_BUILD_ID "${CMAKE_CURRENT_BINARY_DIR}")
string(SUBSTRING "${MUSIC_BUILD_ID}" 0 8 MUSIC_BUILD_ID)
set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.huxerui/${MUSIC_BUILD_ID}")
huxerui_use_library(huxer_music
        TARGET HuxerUI::MediaPlayer
        URL "https://github.com/HuxerUI/Lib-MediaPlayer.git"
        COMMIT "4243f48f19b5314236c62cb9f5db39daf6865bf3"
)

huxerui_configure_project_app(huxer_music)
