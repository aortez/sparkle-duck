#pragma once

#include "CrashDumpHandler.h"
#include <cassert>

/**
 * SparkleAssert.h - Custom assertion macros with crash dump integration
 * 
 * Provides enhanced assertion macros that automatically generate JSON crash dumps
 * of the complete world state when assertions fail.
 */

/**
 * SPARKLE_ASSERT - Enhanced assertion with crash dump
 * 
 * Usage: SPARKLE_ASSERT(condition, "Description of what failed");
 * 
 * On failure:
 * 1. Triggers crash dump with complete world state in JSON format
 * 2. Logs detailed failure information
 * 3. Calls standard assert() to terminate (in debug builds)
 */
#define SPARKLE_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            CrashDumpHandler::onAssertionFailure(#condition, __FILE__, __LINE__, message); \
            assert(condition); \
        } \
    } while (0)

/**
 * SPARKLE_ASSERT_MSG - Assertion with formatted message
 * 
 * Usage: SPARKLE_ASSERT_MSG(x > 0, "Value must be positive, got: %d", x);
 */
#define SPARKLE_ASSERT_MSG(condition, format, ...) \
    do { \
        if (!(condition)) { \
            char _sparkle_assert_buffer[1024]; \
            snprintf(_sparkle_assert_buffer, sizeof(_sparkle_assert_buffer), format, ##__VA_ARGS__); \
            CrashDumpHandler::onAssertionFailure(#condition, __FILE__, __LINE__, _sparkle_assert_buffer); \
            assert(condition); \
        } \
    } while (0)

/**
 * SPARKLE_VERIFY - Non-fatal assertion that dumps but continues
 * 
 * Usage: SPARKLE_VERIFY(condition, "Warning: unexpected condition");
 * 
 * On failure:
 * 1. Triggers crash dump
 * 2. Logs error but continues execution
 * 3. Does NOT call assert() - program continues
 */
#define SPARKLE_VERIFY(condition, message) \
    do { \
        if (!(condition)) { \
            CrashDumpHandler::onAssertionFailure(#condition, __FILE__, __LINE__, message); \
        } \
    } while (0)

/**
 * SPARKLE_DUMP - Manual crash dump trigger
 * 
 * Usage: SPARKLE_DUMP("Manual dump at checkpoint");
 * 
 * Useful for debugging or capturing state at specific points.
 */
#define SPARKLE_DUMP(reason) \
    CrashDumpHandler::dumpWorldState(reason)

/**
 * Legacy compatibility - map old ASSERT to new system
 * 
 * Gradually replace existing ASSERT usage with SPARKLE_ASSERT.
 * This ensures existing code gets crash dump benefits immediately.
 */
#ifdef ASSERT
#undef ASSERT
#endif

#define ASSERT(condition, message, ...) \
    SPARKLE_ASSERT_MSG(condition, message, ##__VA_ARGS__)

/**
 * Debug-only assertions that are compiled out in release builds
 */
#ifdef NDEBUG
    #define SPARKLE_DEBUG_ASSERT(condition, message) ((void)0)
    #define SPARKLE_DEBUG_VERIFY(condition, message) ((void)0)
#else
    #define SPARKLE_DEBUG_ASSERT(condition, message) SPARKLE_ASSERT(condition, message)
    #define SPARKLE_DEBUG_VERIFY(condition, message) SPARKLE_VERIFY(condition, message)
#endif