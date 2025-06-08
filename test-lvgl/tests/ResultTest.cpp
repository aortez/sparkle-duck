#include "core/Result.h"
#include <cassert>
#include <iostream>

int main() {
    // Test 1: Default constructor creates error state
    {
        Result<int, std::string> result;
        assert(!result.isValue());
        assert(result.isError());
        try {
            result.value();
            assert(false); // Should not reach here
        } catch (...) {
            // Expected assertion failure
        }
        std::string error = result.error();
        assert(error == std::string()); // Default-constructed error
    }

    // Test 2: Success with default value
    {
        Result<int, std::string> result = Result<int, std::string>::okay();
        assert(result.isValue());
        assert(!result.isError());
        int value = result.value();
        assert(value == 0); // Default-constructed int
        try {
            result.error();
            assert(false); // Should not reach here
        } catch (...) {
            // Expected assertion failure
        }
    }

    // Test 3: Success with specific value
    {
        Result<int, std::string> result = Result<int, std::string>::okay(42);
        assert(result.isValue());
        assert(result.value() == 42);
    }

    // Test 4: Error with default value
    {
        Result<int, std::string> result = Result<int, std::string>::error();
        assert(result.isError());
        assert(result.error() == std::string());
    }

    // Test 5: Error with specific value
    {
        Result<int, std::string> result = Result<int, std::string>::error("Test error");
        assert(result.isError());
        assert(result.error() == "Test error");
    }

    std::cout << "All tests passed!" << std::endl;
    return 0;
}