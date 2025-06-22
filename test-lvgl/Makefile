# Custom Makefile for Sparkle Duck project.

# Default target.
.DEFAULT_GOAL := help

# Build directory.
BUILD_DIR := build
BIN_DIR := bin

# Number of parallel jobs (use all available cores).
JOBS := $(shell nproc 2>/dev/null || echo 4)

# Test binary name.
TEST_BINARY := sparkle-duck-tests
MAIN_BINARY := sparkle-duck

.PHONY: all clean debug release build-tests test test-all visual-tests run format help

all: release

release:
	@echo "Building release version..."
	@rm -rf $(BUILD_DIR)
	@cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Release
	@$(MAKE) -C $(BUILD_DIR) -j$(JOBS)
	@echo "Running tests to verify build..."
	@./$(BUILD_DIR)/$(BIN_DIR)/$(TEST_BINARY)

debug:
	@echo "Building debug version..."
	@cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_C_FLAGS="-g -O0 -DLOG_DEBUG" \
		-DCMAKE_CXX_FLAGS="-g -O0 -DLOG_DEBUG"
	@$(MAKE) -C $(BUILD_DIR) -j$(JOBS)

build-tests: debug
	@echo "Test binary built successfully: $(BUILD_DIR)/$(BIN_DIR)/$(TEST_BINARY)"

test: build-tests
	@echo "Running tests..."
	@./$(BUILD_DIR)/$(BIN_DIR)/$(TEST_BINARY) $(ARGS)

test-all: test visual-tests

visual-tests:
	@echo "Running visual tests..."
	@if [ ! -f "./$(BUILD_DIR)/$(BIN_DIR)/$(TEST_BINARY)" ]; then \
		echo "Error: Test binary not found. Run 'make debug' first."; \
		exit 1; \
	fi
	@if [ -z "$$DISPLAY" ] && [ -z "$$WAYLAND_DISPLAY" ]; then \
		echo "Warning: No display detected. Visual tests may not work properly."; \
	fi
	@SPARKLE_DUCK_VISUAL_TESTS=1 ./$(BUILD_DIR)/$(BIN_DIR)/$(TEST_BINARY) $(ARGS)

run:
	@echo "Running sparkle-duck..."
	@./$(BUILD_DIR)/$(BIN_DIR)/$(MAIN_BINARY) -b wayland $(ARGS)

format:
	@echo "Formatting source code..."
	@clang-format --style=file -i src/*.cpp src/*.h

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR) lib
	@echo "Note: To clean in-source CMake artifacts, run: make clean-cmake"

clean-cmake:
	@echo "Cleaning in-source CMake artifacts..."
	@rm -rf CMakeCache.txt CMakeFiles cmake_install.cmake CTestTestfile.cmake Testing

help:
	@echo "Available targets:"
	@echo "  all          - Build release version (default)"
	@echo "  release      - Build optimized release version with test verification"
	@echo "  debug        - Build debug version with symbols and LOG_DEBUG"
	@echo "  build-tests  - Build test binary without running tests"
	@echo "  test         - Build debug and run unit tests"
	@echo "  test-all     - Run all tests (unit + visual)"
	@echo "  visual-tests - Run tests with visual output enabled"
	@echo "  run          - Run the main sparkle-duck executable"
	@echo "  format       - Format source code using clang-format"
	@echo "  clean        - Remove build directory and binaries"
	@echo "  clean-cmake  - Remove in-source CMake artifacts"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Pass arguments to test/run targets using ARGS:"
	@echo "  make test ARGS='--gtest_filter=WorldB*'"
	@echo "  make run ARGS='-b sdl -s 100'"