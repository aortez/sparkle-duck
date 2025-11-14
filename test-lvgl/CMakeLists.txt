cmake_minimum_required(VERSION 3.10)
project(lvgl)

# Allow linking to targets from subdirectories.
cmake_policy(SET CMP0079 NEW)

# Add Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Add libdatachannel for WebSocket/WebRTC support.
FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.23.2
)
# Disable tests and examples for libdatachannel.
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libdatachannel)

# Add nlohmann/json for JSON serialization.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Add msgpack-c for binary serialization.
FetchContent_Declare(
    msgpack
    GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git
    GIT_TAG cpp-6.1.0
)
set(MSGPACK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MSGPACK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MSGPACK_USE_BOOST ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(msgpack)

# Find Boost for msgpack variant support.
find_package(Boost REQUIRED)

# Add zpp_bits for high-performance binary serialization.
FetchContent_Declare(
    zpp_bits
    GIT_REPOSITORY https://github.com/eyalz800/zpp_bits.git
    GIT_TAG v4.4.25
)
FetchContent_MakeAvailable(zpp_bits)

# qlibs/reflect is header-only, downloaded to src/core/reflect

set(CONF_PATH "${PROJECT_SOURCE_DIR}/lv_conf.h")

foreach(BACKEND_NAME "SDL" "LINUX_DRM" "LINUX_FBDEV" "X11" "WAYLAND" "OPENGLES")

    execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    COMMAND "scripts/backend_conf.sh" ${BACKEND_NAME} ${CONF_PATH} OUTPUT_VARIABLE IS_BACKEND_ENABLED)
    set("LV_USE_${BACKEND_NAME}" ${IS_BACKEND_ENABLED})

endforeach()

# Uncomment if the program needs debugging
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")

set(CMAKE_C_STANDARD 99) # LVGL officially supports C99 and above
set(CMAKE_CXX_STANDARD 23) #C++23
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# Configuration
if (LV_USE_SDL)

    message("Including SDL2 support")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

    list(APPEND PKG_CONFIG_LIB ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})
    list(APPEND PKG_CONFIG_INC ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})

    list(APPEND LV_LINUX_BACKEND_SRC src/ui/lib/display_backends/sdl.cpp)
endif()


if (LV_USE_WAYLAND)

    message("Including Wayland support")

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
    pkg_check_modules(WAYLAND_CURSOR REQUIRED wayland-cursor)
    pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)

    list(APPEND PKG_CONFIG_LIB ${WAYLAND_CLIENT_LIBRARIES})
    list(APPEND PKG_CONFIG_LIB ${WAYLAND_CURSOR_LIBRARIES})
    list(APPEND PKG_CONFIG_LIB ${XKBCOMMON_LIBRARIES})

    # Wayland protocols
    pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=1.25)
    pkg_get_variable(WAYLAND_PROTOCOLS_BASE wayland-protocols pkgdatadir)

    execute_process(COMMAND "scripts/gen_wl_protocols.sh" OUTPUT_VARIABLE WAYLAND_PROTOCOLS_SRC)

    list(APPEND PKG_CONFIG_INC "${PROJECT_SOURCE_DIR}/wl_protocols")
    list(APPEND LV_LINUX_BACKEND_SRC src/ui/lib/display_backends/wayland.cpp wl_protocols/wayland_xdg_shell.c)

endif()

if (LV_USE_X11)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED x11)

    message("Including X11 support")

    list(APPEND PKG_CONFIG_INC ${X11_INCLUDE_DIRS})
    list(APPEND PKG_CONFIG_LIB ${X11_LIBRARIES})
    list(APPEND LV_LINUX_BACKEND_SRC src/ui/lib/display_backends/x11.cpp)

endif()

if (LV_USE_LINUX_FBDEV)

    # FBDEV has no dependencies
    message("Including FBDEV support")
    list(APPEND LV_LINUX_BACKEND_SRC src/ui/lib/display_backends/fbdev.cpp)

endif()

foreach(arg ${PKG_CONFIG_LIB})
    string(APPEND LVGL_PKG_CONFIG_EXT_LIB " -l${arg}")
endforeach()

string(APPEND LVGL_PKG_CONFIG_LIB "-llvgl_linux")

