#include "../Cell.h"
#include "../World.h"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

TEST(WorldSizeTest, MeasureSizes)
{
    spdlog::info("sizeof(Cell): {} bytes", sizeof(Cell));
    spdlog::info("sizeof(World): {} bytes (object overhead)", sizeof(World));

    // Create worlds of different sizes.
    World world_50x50(50, 50);
    World world_100x100(100, 100);
    World world_200x150(200, 150);

    // Estimate total memory.
    size_t cells_50 = sizeof(Cell) * 50 * 50;
    size_t cells_100 = sizeof(Cell) * 100 * 100;
    size_t cells_200x150 = sizeof(Cell) * 200 * 150;

    spdlog::info("50x50 World:");
    spdlog::info(
        "  Cells: {} × {} = {} bytes ({:.2f} KB)",
        50 * 50,
        sizeof(Cell),
        cells_50,
        cells_50 / 1024.0);
    spdlog::info("  Approx total: {:.2f} KB", (cells_50 + sizeof(World)) / 1024.0);

    spdlog::info("100x100 World:");
    spdlog::info(
        "  Cells: {} × {} = {} bytes ({:.2f} KB)",
        100 * 100,
        sizeof(Cell),
        cells_100,
        cells_100 / 1024.0);
    spdlog::info("  Approx total: {:.2f} KB", (cells_100 + sizeof(World)) / 1024.0);

    spdlog::info("200x150 World:");
    spdlog::info(
        "  Cells: {} × {} = {} bytes ({:.2f} KB)",
        200 * 150,
        sizeof(Cell),
        cells_200x150,
        cells_200x150 / 1024.0);
    spdlog::info("  Approx total: {:.2f} KB", (cells_200x150 + sizeof(World)) / 1024.0);

    spdlog::info("At 60 FPS (worst case, copying every frame):");
    spdlog::info("  50x50: {:.2f} MB/sec", cells_50 * 60 / 1024.0 / 1024.0);
    spdlog::info("  100x100: {:.2f} MB/sec", cells_100 * 60 / 1024.0 / 1024.0);
    spdlog::info("  200x150: {:.2f} MB/sec", cells_200x150 * 60 / 1024.0 / 1024.0);
}
