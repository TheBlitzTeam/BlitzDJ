cmake_minimum_required(VERSION 3.21)

project(BlitzDJ
    VERSION 0.3.0
    DESCRIPTION "BlitzDJ - Intelligent Spotify Music Controller"
    HOMEPAGE_URL "https://github.com/TheBlitzTeam/BlitzDJ"
    LANGUAGES C CXX
)

include(GNUInstallDirs)
include(FetchContent)
include(CheckIPOSupported)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

if(BLITZDJ_MOBILE)
    set(BLITZDJ_DEFAULT_CLI OFF)
else()
    set(BLITZDJ_DEFAULT_CLI ON)
endif()

if(BLITZDJ_PLATFORM STREQUAL "Android")
    set(BLITZDJ_DEFAULT_HTTP "android")
elseif(BLITZDJ_PLATFORM STREQUAL "iOS")
    set(BLITZDJ_DEFAULT_HTTP "apple")
else()
    set(BLITZDJ_DEFAULT_HTTP "curl")
endif()

option(BLITZDJ_ENABLE_BLUETOOTH "Enable Bluetooth support" ON)
option(BLITZDJ_ENABLE_LTO "Enable Release link-time optimization" ON)
option(BLITZDJ_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(BLITZDJ_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(BLITZDJ_BUILD_TESTS "Build tests" OFF)
option(BLITZDJ_BUILD_CLI "Build the terminal application" ${BLITZDJ_DEFAULT_CLI})
option(BLITZDJ_BUILD_GUI "Build the desktop application" ${BLITZDJ_DEFAULT_CLI})
option(BLITZDJ_USE_SYSTEM_JSON "Use system nlohmann_json" OFF)
option(BLITZDJ_INSTALL_ASSETS "Install application assets" ON)

set(BLITZDJ_SPOTIFY_CLIENT_ID "7941be601f504eb39407677cd5738cb5" CACHE STRING "Default Spotify Client ID")

set(BLITZDJ_HTTP_BACKEND "${BLITZDJ_DEFAULT_HTTP}" CACHE STRING "HTTP backend: curl, android or apple")
set_property(CACHE BLITZDJ_HTTP_BACKEND PROPERTY STRINGS curl android apple)

if(BLITZDJ_MOBILE AND BLITZDJ_BUILD_CLI)
    message(STATUS "The terminal application is unavailable on ${BLITZDJ_PLATFORM}")
    set(BLITZDJ_BUILD_CLI OFF CACHE BOOL "Build the terminal application" FORCE)
endif()

if(BLITZDJ_MOBILE AND BLITZDJ_BUILD_GUI)
    message(STATUS "The desktop application is unavailable on ${BLITZDJ_PLATFORM}")
    set(BLITZDJ_BUILD_GUI OFF CACHE BOOL "Build the desktop application" FORCE)
endif()

set(BLITZDJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(BLITZDJ_SOURCE_DIR "${BLITZDJ_ROOT}/src")
set(BLITZDJ_INCLUDE_DIR "${BLITZDJ_ROOT}/include")
set(BLITZDJ_ASSETS_DIR "${BLITZDJ_ROOT}/assets")
set(BLITZDJ_CONFIG_DIR "${BLITZDJ_ROOT}/config")
set(BLITZDJ_ICON "${BLITZDJ_ASSETS_DIR}/icon.png")

find_package(Threads REQUIRED)

if(BLITZDJ_USE_SYSTEM_JSON)
    find_package(nlohmann_json 3.11 REQUIRED)
else()
    set(BLITZDJ_FETCH_OPTIONS)

    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
        list(APPEND BLITZDJ_FETCH_OPTIONS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()

    FetchContent_Declare(
        nlohmann_json
        URL "https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz"
        ${BLITZDJ_FETCH_OPTIONS}
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

set(BLITZDJ_PUBLIC_HEADERS
    "${BLITZDJ_INCLUDE_DIR}/BlitzDJ.hpp"
    "${BLITZDJ_INCLUDE_DIR}/BlitzAI.hpp"
    "${BLITZDJ_INCLUDE_DIR}/BlitzSpotify.hpp"
    "${BLITZDJ_INCLUDE_DIR}/HttpClient.hpp"
    "${BLITZDJ_INCLUDE_DIR}/BlitzDJBridge.h"
)

set(BLITZDJ_CORE_SOURCES
    "${BLITZDJ_SOURCE_DIR}/BlitzDJ.cpp"
    "${BLITZDJ_SOURCE_DIR}/BlitzAI.cpp"
    "${BLITZDJ_SOURCE_DIR}/BlitzSpotify.cpp"
    "${BLITZDJ_SOURCE_DIR}/BlitzDJBridge.cpp"
    "${BLITZDJ_SOURCE_DIR}/Crypto.cpp"
    "${BLITZDJ_SOURCE_DIR}/Crypto.hpp"
)

if(BLITZDJ_HTTP_BACKEND STREQUAL "curl")
    find_package(CURL REQUIRED)
    list(APPEND BLITZDJ_CORE_SOURCES "${BLITZDJ_SOURCE_DIR}/http/CurlHttpClient.cpp")
elseif(BLITZDJ_HTTP_BACKEND STREQUAL "android")
    if(NOT ANDROID)
        message(FATAL_ERROR "The android HTTP backend requires the Android NDK")
    endif()
    list(APPEND BLITZDJ_CORE_SOURCES
        "${BLITZDJ_SOURCE_DIR}/platform/android/AndroidJni.hpp"
        "${BLITZDJ_SOURCE_DIR}/platform/android/AndroidHttpClient.cpp"
        "${BLITZDJ_SOURCE_DIR}/platform/android/AndroidBridge.cpp"
    )
elseif(BLITZDJ_HTTP_BACKEND STREQUAL "apple")
    if(NOT APPLE)
        message(FATAL_ERROR "The apple HTTP backend requires an Apple platform")
    endif()
    enable_language(OBJCXX)
    list(APPEND BLITZDJ_CORE_SOURCES "${BLITZDJ_SOURCE_DIR}/platform/apple/AppleHttpClient.mm")
    set_source_files_properties(
        "${BLITZDJ_SOURCE_DIR}/platform/apple/AppleHttpClient.mm"
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc"
    )
else()
    message(FATAL_ERROR "Unknown HTTP backend: ${BLITZDJ_HTTP_BACKEND}")
endif()

if(NOT BLITZDJ_MOBILE)
    list(APPEND BLITZDJ_CORE_SOURCES "${BLITZDJ_SOURCE_DIR}/OAuthCallbackServer.cpp")
    list(APPEND BLITZDJ_PUBLIC_HEADERS "${BLITZDJ_INCLUDE_DIR}/OAuthCallbackServer.hpp")
endif()

set(BLITZDJ_HAS_BLUETOOTH OFF)

list(APPEND BLITZDJ_CORE_SOURCES "${BLITZDJ_SOURCE_DIR}/Bluetooth.cpp")
list(APPEND BLITZDJ_PUBLIC_HEADERS "${BLITZDJ_INCLUDE_DIR}/Bluetooth.hpp")

if(BLITZDJ_ENABLE_BLUETOOTH)
    if(BLITZDJ_PLATFORM STREQUAL "Linux")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
        set(BLITZDJ_HAS_BLUETOOTH ON)
    else()
        message(STATUS "Native Bluetooth backend unavailable on ${BLITZDJ_PLATFORM}")
    endif()
endif()

if(ANDROID)
    add_library(BlitzDJCore SHARED ${BLITZDJ_CORE_SOURCES} ${BLITZDJ_PUBLIC_HEADERS})
else()
    add_library(BlitzDJCore STATIC ${BLITZDJ_CORE_SOURCES} ${BLITZDJ_PUBLIC_HEADERS})
    set_target_properties(BlitzDJCore PROPERTIES VERSION "${PROJECT_VERSION}")
endif()

add_library(BlitzDJ::Core ALIAS BlitzDJCore)

set_target_properties(BlitzDJCore PROPERTIES OUTPUT_NAME "blitzdj")

target_compile_features(BlitzDJCore PUBLIC cxx_std_17)

target_include_directories(BlitzDJCore
    PUBLIC
        $<BUILD_INTERFACE:${BLITZDJ_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/blitzdj>
    PRIVATE
        "${BLITZDJ_SOURCE_DIR}"
        "${BLITZDJ_SOURCE_DIR}/platform/android"
)

target_link_libraries(BlitzDJCore PUBLIC
    Threads::Threads
    nlohmann_json::nlohmann_json
)

if(BLITZDJ_HTTP_BACKEND STREQUAL "curl")
    target_link_libraries(BlitzDJCore PRIVATE CURL::libcurl)
elseif(BLITZDJ_HTTP_BACKEND STREQUAL "android")
    target_link_libraries(BlitzDJCore PRIVATE log)
    target_link_options(BlitzDJCore PRIVATE "-Wl,-z,max-page-size=16384")
elseif(BLITZDJ_HTTP_BACKEND STREQUAL "apple")
    target_link_libraries(BlitzDJCore PUBLIC "-framework Foundation")
endif()

if(BLITZDJ_HAS_BLUETOOTH)
    target_link_libraries(BlitzDJCore PRIVATE PkgConfig::GIO)
endif()

if(WIN32)
    target_link_libraries(BlitzDJCore PUBLIC ws2_32)
endif()

target_compile_definitions(BlitzDJCore PUBLIC
    BLITZDJ_VERSION="${PROJECT_VERSION}"
    BLITZDJ_PLATFORM="${BLITZDJ_PLATFORM}"
    BLITZDJ_HAS_BLUETOOTH=$<BOOL:${BLITZDJ_HAS_BLUETOOTH}>
    BLITZDJ_MOBILE=$<BOOL:${BLITZDJ_MOBILE}>
    BLITZDJ_DEFAULT_CLIENT_ID="${BLITZDJ_SPOTIFY_CLIENT_ID}"
)

if(BLITZDJ_PLATFORM STREQUAL "iOS")
    set_target_properties(BlitzDJCore PROPERTIES
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    )
endif()

if(BLITZDJ_BUILD_CLI)
    add_executable(BlitzDJ "${BLITZDJ_SOURCE_DIR}/Main.cpp")
    target_link_libraries(BlitzDJ PRIVATE BlitzDJ::Core)
    set_target_properties(BlitzDJ PROPERTIES OUTPUT_NAME "BlitzDJ")

    if(WIN32)
        target_link_libraries(BlitzDJ PRIVATE shell32)
        target_compile_definitions(BlitzDJ PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

if(BLITZDJ_ENABLE_WARNINGS)
    if(MSVC)
        set(BLITZDJ_WARNING_FLAGS /W4 /permissive- /utf-8 /EHsc)

        if(BLITZDJ_WARNINGS_AS_ERRORS)
            list(APPEND BLITZDJ_WARNING_FLAGS /WX)
        endif()
    else()
        set(BLITZDJ_WARNING_FLAGS -Wall -Wextra -Wpedantic)

        if(BLITZDJ_WARNINGS_AS_ERRORS)
            list(APPEND BLITZDJ_WARNING_FLAGS -Werror)
        endif()
    endif()

    target_compile_options(BlitzDJCore PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${BLITZDJ_WARNING_FLAGS}>)

    if(TARGET BlitzDJ)
        target_compile_options(BlitzDJ PRIVATE ${BLITZDJ_WARNING_FLAGS})
    endif()
elseif(MSVC)
    target_compile_options(BlitzDJCore PRIVATE /utf-8 /EHsc)
endif()

if(BLITZDJ_BUILD_GUI)
    find_package(OpenGL REQUIRED)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        glfw
        URL "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip"
        ${BLITZDJ_FETCH_OPTIONS}
    )

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
        GIT_TAG "f401021d5a5d56fe2304056c391e78f81c8d4b8f"
    )

    FetchContent_MakeAvailable(glfw imgui)

    set(BLITZDJ_STB_DIR "${CMAKE_BINARY_DIR}/_deps/stb")
    set(BLITZDJ_STB_HEADER "${BLITZDJ_STB_DIR}/stb_image.h")

    if(NOT EXISTS "${BLITZDJ_STB_HEADER}")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/nothings/stb/6b5799bae8a1fe3792691ef4b72ae26434c3e799/stb_image.h"
            "${BLITZDJ_STB_HEADER}"
            STATUS BLITZDJ_STB_STATUS
            TLS_VERIFY ON
        )
        list(GET BLITZDJ_STB_STATUS 0 BLITZDJ_STB_CODE)

        if(NOT BLITZDJ_STB_CODE EQUAL 0)
            file(REMOVE "${BLITZDJ_STB_HEADER}")
            message(FATAL_ERROR "Could not download stb_image.h: ${BLITZDJ_STB_STATUS}")
        endif()
    endif()

    function(blitzdj_embed_file input symbol output)
        if(NOT EXISTS "${output}" OR "${input}" IS_NEWER_THAN "${output}")
            file(READ "${input}" content HEX)
            string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," content "${content}")
            file(WRITE "${output}"
                "#include <cstddef>\n"
                "extern const unsigned char ${symbol}[] = {${content}};\n"
                "extern const std::size_t ${symbol}Size = sizeof(${symbol});\n"
            )
        endif()
    endfunction()

    set(BLITZDJ_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
    blitzdj_embed_file("${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf" blitzdjFont "${BLITZDJ_GENERATED_DIR}/EmbeddedFont.cpp")
    blitzdj_embed_file("${BLITZDJ_ICON}" blitzdjIcon "${BLITZDJ_GENERATED_DIR}/EmbeddedIcon.cpp")

    add_library(BlitzDJImGui STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/imgui_demo.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    )

    target_include_directories(BlitzDJImGui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
    target_link_libraries(BlitzDJImGui PUBLIC glfw OpenGL::GL ${CMAKE_DL_LIBS})

    add_executable(BlitzDJApp
        "${BLITZDJ_SOURCE_DIR}/app/AppMain.cpp"
        "${BLITZDJ_SOURCE_DIR}/app/App.cpp"
        "${BLITZDJ_SOURCE_DIR}/app/App.hpp"
        "${BLITZDJ_SOURCE_DIR}/app/ImageCache.cpp"
        "${BLITZDJ_SOURCE_DIR}/app/ImageCache.hpp"
        "${BLITZDJ_SOURCE_DIR}/app/Icons.cpp"
        "${BLITZDJ_SOURCE_DIR}/app/Icons.hpp"
        "${BLITZDJ_SOURCE_DIR}/app/Platform.cpp"
        "${BLITZDJ_SOURCE_DIR}/app/Platform.hpp"
        "${BLITZDJ_SOURCE_DIR}/app/Strings.cpp"
        "${BLITZDJ_SOURCE_DIR}/app/Strings.hpp"
        "${BLITZDJ_SOURCE_DIR}/app/TaskQueue.hpp"
        "${BLITZDJ_GENERATED_DIR}/EmbeddedFont.cpp"
        "${BLITZDJ_GENERATED_DIR}/EmbeddedIcon.cpp"
    )

    target_include_directories(BlitzDJApp PRIVATE "${BLITZDJ_SOURCE_DIR}/app" "${BLITZDJ_STB_DIR}")
    target_link_libraries(BlitzDJApp PRIVATE BlitzDJ::Core BlitzDJImGui)

    if(WIN32)
        set_target_properties(BlitzDJApp PROPERTIES WIN32_EXECUTABLE TRUE)
        target_link_libraries(BlitzDJApp PRIVATE shell32)
        target_compile_definitions(BlitzDJApp PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)

        if(MSVC)
            target_link_options(BlitzDJApp PRIVATE "/ENTRY:mainCRTStartup")
        else()
            target_link_options(BlitzDJApp PRIVATE "-mwindows")
        endif()
    endif()

    if(APPLE)
        target_compile_definitions(BlitzDJImGui PUBLIC GL_SILENCE_DEPRECATION)
    endif()

    if(BLITZDJ_ENABLE_WARNINGS)
        target_compile_options(BlitzDJApp PRIVATE ${BLITZDJ_WARNING_FLAGS})
    endif()

    install(TARGETS BlitzDJApp RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if(BLITZDJ_ENABLE_LTO)
    check_ipo_supported(RESULT BLITZDJ_IPO_SUPPORTED OUTPUT BLITZDJ_IPO_MESSAGE LANGUAGES CXX)

    if(BLITZDJ_IPO_SUPPORTED)
        set_property(TARGET BlitzDJCore PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

        if(TARGET BlitzDJ)
            set_property(TARGET BlitzDJ PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        endif()

        if(TARGET BlitzDJApp)
            set_property(TARGET BlitzDJApp PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        endif()
    else()
        message(STATUS "LTO unavailable: ${BLITZDJ_IPO_MESSAGE}")
    endif()
endif()

if(TARGET BlitzDJ AND EXISTS "${BLITZDJ_ICON}")
    add_custom_command(TARGET BlitzDJ POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:BlitzDJ>/assets"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BLITZDJ_ICON}" "$<TARGET_FILE_DIR:BlitzDJ>/assets/icon.png"
        COMMENT "Copying BlitzDJ icon"
        VERBATIM
    )
endif()

install(TARGETS BlitzDJCore
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(TARGET BlitzDJ)
    install(TARGETS BlitzDJ RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

install(FILES ${BLITZDJ_PUBLIC_HEADERS} "${BLITZDJ_INCLUDE_DIR}/module.modulemap" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/blitzdj")

if(BLITZDJ_INSTALL_ASSETS AND EXISTS "${BLITZDJ_ASSETS_DIR}")
    install(DIRECTORY "${BLITZDJ_ASSETS_DIR}/" DESTINATION "${CMAKE_INSTALL_DATADIR}/BlitzDJ/assets")
endif()

if(BLITZDJ_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

set(CPACK_PACKAGE_NAME "BlitzDJ")
set(CPACK_PACKAGE_VENDOR "TheBlitzTeam")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Intelligent Spotify DJ powered by C++")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
else()
    set(CPACK_GENERATOR "TGZ;ZIP")
endif()

include(CPack)

message(STATUS "BlitzDJ ${PROJECT_VERSION} | ${BLITZDJ_PLATFORM} | HTTP: ${BLITZDJ_HTTP_BACKEND} | Bluetooth: ${BLITZDJ_HAS_BLUETOOTH} | CLI: ${BLITZDJ_BUILD_CLI} | App: ${BLITZDJ_BUILD_GUI}")