file(GLOB LV_LINUX_SRC src/ui/lib/*.c src/ui/lib/*.cpp)
set(LV_LINUX_INC src/ui/lib)

set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

add_subdirectory(./lvgl ${CMAKE_BINARY_DIR}/lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/lib ${PKG_CONFIG_INC})
target_link_libraries(lvgl PUBLIC ${PKG_CONFIG_LIB})

add_subdirectory(./spdlog ${CMAKE_BINARY_DIR}/spdlog)

# Add args header-only library
set(ARGS_BUILD_EXAMPLE OFF CACHE BOOL "Build examples" FORCE)
set(ARGS_BUILD_UNITTESTS OFF CACHE BOOL "Build unit tests" FORCE)
add_subdirectory(./external/args ${CMAKE_BINARY_DIR}/args)

add_library(lvgl_linux STATIC ${LV_LINUX_SRC} ${LV_LINUX_BACKEND_SRC})
target_include_directories(lvgl_linux PRIVATE ${LV_LINUX_INC} ${PROJECT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/src/ui/lib ${zpp_bits_SOURCE_DIR} ${PKG_CONFIG_INC})
target_link_libraries(lvgl_linux PUBLIC msgpack-cxx nlohmann_json::nlohmann_json spdlog::spdlog ${PKG_CONFIG_LIB})

# ============================================================================
# Component Libraries
# ============================================================================

# Core library - minimal shared types for serialization.
add_library(sparkle-duck-core STATIC
    src/core/Cell.cpp
    src/core/MaterialType.cpp
    src/core/StateMachineBase.cpp
    src/core/Timers.cpp
    src/core/Vector2d.cpp
    src/core/Vector2i.cpp
)
target_include_directories(sparkle-duck-core PUBLIC ${CMAKE_SOURCE_DIR}/src ${zpp_bits_SOURCE_DIR})
target_link_libraries(sparkle-duck-core PUBLIC nlohmann_json::nlohmann_json msgpack-cxx spdlog::spdlog)

# Server library - physics engine and server logic.
add_library(sparkle-duck-server-lib STATIC
    # Server state machine.
    src/server/StateMachine.cpp
    src/server/EventProcessor.cpp
    src/server/states/Idle.cpp
    src/server/states/Shutdown.cpp
    src/server/states/SimPaused.cpp
    src/server/states/SimRunning.cpp
    src/server/states/Startup.cpp

    # Server API commands.
    src/server/api/CellGet.cpp
    src/server/api/CellSet.cpp
    src/server/api/DiagramGet.cpp
    src/server/api/Exit.cpp
    src/server/api/FrameReady.cpp
    src/server/api/GravitySet.cpp
    src/server/api/PerfStatsGet.cpp
    src/server/api/PhysicsSettingsGet.cpp
    src/server/api/PhysicsSettingsSet.cpp
    src/server/api/Reset.cpp
    src/server/api/ScenarioConfigSet.cpp
    src/server/api/SeedAdd.cpp
    src/server/api/SimRun.cpp
    src/server/api/SpawnDirtBall.cpp
    src/server/api/StateGet.cpp
    src/server/api/TimerStatsGet.cpp
    src/server/api/WorldResize.cpp

    # Server network.
    src/server/network/CommandDeserializerJson.cpp
    src/server/network/WebSocketServer.cpp

    # Scenarios.
    src/server/scenarios/ScenarioRegistry.cpp
    src/server/scenarios/ScenarioWorldEventGenerator.cpp
    src/server/scenarios/scenarios/DamBreakScenario.cpp
    src/server/scenarios/scenarios/EmptyScenario.cpp
    src/server/scenarios/scenarios/FallingDirtScenario.cpp
    src/server/scenarios/scenarios/RainingScenario.cpp
    src/server/scenarios/scenarios/SandboxScenario.cpp
    src/server/scenarios/scenarios/TreeGerminationScenario.cpp
    src/server/scenarios/scenarios/WaterEqualizationScenario.cpp

    # Physics engine.
    src/core/World.cpp
    src/core/WorldAdhesionCalculator.cpp
    src/core/WorldAirResistanceCalculator.cpp
    src/core/WorldCalculatorBase.cpp
    src/core/WorldCohesionCalculator.cpp
    src/core/WorldCollisionCalculator.cpp
    src/core/WorldDiagramGeneratorEmoji.cpp
    src/core/WorldEventGenerator.cpp
    src/core/WorldFrictionCalculator.cpp
    src/core/WorldInterpolationTool.cpp
    src/core/WorldPressureCalculator.cpp
    src/core/WorldSupportCalculator.cpp

    # Organism system.
    src/core/organisms/Tree.cpp
    src/core/organisms/TreeManager.cpp
    src/core/organisms/brains/RuleBasedBrain.cpp
)
target_include_directories(sparkle-duck-server-lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${zpp_bits_SOURCE_DIR})
target_link_libraries(sparkle-duck-server-lib PUBLIC sparkle-duck-core datachannel-static)

# UI library - UI components and client logic.
add_library(sparkle-duck-ui-lib STATIC
    # UI state machine.
    src/ui/state-machine/StateMachine.cpp
    src/ui/state-machine/EventProcessor.cpp
    src/ui/state-machine/states/Disconnected.cpp
    src/ui/state-machine/states/Paused.cpp
    src/ui/state-machine/states/Shutdown.cpp
    src/ui/state-machine/states/SimRunning.cpp
    src/ui/state-machine/states/StartMenu.cpp
    src/ui/state-machine/states/Startup.cpp

    # UI API commands.
    src/ui/state-machine/api/DrawDebugToggle.cpp
    src/ui/state-machine/api/Exit.cpp
    src/ui/state-machine/api/MouseDown.cpp
    src/ui/state-machine/api/MouseMove.cpp
    src/ui/state-machine/api/MouseUp.cpp
    src/ui/state-machine/api/Screenshot.cpp
    src/ui/state-machine/api/SimPause.cpp
    src/ui/state-machine/api/SimRun.cpp

    # UI network.
    src/ui/state-machine/network/CommandDeserializerJson.cpp
    src/ui/state-machine/network/MessageParser.cpp
    src/ui/state-machine/network/WebSocketClient.cpp
    src/ui/state-machine/network/WebSocketServer.cpp

    # UI components.
    src/ui/UiComponentManager.cpp
    src/ui/SimPlayground.cpp
    src/ui/rendering/CellRenderer.cpp
    src/ui/rendering/JuliaFractal.cpp
    src/ui/rendering/NeuralGridRenderer.cpp
    src/ui/controls/ControlPanel.cpp
    src/ui/controls/CoreControls.cpp
    src/ui/controls/PhysicsControls.cpp
    src/ui/controls/SandboxControls.cpp
    src/ui/ui_builders/LVGLBuilder.cpp
)
target_include_directories(sparkle-duck-ui-lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${zpp_bits_SOURCE_DIR})
target_link_libraries(sparkle-duck-ui-lib PUBLIC sparkle-duck-core sparkle-duck-server-lib lvgl_linux lvgl)

add_executable(sparkle-duck-ui
    src/ui/main.cpp
)
target_link_libraries(sparkle-duck-ui PRIVATE
    sparkle-duck-ui-lib
    -Wl,--start-group
    lvgl_linux
    lvgl
    -Wl,--end-group
    lvgl::thorvg
    datachannel-static
    m
    pthread
    ${PKG_CONFIG_LIB}
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
)
target_include_directories(sparkle-duck-ui PRIVATE ${CMAKE_SOURCE_DIR}/src ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/external/args ${zpp_bits_SOURCE_DIR})

# Enable warnings and treat them as errors for UI executable.
target_compile_options(sparkle-duck-ui PRIVATE -Wall -Wextra -Werror)

# Install the lvgl_linux library and its headers
install(DIRECTORY src/lib/
    DESTINATION include/lvgl
    FILES_MATCHING
    PATTERN "backends*" EXCLUDE
    PATTERN "*.h")

install(TARGETS lvgl_linux
    ARCHIVE DESTINATION lib
)

add_custom_target (run COMMAND ${EXECUTABLE_OUTPUT_PATH}/sparkle-duck-ui DEPENDS sparkle-duck-ui)

# Headless WebSocket server executable.
add_executable(sparkle-duck-server
    src/server/main.cpp
)
target_link_libraries(sparkle-duck-server PRIVATE sparkle-duck-server-lib m pthread)
target_include_directories(sparkle-duck-server PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/external/args ${zpp_bits_SOURCE_DIR})

# Enable warnings and treat them as errors for server executable.
target_compile_options(sparkle-duck-server PRIVATE -Wall -Wextra -Werror)

add_custom_target (run-server COMMAND ${EXECUTABLE_OUTPUT_PATH}/sparkle-duck-server DEPENDS sparkle-duck-server)

# CLI client executable.
add_executable(cli
    src/cli/main.cpp
    src/cli/BenchmarkRunner.cpp
    src/cli/IntegrationTest.cpp
    src/cli/RunAllRunner.cpp
    src/cli/SubprocessManager.cpp
    src/cli/WebSocketClient.cpp
)
target_link_libraries(cli PRIVATE sparkle-duck-core datachannel-static pthread)
target_include_directories(cli PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/external/args ${zpp_bits_SOURCE_DIR})

# Enable warnings and treat them as errors for CLI executable.
target_compile_options(cli PRIVATE -Wall -Wextra -Werror)

# Test executable.
add_executable(sparkle-duck-tests
    src/server/tests/StateIdle_test.cpp
    src/server/tests/StateSimRunning_test.cpp
    src/tests/ResultTest.cpp
    src/tests/TimersTest.cpp
    src/tests/Vector2d_test.cpp
    src/tests/Vector2i_test.cpp
)
target_link_libraries(sparkle-duck-tests
    PRIVATE
    sparkle-duck-server-lib
    GTest::gtest_main
    GTest::gmock_main
    m
    pthread
)
target_include_directories(sparkle-duck-tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${PKG_CONFIG_INC} ${zpp_bits_SOURCE_DIR})

# Enable warnings and treat them as errors for test executable.
target_compile_options(sparkle-duck-tests PRIVATE -Wall -Wextra -Werror)

# Enable testing
enable_testing()
add_test(NAME sparkle-duck-tests COMMAND sparkle-duck-tests)

