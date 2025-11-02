cmake_minimum_required(VERSION 3.10)
project(lvgl)

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

add_subdirectory(./lvgl ${CMAKE_BINARY_DIR}/lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/lib ${PKG_CONFIG_INC})

add_subdirectory(./spdlog ${CMAKE_BINARY_DIR}/spdlog)

# Add args header-only library
set(ARGS_BUILD_EXAMPLE OFF CACHE BOOL "Build examples" FORCE)
set(ARGS_BUILD_UNITTESTS OFF CACHE BOOL "Build unit tests" FORCE)
add_subdirectory(./external/args ${CMAKE_BINARY_DIR}/args)

add_library(lvgl_linux STATIC ${LV_LINUX_SRC} ${LV_LINUX_BACKEND_SRC})
target_include_directories(lvgl_linux PRIVATE ${LV_LINUX_INC} ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/ui/lib)
target_link_libraries(lvgl_linux PRIVATE spdlog::spdlog)

add_executable(sparkle-duck-ui
    # UI application (client only).
    src/ui/main.cpp
    src/ui/StateMachine.cpp
    src/ui/EventProcessor.cpp
    src/ui/states/Startup.cpp
    src/ui/states/MainMenu.cpp
    src/ui/states/SimRunning.cpp
    src/ui/states/Paused.cpp
    src/ui/states/Shutdown.cpp
    src/ui/SimulatorUI.cpp
    src/ui/UIUpdateConsumer.cpp
    src/ui/UIManager.cpp
    src/ui/MaterialPicker.cpp
    src/ui/ui_builders/LVGLBuilder.cpp
    src/ui/ui_builders/LVGLEventBuilder.cpp

    # Core components (rendering only, no physics).
    src/core/Cell.cpp
    src/core/Vector2d.cpp
    src/core/Vector2i.cpp
    src/core/World.cpp
    src/core/MaterialType.cpp
    src/core/WorldDiagramGeneratorEmoji.cpp
    src/core/CrashDumpHandler.cpp
    src/core/StateMachineBase.cpp

    # LVGL backend.
    ${LV_LINUX_SRC}
    ${LV_LINUX_BACKEND_SRC})
target_link_libraries(sparkle-duck-ui lvgl_linux lvgl lvgl::thorvg spdlog::spdlog datachannel-static m pthread ${PKG_CONFIG_LIB} ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})
target_include_directories(sparkle-duck-ui PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/external/args)

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
    # Server application.
    src/server/main_server.cpp
    src/server/StateMachine.cpp
    src/server/EventProcessor.cpp
    src/server/states/Startup.cpp
    src/server/states/MainMenu.cpp
    src/server/states/SimRunning.cpp
    src/server/states/SimPaused.cpp
    src/server/states/Config.cpp
    src/server/states/Shutdown.cpp
    src/server/scenarios/ScenarioWorldEventGenerator.cpp
    src/server/scenarios/ScenarioRegistry.cpp
    src/server/scenarios/scenarios/EmptyScenario.cpp
    src/server/scenarios/scenarios/SandboxScenario.cpp
    src/server/scenarios/scenarios/RainingScenario.cpp
    src/server/scenarios/scenarios/DamBreakScenario.cpp
    src/server/scenarios/scenarios/WaterEqualizationScenario.cpp
    src/server/scenarios/scenarios/FallingDirtScenario.cpp
    src/server/network/CommandDeserializerJson.cpp
    src/server/network/WebSocketServer.cpp

    # Core components.
    src/core/Cell.cpp
    src/core/Vector2d.cpp
    src/core/Vector2i.cpp
    src/core/World.cpp
    src/core/Timers.cpp
    src/core/WorldEventGenerator.cpp
    src/core/MaterialType.cpp
    src/core/WorldCalculatorBase.cpp
    src/core/WorldCohesionCalculator.cpp
    src/core/WorldSupportCalculator.cpp
    src/core/WorldPressureCalculator.cpp
    src/core/WorldAirResistanceCalculator.cpp
    src/core/WorldCollisionCalculator.cpp
    src/core/WorldAdhesionCalculator.cpp
    src/core/WorldFrictionCalculator.cpp
    src/core/CrashDumpHandler.cpp
    src/core/WorldInterpolationTool.cpp
    src/core/WorldDiagramGeneratorEmoji.cpp
    src/core/StateMachineBase.cpp)
