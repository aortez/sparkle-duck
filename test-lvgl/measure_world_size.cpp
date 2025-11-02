#include "src/Cell.h"
#include "src/World.h"
#include <iostream>

int main()
{
    std::cout << "sizeof(Cell): " << sizeof(Cell) << " bytes" << std::endl;
    std::cout << "sizeof(World): " << sizeof(World) << " bytes (object overhead)" << std::endl;
    std::cout << std::endl;

    // Create worlds of different sizes.
    World world_50x50(50, 50);
    World world_100x100(100, 100);
    World world_200x150(200, 150);

    // Estimate total memory.
    size_t cells_50 = sizeof(Cell) * 50 * 50;
    size_t cells_100 = sizeof(Cell) * 100 * 100;
    size_t cells_200x150 = sizeof(Cell) * 200 * 150;

    std::cout << "50x50 World:" << std::endl;
    std::cout << "  Cells: " << (50 * 50) << " × " << sizeof(Cell) << " = " << cells_50
              << " bytes (" << (cells_50 / 1024.0) << " KB)" << std::endl;
    std::cout << "  Approx total: " << (cells_50 + sizeof(World)) / 1024.0 << " KB" << std::endl;
    std::cout << std::endl;

    std::cout << "100x100 World:" << std::endl;
    std::cout << "  Cells: " << (100 * 100) << " × " << sizeof(Cell) << " = " << cells_100
              << " bytes (" << (cells_100 / 1024.0) << " KB)" << std::endl;
    std::cout << "  Approx total: " << (cells_100 + sizeof(World)) / 1024.0 << " KB" << std::endl;
    std::cout << std::endl;

    std::cout << "200x150 World (design doc example):" << std::endl;
    std::cout << "  Cells: " << (200 * 150) << " × " << sizeof(Cell) << " = " << cells_200x150
              << " bytes (" << (cells_200x150 / 1024.0) << " KB)" << std::endl;
    std::cout << "  Approx total: " << (cells_200x150 + sizeof(World)) / 1024.0 << " KB"
              << std::endl;
    std::cout << std::endl;

    std::cout << "At 60 FPS (worst case, no dirty flags):" << std::endl;
    std::cout << "  50x50: " << (cells_50 * 60 / 1024.0 / 1024.0) << " MB/sec" << std::endl;
    std::cout << "  100x100: " << (cells_100 * 60 / 1024.0 / 1024.0) << " MB/sec" << std::endl;
    std::cout << "  200x150: " << (cells_200x150 * 60 / 1024.0 / 1024.0) << " MB/sec" << std::endl;

    return 0;
}
