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

set(CONF_PATH "${PROJECT_SOURCE_DIR}/lv_conf.h")

foreach(BACKEND_NAME "SDL" "LINUX_DRM" "LINUX_FBDEV" "X11" "WAYLAND" "OPENGLES")

    execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    COMMAND "scripts/backend_conf.sh" ${BACKEND_NAME} ${CONF_PATH} OUTPUT_VARIABLE IS_BACKEND_ENABLED)
    set("LV_USE_${BACKEND_NAME}" ${IS_BACKEND_ENABLED})

endforeach()

# Uncomment if the program needs debugging
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")

set(CMAKE_C_STANDARD 99) # LVGL officially supports C99 and above
set(CMAKE_CXX_STANDARD 17) #C17
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

    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/sdl.cpp)
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
    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/wayland.cpp wl_protocols/wayland_xdg_shell.c)

endif()

if (LV_USE_X11)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED x11)

    message("Including X11 support")

    list(APPEND PKG_CONFIG_INC ${X11_INCLUDE_DIRS})
    list(APPEND PKG_CONFIG_LIB ${X11_LIBRARIES})
    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/x11.cpp)

endif()

if (LV_USE_LINUX_FBDEV)

    # FBDEV has no dependencies
    message("Including FBDEV support")
    list(APPEND LV_LINUX_BACKEND_SRC src/lib/display_backends/fbdev.cpp)

endif()

foreach(arg ${PKG_CONFIG_LIB})
    string(APPEND LVGL_PKG_CONFIG_EXT_LIB " -l${arg}")
endforeach()

string(APPEND LVGL_PKG_CONFIG_LIB "-llvgl_linux")

file(GLOB LV_LINUX_SRC src/lib/*.c src/lib/*.cpp)
set(LV_LINUX_INC src/lib)

add_subdirectory(./lvgl ${CMAKE_BINARY_DIR}/lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/lib ${PKG_CONFIG_INC})

add_subdirectory(./spdlog ${CMAKE_BINARY_DIR}/spdlog)
add_library(lvgl_linux STATIC ${LV_LINUX_SRC} ${LV_LINUX_BACKEND_SRC})
target_include_directories(lvgl_linux PRIVATE ${LV_LINUX_INC} ${PROJECT_SOURCE_DIR})

add_executable(sparkle-duck 
    src/main.cpp 
    src/Cell.cpp
    src/Vector2d.cpp
    src/Vector2i.cpp
    src/World.cpp 
    src/Timers.cpp
    src/WorldSetup.cpp
    src/SimulatorUI.cpp
    src/SimulationManager.cpp
    src/MaterialType.cpp
    src/CellB.cpp
    src/WorldB.cpp
    src/WorldInterface.cpp
    src/WorldBCohesionCalculator.cpp
    src/WorldBSupportCalculator.cpp
    src/WorldDiagramGenerator.cpp
    src/WorldFactory.cpp
    src/MaterialPicker.cpp
    src/WorldState.cpp
    src/CrashDumpHandler.cpp
    src/WorldInterpolationTool.cpp
    ${LV_LINUX_SRC} 
    ${LV_LINUX_BACKEND_SRC})
target_link_libraries(sparkle-duck lvgl_linux lvgl lvgl::examples lvgl::demos lvgl::thorvg spdlog::spdlog m pthread ${PKG_CONFIG_LIB} ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})
target_include_directories(sparkle-duck PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})

# Enable warnings and treat them as errors for main executable
target_compile_options(sparkle-duck PRIVATE -Wall -Wextra -Werror)

# Install the lvgl_linux library and its headers
install(DIRECTORY src/lib/
    DESTINATION include/lvgl
    FILES_MATCHING
    PATTERN "backends*" EXCLUDE
    PATTERN "*.h")

install(TARGETS lvgl_linux
    ARCHIVE DESTINATION lib
)

add_custom_target (run COMMAND ${EXECUTABLE_OUTPUT_PATH}/sparkle-duck DEPENDS sparkle-duck)


# Add test executable
add_executable(sparkle-duck-tests
    src/tests/Vector2d_test.cpp
    tests/Vector2dJSON_test.cpp
    tests/MaterialTypeJSON_test.cpp
    tests/WorldStateJSON_test.cpp
    tests/CrashDumpHandler_test.cpp
    src/tests/Vector2i_test.cpp
    src/tests/WorldVisual_test.cpp
    src/tests/WorldBVisual_test.cpp
    src/tests/InterfaceCompatibility_test.cpp
    src/tests/WaterPressure180_test.cpp
    src/tests/PressureSystemVisual_test.cpp
    src/tests/PressureSystem_test.cpp
    src/tests/DensityMechanics_test.cpp
    src/tests/CollisionSystem_test.cpp
    src/tests/ForceCalculation_test.cpp
    src/tests/ForceInfluencedMovement_test.cpp
    src/tests/ForceDebug_test.cpp
    src/tests/ForcePhysicsIntegration_test.cpp
    src/tests/HorizontalLineStability_test.cpp
    src/tests/DistanceToSupport_test.cpp
    src/tests/COMCohesionForce_test.cpp
    src/tests/PressureDynamic_test.cpp
    src/tests/PressureHydrostatic_test.cpp
    src/tests/AsciiDiagram_test.cpp
    src/tests/TestUI.cpp
    src/tests/visual_test_runner.cpp
    tests/ResultTest.cpp
    test/TimersTest.cpp
    simple_reset_test.cpp
    src/Vector2d.cpp
    src/Vector2i.cpp
    src/World.cpp
    src/Cell.cpp
    src/Timers.cpp
    src/WorldSetup.cpp
    src/SimulatorUI.cpp
    src/SimulationManager.cpp
    src/MaterialType.cpp
    src/CellB.cpp
    src/WorldB.cpp
    src/WorldInterface.cpp
    src/WorldBCohesionCalculator.cpp
    src/WorldBSupportCalculator.cpp
    src/WorldDiagramGenerator.cpp
    src/WorldFactory.cpp
    src/MaterialPicker.cpp
    src/WorldState.cpp
    src/CrashDumpHandler.cpp
    src/WorldInterpolationTool.cpp
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
