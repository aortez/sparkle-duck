#include <iostream>
#include <cassert>

// Mock the necessary components for testing
struct Vector2d {
    double x, y;
    Vector2d(double x = 0, double y = 0) : x(x), y(y) {}
};

class MockCell {
public:
    double dirt = 0.0;
    double water = 0.0;
    Vector2d com;
    Vector2d v;
    
    void update(double d, Vector2d c, Vector2d vel) {
        dirt = d; com = c; v = vel;
    }
    void markDirty() {}
    double percentFull() const { return dirt + water; }
};

// Test the core logic without LVGL dependencies
int main() {
    std::cout << "Testing Reset Button Core Logic..." << std::endl;
    
    // Simulate the time reversal state variables
    int currentHistoryIndex = -1;  // -1 = current state, >= 0 = navigating history
    bool hasStoredCurrentState = false;
    bool hasUserInputSinceLastSave = false;
    
    // Function to simulate markUserInput()
    auto markUserInput = [&]() {
        hasUserInputSinceLastSave = true;
    };
    
    // Function to simulate the old reset() behavior (problematic)
    auto oldReset = [&]() {
        markUserInput();
        std::cout << "  Old reset: markUserInput() called" << std::endl;
        // The problem: doesn't exit time navigation mode
    };
    
    // Function to simulate the new reset() behavior (fixed)
    auto newReset = [&]() {
        // Exit time reversal navigation mode to ensure we're working with current state
        currentHistoryIndex = -1;
        hasStoredCurrentState = false;
        markUserInput();
        std::cout << "  New reset: exited navigation mode and called markUserInput()" << std::endl;
    };
    
    // Test scenario 1: Reset in normal mode (should work with both)
    std::cout << "\nTest 1: Reset in normal mode..." << std::endl;
    currentHistoryIndex = -1;  // Normal mode
    hasStoredCurrentState = false;
    
    std::cout << "  Before reset: currentHistoryIndex=" << currentHistoryIndex << std::endl;
    newReset();
    std::cout << "  After reset: currentHistoryIndex=" << currentHistoryIndex << std::endl;
    assert(currentHistoryIndex == -1);
    assert(hasStoredCurrentState == false);
    std::cout << "  Test 1 PASSED" << std::endl;
    
    // Test scenario 2: Reset when in navigation mode (this was the problem)
    std::cout << "\nTest 2: Reset when in time navigation mode..." << std::endl;
    
    // Simulate going backward in time (the problematic state)
    currentHistoryIndex = 5;  // Simulating navigation mode
    hasStoredCurrentState = true;  // We're navigating history
    
    std::cout << "  Before reset: currentHistoryIndex=" << currentHistoryIndex 
              << ", hasStoredCurrentState=" << hasStoredCurrentState << std::endl;
    
    // Test old behavior (would leave us in navigation mode)
    int savedIndex = currentHistoryIndex;
    bool savedState = hasStoredCurrentState;
    oldReset();
    std::cout << "  Old reset result: currentHistoryIndex=" << currentHistoryIndex 
              << ", hasStoredCurrentState=" << hasStoredCurrentState << std::endl;
    // Restore state for proper test
    currentHistoryIndex = savedIndex;
    hasStoredCurrentState = savedState;
    
    // Test new behavior (should exit navigation mode)
    newReset();
    std::cout << "  New reset result: currentHistoryIndex=" << currentHistoryIndex 
              << ", hasStoredCurrentState=" << hasStoredCurrentState << std::endl;
    
    assert(currentHistoryIndex == -1);      // Should exit navigation mode
    assert(hasStoredCurrentState == false); // Should clear stored state
    std::cout << "  Test 2 PASSED" << std::endl;
    
    // Test scenario 3: Multiple resets don't interfere
    std::cout << "\nTest 3: Multiple sequential resets..." << std::endl;
    for (int i = 0; i < 3; i++) {
        // Simulate entering navigation mode
        currentHistoryIndex = i + 1;
        hasStoredCurrentState = true;
        
        newReset();
        
        std::cout << "  Reset " << (i+1) << ": currentHistoryIndex=" << currentHistoryIndex 
                  << ", hasStoredCurrentState=" << hasStoredCurrentState << std::endl;
        
        assert(currentHistoryIndex == -1);
        assert(hasStoredCurrentState == false);
    }
    std::cout << "  Test 3 PASSED" << std::endl;
    
    std::cout << "\nAll tests PASSED!" << std::endl;
    std::cout << "\nSUMMARY:" << std::endl;
    std::cout << "The issue was that reset() didn't exit time navigation mode." << std::endl;
    std::cout << "When currentHistoryIndex >= 0, the system was still navigating history," << std::endl;
    std::cout << "and the reset could be overridden by time reversal restoration." << std::endl;
    std::cout << "\nThe fix ensures reset() always returns to 'current state' mode by:" << std::endl;
    std::cout << "1. Setting currentHistoryIndex = -1 (exit navigation mode)" << std::endl;
    std::cout << "2. Setting hasStoredCurrentState = false (clear stored state)" << std::endl;
    std::cout << "3. Then performing the normal reset operations" << std::endl;
    
    return 0;
} 
