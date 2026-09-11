cmake_minimum_required(VERSION 3.20)

project(huxer_graph VERSION 0.1.0 LANGUAGES NONE)

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

huxerui_add_app(huxer_graph
        SOURCES
            ${APP_SOURCE_FILES}
        RESOURCES
            resources
        RESOURCE_NAMESPACE
            app
        ${HUXERUI_APP_RESOURCE_OUTPUT_ARGUMENTS}
        BUNDLE_NAME
            "Huxer Graph"
        BUNDLE_IDENTIFIER
            "com.huxerui.demos.huxergraph"
)

huxerui_configure_project_app(huxer_graph)

if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    enable_language(C CXX)
    include(cmake/Dependencies.cmake)
    target_link_libraries(huxer_graph PRIVATE graph_core)
    option(GRAPH_BUILD_TESTS "Build independent mathematical checks" OFF)
    if (GRAPH_BUILD_TESTS)
        enable_testing()
        add_executable(graph_tests tests/math_tests.cpp)
        target_link_libraries(graph_tests PRIVATE graph_core)
        add_test(NAME graph_math_and_documents COMMAND graph_tests)
    endif ()
    if (MSVC)
        target_compile_options(huxer_graph PRIVATE /utf-8)
    endif ()
endif ()
