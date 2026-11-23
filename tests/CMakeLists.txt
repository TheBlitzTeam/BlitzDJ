add_executable(BlitzDJTests CoreTests.cpp)

target_link_libraries(BlitzDJTests PRIVATE BlitzDJ::Core)
target_include_directories(BlitzDJTests PRIVATE "${BLITZDJ_SOURCE_DIR}")

add_test(NAME BlitzDJTests COMMAND BlitzDJTests)
