cmake_minimum_required(VERSION 3.21)

project(BlitzDJ
    VERSION 0.2.0
    DESCRIPTION "BlitzDJ - Intelligent Spotify Music Controller"
    HOMEPAGE_URL "https://github.com/TheBlitzTeam/BlitzDJ"
    LANGUAGES C CXX
)

include(GNUInstallDirs)
include(FetchContent)
include(CheckIPOSupported)
include(CMakeDependentOption)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------
# Platform detection
# ---------------------------------------------

set(BLITZDJ_PLATFORM "Unknown")
set(BLITZDJ_MOBILE OFF)

if(ANDROID)
    set(BLITZDJ_PLATFORM "Android")
    set(BLITZDJ_MOBILE ON)

elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(BLITZDJ_PLATFORM "iOS")
    set(BLITZDJ_MOBILE ON)

elseif(WIN32)
    set(BLITZDJ_PLATFORM "Windows")

elseif(APPLE)
    set(BLITZDJ_PLATFORM "macOS")

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(BLITZDJ_PLATFORM "Linux")

else()
    message(FATAL_ERROR
        "Unsupported platform: ${CMAKE_SYSTEM_NAME}"
    )
endif()

# ---------------------------------------------
# Build options
# ---------------------------------------------

option(BLITZDJ_ENABLE_SPOTIFY
    "Enable Spotify integration" ON
)

option(BLITZDJ_ENABLE_BLUETOOTH
    "Enable Bluetooth support" ON
)

option(BLITZDJ_ENABLE_LTO
    "Enable Release link-time optimization" ON
)

option(BLITZDJ_ENABLE_WARNINGS
    "Enable compiler warnings" ON
)

option(BLITZDJ_WARNINGS_AS_ERRORS
    "Treat warnings as errors" OFF
)

option(BLITZDJ_BUILD_TESTS
    "Build tests" OFF
)

option(BLITZDJ_BUILD_CLI
    "Build the terminal application"
    ${BLITZDJ_MOBILE}
)

option(BLITZDJ_USE_SYSTEM_JSON
    "Use system nlohmann_json" OFF
)

option(BLITZDJ_INSTALL_ASSETS
    "Install application assets" ON
)

# Mobile platforms use the C++ core as a library.
if(BLITZDJ_MOBILE)
    set(BLITZDJ_BUILD_CLI OFF CACHE BOOL
        "CLI is unavailable on mobile" FORCE
    )
endif()

# ---------------------------------------------
# Directories
# ---------------------------------------------

