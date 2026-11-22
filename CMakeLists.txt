cmake_minimum_required(VERSION 3.20)

project(BlitzDJ
    VERSION 0.1.0
    DESCRIPTION "An intelligent Spotify DJ powered by C++"
    HOMEPAGE_URL "https://github.com/TheBlitzTeam/BlitzDJ"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)
include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BLITZDJ_ENABLE_BLUETOOTH
    "Enable Bluetooth support" ON
)

option(BLITZDJ_ENABLE_LTO
    "Enable link-time optimization in Release builds" ON
)

option(BLITZDJ_BUILD_TESTS
    "Build BlitzDJ tests" OFF
)

option(BLITZDJ_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors" OFF
)

option(BLITZDJ_INSTALL_ASSETS
    "Install BlitzDJ assets" ON
)

option(BLITZDJ_USE_SYSTEM_JSON
    "Use an installed nlohmann_json package" OFF
)

set(BLITZDJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(BLITZDJ_SOURCE_DIR "${BLITZDJ_ROOT}/src")
set(BLITZDJ_INCLUDE_DIR "${BLITZDJ_ROOT}/include")
set(BLITZDJ_ASSETS_DIR "${BLITZDJ_ROOT}/assets")

set(BLITZDJ_ICON
    "${BLITZDJ_ASSETS_DIR}/icon.png"
)

message(STATUS "====================================")
message(STATUS "BlitzDJ ${PROJECT_VERSION}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Bluetooth: ${BLITZDJ_ENABLE_BLUETOOTH}")
message(STATUS "====================================")

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

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

set(BLITZDJ_SOURCES
    "${BLITZDJ_SOURCE_DIR}/Main.cpp"
    "${BLITZDJ_SOURCE_DIR}/BlitzDJ.cpp"
    "${BLITZDJ_SOURCE_DIR}/BlitzAI.cpp"
    "${BLITZDJ_SOURCE_DIR}/BlitzSpotify.cpp"
)

set(BLITZDJ_HEADERS
    "${BLITZDJ_INCLUDE_DIR}/BlitzDJ.hpp"
    "${BLITZDJ_INCLUDE_DIR}/BlitzAI.hpp"
    "${BLITZDJ_INCLUDE_DIR}/BlitzSpotify.hpp"
)

set(BLITZDJ_BLUETOOTH_AVAILABLE OFF)

if(BLITZDJ_ENABLE_BLUETOOTH)
    list(APPEND BLITZDJ_SOURCES
        "${BLITZDJ_SOURCE_DIR}/Bluetooth.cpp"
    )

    list(APPEND BLITZDJ_HEADERS
        "${BLITZDJ_INCLUDE_DIR}/Bluetooth.hpp"
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(PkgConfig REQUIRED)

        pkg_check_modules(
            GIO REQUIRED
            IMPORTED_TARGET
            gio-2.0
        )

        set(BLITZDJ_BLUETOOTH_AVAILABLE ON)

    elseif(WIN32)
        message(STATUS
            "Windows Bluetooth backend is not implemented yet"
        )

    elseif(APPLE)
        message(STATUS
            "macOS Bluetooth backend is not implemented yet"
        )
    endif()
endif()

if(WIN32)
    set(BLITZDJ_PLATFORM "Windows")
elseif(APPLE)
    set(BLITZDJ_PLATFORM "macOS")
elseif(UNIX)
    set(BLITZDJ_PLATFORM "Linux")
else()
    set(BLITZDJ_PLATFORM "Unknown")
endif()

add_executable(BlitzDJ
    ${BLITZDJ_SOURCES}
    ${BLITZDJ_HEADERS}
)

add_executable(BlitzDJ::BlitzDJ ALIAS BlitzDJ)

target_compile_features(BlitzDJ PRIVATE cxx_std_17)

target_include_directories(BlitzDJ PRIVATE
    "${BLITZDJ_INCLUDE_DIR}"
)

target_link_libraries(BlitzDJ PRIVATE
    Threads::Threads
    CURL::libcurl
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

if(BLITZDJ_BLUETOOTH_AVAILABLE)
    target_link_libraries(BlitzDJ PRIVATE
        PkgConfig::GIO
    )
endif()

target_compile_definitions(BlitzDJ PRIVATE
    BLITZDJ_VERSION="${PROJECT_VERSION}"
    BLITZDJ_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BLITZDJ_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BLITZDJ_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    BLITZDJ_PLATFORM="${BLITZDJ_PLATFORM}"
    BLITZDJ_ICON_PATH="assets/icon.png"
    BLITZDJ_HAS_BLUETOOTH=$<BOOL:${BLITZDJ_BLUETOOTH_AVAILABLE}>
)

if(MSVC)
    target_compile_options(BlitzDJ PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
    )

    if(BLITZDJ_WARNINGS_AS_ERRORS)
        target_compile_options(BlitzDJ PRIVATE /WX)
    endif()
else()
    target_compile_options(BlitzDJ PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    if(BLITZDJ_WARNINGS_AS_ERRORS)
        target_compile_options(BlitzDJ PRIVATE -Werror)
    endif()
endif()

if(BLITZDJ_ENABLE_LTO)
    include(CheckIPOSupported)

    check_ipo_supported(
        RESULT BLITZDJ_IPO_SUPPORTED
        OUTPUT BLITZDJ_IPO_ERROR
    )

    if(BLITZDJ_IPO_SUPPORTED)
        set_property(
            TARGET BlitzDJ
            PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )

        message(STATUS "LTO enabled for Release builds")
    else()
        message(STATUS
            "LTO unavailable: ${BLITZDJ_IPO_ERROR}"
        )
    endif()
endif()

if(EXISTS "${BLITZDJ_ICON}")
    add_custom_command(
        TARGET BlitzDJ
        POST_BUILD

        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:BlitzDJ>/assets"

        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${BLITZDJ_ICON}"
            "$<TARGET_FILE_DIR:BlitzDJ>/assets/icon.png"

        COMMENT "Copying BlitzDJ icon"
        VERBATIM
    )
else()
    message(WARNING
        "BlitzDJ icon not found: ${BLITZDJ_ICON}"
    )
endif()

if(EXISTS "${BLITZDJ_ROOT}/config/config.json")
    add_custom_command(
        TARGET BlitzDJ
        POST_BUILD

        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:BlitzDJ>/config"

        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${BLITZDJ_ROOT}/config/config.json"
            "$<TARGET_FILE_DIR:BlitzDJ>/config/config.json"

        COMMENT "Copying BlitzDJ configuration"
        VERBATIM
    )
endif()

install(TARGETS BlitzDJ
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(BLITZDJ_INSTALL_ASSETS)
    if(EXISTS "${BLITZDJ_ASSETS_DIR}")
        install(
            DIRECTORY "${BLITZDJ_ASSETS_DIR}/"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/assets"
        )
    endif()

    if(EXISTS "${BLITZDJ_ROOT}/config/config.json")
        install(
            FILES "${BLITZDJ_ROOT}/config/config.json"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/config"
        )
    endif()
endif()

if(BLITZDJ_BUILD_TESTS)
    enable_testing()

    if(EXISTS "${BLITZDJ_ROOT}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        message(WARNING
            "Tests enabled, but tests/CMakeLists.txt was not found"
        )
    endif()
endif()

set(CPACK_PACKAGE_NAME "BlitzDJ")
set(CPACK_PACKAGE_VENDOR "TheBlitzTeam")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "An intelligent Spotify DJ powered by C++"
)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
else()
    set(CPACK_GENERATOR "TGZ;ZIP")
endif()

include(CPack)

message(STATUS "")
message(STATUS "BlitzDJ Configuration Summary")
message(STATUS "------------------------------------")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Platform: ${BLITZDJ_PLATFORM}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Bluetooth: ${BLITZDJ_ENABLE_BLUETOOTH}")
message(STATUS "Bluetooth backend: ${BLITZDJ_BLUETOOTH_AVAILABLE}")
message(STATUS "LTO: ${BLITZDJ_ENABLE_LTO}")
message(STATUS "Tests: ${BLITZDJ_BUILD_TESTS}")
message(STATUS "Install assets: ${BLITZDJ_INSTALL_ASSETS}")
message(STATUS "------------------------------------")
