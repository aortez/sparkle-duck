#include "ui/rendering/CellRenderer.h"
#include <gtest/gtest.h>
#include <set>
#include <spdlog/spdlog.h>
#include <vector>

using namespace DirtSim::Ui;

// Helper to count pixels of a specific color in the buffer.
int countPixels(const std::vector<uint32_t>& buffer, uint32_t color)
{
    int count = 0;
    for (uint32_t pixel : buffer) {
        if (pixel == color) count++;
    }
    return count;
}

// Helper to check if a specific pixel is set.
bool pixelSet(const std::vector<uint32_t>& buffer, uint32_t width, int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= static_cast<int>(width)) return false;
    uint32_t height = buffer.size() / width;
    if (y >= static_cast<int>(height)) return false;
    return buffer[y * width + x] == color;
}

// Helper to collect all set pixel coordinates.
std::set<std::pair<int, int>> getSetPixels(
    const std::vector<uint32_t>& buffer, uint32_t width, uint32_t color)
{
    std::set<std::pair<int, int>> pixels;
    uint32_t height = buffer.size() / width;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            if (buffer[y * width + x] == color) {
                pixels.insert({ x, y });
            }
        }
    }
    return pixels;
}

class BresenhamLineTest : public ::testing::Test {
protected:
    static constexpr uint32_t CANVAS_WIDTH = 20;
    static constexpr uint32_t CANVAS_HEIGHT = 20;
    static constexpr uint32_t TEST_COLOR = 0xFF00FFFF; // Cyan.
    static constexpr uint32_t BG_COLOR = 0xFF000000;   // Black.

    std::vector<uint32_t> buffer_;

    void SetUp() override
    {
        // Initialize buffer with black background.
        buffer_.resize(CANVAS_WIDTH * CANVAS_HEIGHT, BG_COLOR);
    }

    void clearBuffer() { std::fill(buffer_.begin(), buffer_.end(), BG_COLOR); }
};

TEST_F(BresenhamLineTest, HorizontalLineLeftToRight)
{
    spdlog::info("Testing horizontal line left to right");

    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 5, 10, 15, 10, TEST_COLOR);

    // Should have 11 pixels (inclusive of endpoints).
    EXPECT_EQ(countPixels(buffer_, TEST_COLOR), 11);

    // Check all pixels on the line.
    for (int x = 5; x <= 15; ++x) {
        EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, x, 10, TEST_COLOR))
            << "Pixel at (" << x << ", 10) should be set";
    }
}

TEST_F(BresenhamLineTest, HorizontalLineRightToLeft)
{
    spdlog::info("Testing horizontal line right to left");

    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 15, 10, 5, 10, TEST_COLOR);

    EXPECT_EQ(countPixels(buffer_, TEST_COLOR), 11);
    for (int x = 5; x <= 15; ++x) {
        EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, x, 10, TEST_COLOR));
    }
}

TEST_F(BresenhamLineTest, VerticalLineTopToBottom)
{
    spdlog::info("Testing vertical line top to bottom");

    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 10, 3, 10, 17, TEST_COLOR);

    // Should have 15 pixels.
    EXPECT_EQ(countPixels(buffer_, TEST_COLOR), 15);

    for (int y = 3; y <= 17; ++y) {
        EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, 10, y, TEST_COLOR))
            << "Pixel at (10, " << y << ") should be set";
    }
}

TEST_F(BresenhamLineTest, VerticalLineBottomToTop)
{
    spdlog::info("Testing vertical line bottom to top");

    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 10, 17, 10, 3, TEST_COLOR);

    EXPECT_EQ(countPixels(buffer_, TEST_COLOR), 15);
    for (int y = 3; y <= 17; ++y) {
        EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, 10, y, TEST_COLOR));
    }
}

TEST_F(BresenhamLineTest, DiagonalLine45Degrees)
{
    spdlog::info("Testing 45-degree diagonal line");

    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 2, 2, 12, 12, TEST_COLOR);

    // Should have 11 pixels for a 45-degree line.
    EXPECT_EQ(countPixels(buffer_, TEST_COLOR), 11);

    // Check diagonal pixels.
    for (int i = 0; i <= 10; ++i) {
        EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, 2 + i, 2 + i, TEST_COLOR))
            << "Pixel at (" << (2 + i) << ", " << (2 + i) << ") should be set";
    }
}

TEST_F(BresenhamLineTest, DiagonalLineNegativeSlope)
{
    spdlog::info("Testing diagonal line with negative slope");

    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 2, 12, 12, 2, TEST_COLOR);

    EXPECT_EQ(countPixels(buffer_, TEST_COLOR), 11);

    for (int i = 0; i <= 10; ++i) {
        EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, 2 + i, 12 - i, TEST_COLOR))
            << "Pixel at (" << (2 + i) << ", " << (12 - i) << ") should be set";
    }
}

