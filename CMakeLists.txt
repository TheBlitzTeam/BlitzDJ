cmake_minimum_required(VERSION 3.16)

project(BlitzDJ
    VERSION 0.1.0
    DESCRIPTION "An intelligent Spotify DJ powered by C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BLITZDJ_BUILD_TESTS "Build BlitzDJ tests" OFF)

include(FetchContent)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(json)

set(BLITZDJ_SOURCES
    src/Main.cpp
    src/BlitzDJ.cpp
    src/BlitzAI.cpp
    src/BlitzSpotify.cpp
)

set(BLITZDJ_ICON
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
)

if(NOT EXISTS "${BLITZDJ_ICON}")
    message(FATAL_ERROR "BlitzDJ icon not found: ${BLITZDJ_ICON}")
endif()

add_executable(BlitzDJ ${BLITZDJ_SOURCES})

target_include_directories(BlitzDJ PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(BlitzDJ PRIVATE
    CURL::libcurl
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

target_compile_definitions(BlitzDJ PRIVATE
    BLITZDJ_VERSION="${PROJECT_VERSION}"
    BLITZDJ_ICON_PATH="assets/icon.png"
)

if(MSVC)
    target_compile_options(BlitzDJ PRIVATE /W4)
else()
    target_compile_options(BlitzDJ PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

add_custom_command(
    TARGET BlitzDJ POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:BlitzDJ>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${BLITZDJ_ICON}"
        "$<TARGET_FILE_DIR:BlitzDJ>/assets/icon.png"
    COMMENT "Copying BlitzDJ application icon"
)

install(TARGETS BlitzDJ
    RUNTIME DESTINATION bin
)

install(FILES "${BLITZDJ_ICON}"
    DESTINATION bin/assets
)