target_link_libraries(sparkle-duck-server nlohmann_json::nlohmann_json spdlog::spdlog datachannel-static m pthread)
target_include_directories(sparkle-duck-server PRIVATE ${CMAKE_SOURCE_DIR}/external/args)

# Enable warnings and treat them as errors for server executable.
target_compile_options(sparkle-duck-server PRIVATE -Wall -Wextra -Werror)

add_custom_target (run-server COMMAND ${EXECUTABLE_OUTPUT_PATH}/sparkle-duck-server DEPENDS sparkle-duck-server)


# Add test executable.
# Note: Removed obsolete physics, UI, and state machine tests.
# Long-term TODO: Re-implement physics tests for new architecture.
add_executable(sparkle-duck-tests
    # Test files.
    src/tests/CellJSON_test.cpp
    src/tests/MaterialTypeJSON_test.cpp
    src/tests/Vector2dJSON_test.cpp
    src/tests/WorldJSON_test.cpp
    src/tests/CrashDumpHandler_test.cpp
    src/tests/ResultTest.cpp
    src/tests/TimersTest.cpp
    src/tests/Vector2d_test.cpp
    src/tests/Vector2i_test.cpp
    src/tests/TestUI.cpp
    src/tests/UnifiedSimLoopExample_test.cpp
    src/tests/visual_test_runner.cpp

    # Core components.
    src/core/Cell.cpp
    src/core/Vector2d.cpp
    src/core/Vector2i.cpp
    src/core/World.cpp
    src/core/Timers.cpp
    src/core/WorldEventGenerator.cpp
    src/core/MaterialType.cpp
    src/core/WorldCalculatorBase.cpp
    src/core/WorldCohesionCalculator.cpp
    src/core/WorldSupportCalculator.cpp
    src/core/WorldPressureCalculator.cpp
    src/core/WorldAirResistanceCalculator.cpp
    src/core/WorldCollisionCalculator.cpp
    src/core/WorldAdhesionCalculator.cpp
    src/core/WorldFrictionCalculator.cpp
    src/core/WorldDiagramGeneratorEmoji.cpp
    src/core/CrashDumpHandler.cpp
    src/core/WorldInterpolationTool.cpp
    src/core/StateMachineBase.cpp

    # Server components (for test infrastructure).
    src/server/StateMachine.cpp
    src/server/EventProcessor.cpp
    src/server/states/Startup.cpp
    src/server/states/MainMenu.cpp
    src/server/states/SimRunning.cpp
    src/server/states/SimPaused.cpp
    src/server/states/Config.cpp
    src/server/states/Shutdown.cpp
    src/server/scenarios/ScenarioWorldEventGenerator.cpp
    src/server/scenarios/ScenarioRegistry.cpp
    src/server/scenarios/scenarios/EmptyScenario.cpp
    src/server/scenarios/scenarios/SandboxScenario.cpp
    src/server/scenarios/scenarios/RainingScenario.cpp
    src/server/scenarios/scenarios/DamBreakScenario.cpp
    src/server/scenarios/scenarios/WaterEqualizationScenario.cpp
    src/server/scenarios/scenarios/FallingDirtScenario.cpp

    # UI components (for visual tests).
    src/ui/SimulatorUI.cpp
    src/ui/UIUpdateConsumer.cpp
    src/ui/UIManager.cpp
    src/ui/MaterialPicker.cpp
    src/ui/ui_builders/LVGLBuilder.cpp
    src/ui/ui_builders/LVGLEventBuilder.cpp

    # LVGL backend.
    ${LV_LINUX_SRC}
    ${LV_LINUX_BACKEND_SRC}
)
target_link_libraries(sparkle-duck-tests
    PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    lvgl_linux
    lvgl
    lvgl::thorvg
    spdlog::spdlog
    m
    pthread
    ${PKG_CONFIG_LIB}
)
target_include_directories(sparkle-duck-tests PRIVATE ${PKG_CONFIG_INC})

# Enable warnings and treat them as errors for test executable
target_compile_options(sparkle-duck-tests PRIVATE -Wall -Wextra -Werror)

# Enable testing
enable_testing()
add_test(NAME sparkle-duck-tests COMMAND sparkle-duck-tests)