set(BLITZDJ_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

set(BLITZDJ_SOURCE_DIR
    "${BLITZDJ_ROOT}/src"
)

set(BLITZDJ_INCLUDE_DIR
    "${BLITZDJ_ROOT}/include"
)

set(BLITZDJ_ASSETS_DIR
    "${BLITZDJ_ROOT}/assets"
)

set(BLITZDJ_CONFIG_DIR
    "${BLITZDJ_ROOT}/config"
)

set(BLITZDJ_ICON
    "${BLITZDJ_ASSETS_DIR}/icon.png"
)

# ---------------------------------------------
# Dependencies
# ---------------------------------------------

find_package(Threads REQUIRED)

if(BLITZDJ_ENABLE_SPOTIFY)
    find_package(CURL REQUIRED)
    find_package(OpenSSL REQUIRED)
endif()

if(BLITZDJ_USE_SYSTEM_JSON)
    find_package(nlohmann_json 3.11 REQUIRED)
else()
    FetchContent_Declare(
        nlohmann_json
        URL
        "https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ---------------------------------------------
# Core source files
# ---------------------------------------------

set(BLITZDJ_CORE_SOURCES
    "${BLITZDJ_SOURCE_DIR}/BlitzDJ.cpp"
    "${BLITZDJ_SOURCE_DIR}/BlitzAI.cpp"
    "${BLITZDJ_SOURCE_DIR}/BlitzSpotify.cpp"
)

set(BLITZDJ_CORE_HEADERS
    "${BLITZDJ_INCLUDE_DIR}/BlitzDJ.hpp"
    "${BLITZDJ_INCLUDE_DIR}/BlitzAI.hpp"
    "${BLITZDJ_INCLUDE_DIR}/BlitzSpotify.hpp"
)

# The current core depends on Spotify.
if(NOT BLITZDJ_ENABLE_SPOTIFY)
    message(FATAL_ERROR
        "Spotify cannot be disabled until the core is refactored."
    )
endif()

# ---------------------------------------------
# Bluetooth
# ---------------------------------------------

set(BLITZDJ_HAS_BLUETOOTH OFF)

if(BLITZDJ_ENABLE_BLUETOOTH)

    if(EXISTS
        "${BLITZDJ_SOURCE_DIR}/Bluetooth.cpp"
    )
        list(APPEND BLITZDJ_CORE_SOURCES
            "${BLITZDJ_SOURCE_DIR}/Bluetooth.cpp"
        )

        list(APPEND BLITZDJ_CORE_HEADERS
            "${BLITZDJ_INCLUDE_DIR}/Bluetooth.hpp"
        )

        if(BLITZDJ_PLATFORM STREQUAL "Linux")
            find_package(PkgConfig REQUIRED)

            pkg_check_modules(
                GIO REQUIRED
                IMPORTED_TARGET
                gio-2.0
            )

            set(BLITZDJ_HAS_BLUETOOTH ON)
        else()
            message(STATUS
                "Native Bluetooth backend unavailable on ${BLITZDJ_PLATFORM}"
            )
        endif()
    else()
        message(WARNING
            "Bluetooth.cpp not found; Bluetooth disabled"
        )
    endif()
endif()

# ---------------------------------------------
# Source validation
# ---------------------------------------------

foreach(BLITZDJ_FILE IN LISTS
    BLITZDJ_CORE_SOURCES
    BLITZDJ_CORE_HEADERS
)
    if(NOT EXISTS "${BLITZDJ_FILE}")
        message(FATAL_ERROR
            "Required BlitzDJ file missing: ${BLITZDJ_FILE}"
        )
    endif()
endforeach()

# ---------------------------------------------
# Core library
# ---------------------------------------------

if(ANDROID)
    add_library(BlitzDJCore SHARED
        ${BLITZDJ_CORE_SOURCES}
        ${BLITZDJ_CORE_HEADERS}
    )
else()
    add_library(BlitzDJCore STATIC
        ${BLITZDJ_CORE_SOURCES}
        ${BLITZDJ_CORE_HEADERS}
    )
endif()

add_library(BlitzDJ::Core ALIAS BlitzDJCore)

set_target_properties(BlitzDJCore PROPERTIES
    OUTPUT_NAME "blitzdj"
    VERSION "${PROJECT_VERSION}"
)

target_compile_features(BlitzDJCore PUBLIC
    cxx_std_17
)

target_include_directories(BlitzDJCore PUBLIC
    $<BUILD_INTERFACE:${BLITZDJ_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(BlitzDJCore PUBLIC
    Threads::Threads
    CURL::libcurl
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

if(BLITZDJ_HAS_BLUETOOTH)
    target_link_libraries(BlitzDJCore PRIVATE
        PkgConfig::GIO
    )
endif()

target_compile_definitions(BlitzDJCore PUBLIC
    BLITZDJ_VERSION="${PROJECT_VERSION}"
    BLITZDJ_PLATFORM="${BLITZDJ_PLATFORM}"
    BLITZDJ_HAS_BLUETOOTH=$<BOOL:${BLITZDJ_HAS_BLUETOOTH}>
    BLITZDJ_MOBILE=$<BOOL:${BLITZDJ_MOBILE}>
)

# ---------------------------------------------
# Android configuration
# ---------------------------------------------

if(ANDROID)
    message(STATUS "Configuring Android native library")

    if(NOT ANDROID_ABI)
        message(FATAL_ERROR
            "ANDROID_ABI must be configured"
        )
    endif()

    set_target_properties(BlitzDJCore PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/android/${ANDROID_ABI}"
    )

    target_link_libraries(BlitzDJCore PRIVATE
        log
    )
endif()

# ---------------------------------------------
# iOS configuration
# ---------------------------------------------

if(BLITZDJ_PLATFORM STREQUAL "iOS")
    set_target_properties(BlitzDJCore PROPERTIES
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    )
endif()

# ---------------------------------------------
# Desktop executable
# ---------------------------------------------

if(BLITZDJ_BUILD_CLI)

    set(BLITZDJ_MAIN
        "${BLITZDJ_SOURCE_DIR}/Main.cpp"
    )

    if(NOT EXISTS "${BLITZDJ_MAIN}")
        message(FATAL_ERROR
            "Main.cpp not found: ${BLITZDJ_MAIN}"
        )
    endif()

    add_executable(BlitzDJ
        "${BLITZDJ_MAIN}"
    )

    target_link_libraries(BlitzDJ PRIVATE
        BlitzDJ::Core
    )

    set_target_properties(BlitzDJ PROPERTIES
        OUTPUT_NAME "BlitzDJ"
    )

endif()

# ---------------------------------------------
# Compiler warnings
# ---------------------------------------------

if(BLITZDJ_ENABLE_WARNINGS)

    if(MSVC)
        set(BLITZDJ_WARNING_FLAGS
            /W4
            /permissive-
            /utf-8
            /EHsc
        )

        if(BLITZDJ_WARNINGS_AS_ERRORS)
            list(APPEND BLITZDJ_WARNING_FLAGS /WX)
        endif()

    else()
        set(BLITZDJ_WARNING_FLAGS
            -Wall
            -Wextra
            -Wpedantic
        )

        if(BLITZDJ_WARNINGS_AS_ERRORS)
            list(APPEND BLITZDJ_WARNING_FLAGS -Werror)
        endif()
    endif()

    target_compile_options(BlitzDJCore PRIVATE
        ${BLITZDJ_WARNING_FLAGS}
    )

    if(TARGET BlitzDJ)
        target_compile_options(BlitzDJ PRIVATE
            ${BLITZDJ_WARNING_FLAGS}
        )
    endif()

endif()

# ---------------------------------------------
# Link-time optimization
# ---------------------------------------------

if(BLITZDJ_ENABLE_LTO)

    check_ipo_supported(
        RESULT BLITZDJ_IPO_SUPPORTED
        OUTPUT BLITZDJ_IPO_MESSAGE
    )

    if(BLITZDJ_IPO_SUPPORTED)

        set_property(TARGET BlitzDJCore PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )

        if(TARGET BlitzDJ)
            set_property(TARGET BlitzDJ PROPERTY
                INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
            )
        endif()

    else()
        message(WARNING
            "LTO unavailable: ${BLITZDJ_IPO_MESSAGE}"
        )
    endif()

endif()

# ---------------------------------------------
# Assets
# ---------------------------------------------

if(TARGET BlitzDJ AND EXISTS "${BLITZDJ_ICON}")

    add_custom_command(
        TARGET BlitzDJ POST_BUILD

        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:BlitzDJ>/assets"

        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${BLITZDJ_ICON}"
            "$<TARGET_FILE_DIR:BlitzDJ>/assets/icon.png"

        COMMENT "Copying BlitzDJ icon"
        VERBATIM
    )

endif()

if(TARGET BlitzDJ AND EXISTS
    "${BLITZDJ_CONFIG_DIR}/config.json"
)
    add_custom_command(
        TARGET BlitzDJ POST_BUILD

        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:BlitzDJ>/config"

        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${BLITZDJ_CONFIG_DIR}/config.json"
            "$<TARGET_FILE_DIR:BlitzDJ>/config/config.json"

        COMMENT "Copying BlitzDJ configuration"
        VERBATIM
    )
endif()

# ---------------------------------------------
# Installation
# ---------------------------------------------

install(TARGETS BlitzDJCore
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(TARGET BlitzDJ)
    install(TARGETS BlitzDJ
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()

install(
    FILES ${BLITZDJ_CORE_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/blitzdj"
)

if(BLITZDJ_INSTALL_ASSETS)

    if(EXISTS "${BLITZDJ_ASSETS_DIR}")
        install(
            DIRECTORY "${BLITZDJ_ASSETS_DIR}/"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/BlitzDJ/assets"
        )
    endif()

    if(EXISTS "${BLITZDJ_CONFIG_DIR}/config.json")
        install(
            FILES "${BLITZDJ_CONFIG_DIR}/config.json"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/BlitzDJ/config"
        )
    endif()

endif()

# ---------------------------------------------
# Tests
# ---------------------------------------------

if(BLITZDJ_BUILD_TESTS)
    include(CTest)

    if(EXISTS "${BLITZDJ_ROOT}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        message(WARNING
            "Tests enabled but tests/CMakeLists.txt is missing"
        )
    endif()
endif()

# ---------------------------------------------
# Packaging
# ---------------------------------------------

set(CPACK_PACKAGE_NAME "BlitzDJ")
set(CPACK_PACKAGE_VENDOR "TheBlitzTeam")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Intelligent Spotify DJ powered by C++"
)

if(WIN32)
    set(CPACK_GENERATOR "ZIP")

elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")

else()
    set(CPACK_GENERATOR "TGZ;ZIP")
endif()

include(CPack)

# ---------------------------------------------
# Summary
# ---------------------------------------------

message(STATUS "")
message(STATUS "======================================")
message(STATUS " BlitzDJ Build Configuration")
message(STATUS "======================================")
message(STATUS " Version: ${PROJECT_VERSION}")
message(STATUS " Platform: ${BLITZDJ_PLATFORM}")
message(STATUS " Mobile: ${BLITZDJ_MOBILE}")
message(STATUS " C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS " Spotify: ${BLITZDJ_ENABLE_SPOTIFY}")
message(STATUS " Bluetooth backend: ${BLITZDJ_HAS_BLUETOOTH}")
message(STATUS " CLI: ${BLITZDJ_BUILD_CLI}")
message(STATUS " LTO: ${BLITZDJ_ENABLE_LTO}")
message(STATUS " Tests: ${BLITZDJ_BUILD_TESTS}")

if(ANDROID)
    message(STATUS " Android ABI: ${ANDROID_ABI}")
endif()

message(STATUS "======================================")
