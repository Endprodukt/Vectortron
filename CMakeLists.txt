cmake_minimum_required(VERSION 3.21)

project(vectortron VERSION 0.1.0 LANGUAGES C CXX)

# Vectortron: a 3D vector tribute to Robotron: 2084 for VectorPie.
#
#   cmake --preset pc    && cmake --build --preset pc     # Windows .exe
#   cmake --preset pi    && cmake --build --preset pi     # the cabinet (arm64)
#   cmake --preset linux && cmake --build --preset linux  # Linux, for testing
#   cmake --build --preset pi --target vpk                # the package
#
# The SDK comes from, in this order:
#   1. -DVECTORPIE_SDK_DIR=<VectorPie>/sdk   (built from its source tree)
#   2. an installed SDK (find_package, CMAKE_PREFIX_PATH = ~/VectorPie/dist/<platform>)
#   3. GitHub (Goldfinch2/VectorPie), fetched at configure time

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(VECTORPIE_SDK_DIR "" CACHE PATH "VectorPie SDK source tree (VectorPie/sdk); empty = installed or fetched")
set(VECTORPIE_GIT_TAG "main" CACHE STRING "VectorPie revision fetched when no SDK is found")

if(VECTORPIE_SDK_DIR)
    set(VP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(VP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${VECTORPIE_SDK_DIR} vectorpie-sdk)
else()
    find_package(VectorPie 0.1 CONFIG QUIET)
    if(NOT VectorPie_FOUND)
        message(STATUS "VectorPie SDK not installed: fetching it from GitHub")
        include(FetchContent)
        set(VP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(VP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(vectorpie
            GIT_REPOSITORY https://github.com/Goldfinch2/VectorPie.git
            GIT_TAG        ${VECTORPIE_GIT_TAG}
            GIT_SHALLOW    TRUE
            SOURCE_SUBDIR  sdk)
        FetchContent_MakeAvailable(vectorpie)
    endif()
endif()

add_executable(vectortron
    src/main.c
    src/game.c
    src/waves.c
    src/models.c
    src/render.c
    src/sound.c
    src/controls.c
    src/settings.c
    src/font3d.c
    src/demo.c
)
vp_program(vectortron)
if(UNIX)
    target_link_libraries(vectortron PRIVATE m)
endif()

# A sounds/ folder is optional: WAV or Ogg files there replace the sounds the
# game makes itself (see README.md). Copied next to the executable.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
    add_custom_command(TARGET vectortron POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:vectortron>/assets)
endif()

# --- the package: cmake --build --preset pi --target vpk -----------------------

# VECTORPIE_PACK (vp-pack) is set by the SDK however it was found.
configure_file(pkg/game.ini.in ${CMAKE_CURRENT_BINARY_DIR}/game.ini @ONLY)
set(VT_STAGE ${CMAKE_CURRENT_BINARY_DIR}/stage/vectortron)
add_custom_target(vpk
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${VT_STAGE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${VT_STAGE}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:vectortron> ${VT_STAGE}/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/game.ini ${VT_STAGE}/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/pkg/leds.cfg ${VT_STAGE}/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/pkg/marquee.png ${VT_STAGE}/
    COMMAND sh ${VECTORPIE_PACK} ${VT_STAGE} ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS vectortron
    COMMENT "Packing vectortron-${PROJECT_VERSION}.vpk"
    VERBATIM)

# vt-gallery: every model on one screen, turning (for looking at them).
add_executable(vt-gallery tools/gallery.c src/models.c)
vp_program(vt-gallery)
if(UNIX)
    target_link_libraries(vt-gallery PRIVATE m)
endif()

# vt-sim: the rules without a screen, played by the demo player (a test).
add_executable(vt-sim tests/sim.c src/game.c src/waves.c src/demo.c src/render.c src/models.c
               src/font3d.c src/sound.c)
vp_program(vt-sim)
if(UNIX)
    target_link_libraries(vt-sim PRIVATE m)
endif()
enable_testing()
add_test(NAME sim COMMAND vt-sim 10 40)