TEST_F(BresenhamLineTest, SinglePoint)
{
    spdlog::info("Testing single point (start == end)");

    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 10, 10, 10, 10, TEST_COLOR);

    EXPECT_EQ(countPixels(buffer_, TEST_COLOR), 1);
    EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, 10, 10, TEST_COLOR));
}

TEST_F(BresenhamLineTest, SteepLine)
{
    spdlog::info("Testing steep line (dy > dx)");

    // Line from (5, 2) to (8, 15) - steep, more vertical than horizontal.
    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 5, 2, 8, 15, TEST_COLOR);

    auto pixels = getSetPixels(buffer_, CANVAS_WIDTH, TEST_COLOR);

    // Endpoints should be set.
    EXPECT_TRUE(pixels.count({ 5, 2 }) > 0) << "Start point should be set";
    EXPECT_TRUE(pixels.count({ 8, 15 }) > 0) << "End point should be set";

    // Line should have more pixels than just endpoints (steep lines step through y).
    EXPECT_GE(countPixels(buffer_, TEST_COLOR), 14);
}

TEST_F(BresenhamLineTest, ShallowLine)
{
    spdlog::info("Testing shallow line (dx > dy)");

    // Line from (2, 5) to (15, 8) - shallow, more horizontal than vertical.
    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 2, 5, 15, 8, TEST_COLOR);

    auto pixels = getSetPixels(buffer_, CANVAS_WIDTH, TEST_COLOR);

    // Endpoints should be set.
    EXPECT_TRUE(pixels.count({ 2, 5 }) > 0) << "Start point should be set";
    EXPECT_TRUE(pixels.count({ 15, 8 }) > 0) << "End point should be set";

    // Shallow lines should have ~dx pixels.
    EXPECT_GE(countPixels(buffer_, TEST_COLOR), 14);
}

TEST_F(BresenhamLineTest, BoundsCheckingPartiallyOutOfBounds)
{
    spdlog::info("Testing bounds checking with line partially out of bounds");

    // Line that starts in bounds but goes out of bounds.
    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 15, 10, 25, 10, TEST_COLOR);

    // Should only draw pixels that are within bounds (x < 20).
    EXPECT_EQ(countPixels(buffer_, TEST_COLOR), 5); // x=15,16,17,18,19

    for (int x = 15; x < 20; ++x) {
        EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, x, 10, TEST_COLOR))
            << "Pixel at (" << x << ", 10) should be set";
    }
}

TEST_F(BresenhamLineTest, BoundsCheckingCompletelyOutOfBounds)
{
    spdlog::info("Testing bounds checking with line completely out of bounds");

    // Line completely outside the canvas.
    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, 25, 25, 30, 30, TEST_COLOR);

    // No pixels should be drawn.
    EXPECT_EQ(countPixels(buffer_, TEST_COLOR), 0);
}

TEST_F(BresenhamLineTest, BoundsCheckingNegativeCoordinates)
{
    spdlog::info("Testing bounds checking with negative coordinates");

    // Line starting from negative coordinates going into bounds.
    drawLineBresenham(buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, -5, 10, 5, 10, TEST_COLOR);

    // Should only draw pixels within bounds (x >= 0).
    EXPECT_EQ(countPixels(buffer_, TEST_COLOR), 6); // x=0,1,2,3,4,5

    for (int x = 0; x <= 5; ++x) {
        EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, x, 10, TEST_COLOR))
            << "Pixel at (" << x << ", 10) should be set";
    }
}

TEST_F(BresenhamLineTest, AllQuadrants)
{
    spdlog::info("Testing lines in all four quadrants from center");

    int cx = 10, cy = 10;

    // Quadrant 1: right-down.
    clearBuffer();
    drawLineBresenham(
        buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, cx, cy, cx + 5, cy + 5, TEST_COLOR);
    EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, cx, cy, TEST_COLOR));
    EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, cx + 5, cy + 5, TEST_COLOR));

    // Quadrant 2: left-down.
    clearBuffer();
    drawLineBresenham(
        buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, cx, cy, cx - 5, cy + 5, TEST_COLOR);
    EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, cx, cy, TEST_COLOR));
    EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, cx - 5, cy + 5, TEST_COLOR));

    // Quadrant 3: left-up.
    clearBuffer();
    drawLineBresenham(
        buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, cx, cy, cx - 5, cy - 5, TEST_COLOR);
    EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, cx, cy, TEST_COLOR));
    EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, cx - 5, cy - 5, TEST_COLOR));

    // Quadrant 4: right-up.
    clearBuffer();
    drawLineBresenham(
        buffer_.data(), CANVAS_WIDTH, CANVAS_HEIGHT, cx, cy, cx + 5, cy - 5, TEST_COLOR);
    EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, cx, cy, TEST_COLOR));
    EXPECT_TRUE(pixelSet(buffer_, CANVAS_WIDTH, cx + 5, cy - 5, TEST_COLOR));
}
