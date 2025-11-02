#pragma once

#include <string>

// Forward declarations.
class WorldInterface;

/**
 * CrashDumpHandler - Captures complete world state on assertion failures.
 *
 * Provides JSON-based world state dumps for debugging crashes and assertion failures.
 * Hooks into the existing ASSERT macro to automatically capture simulation state.
 */
class CrashDumpHandler {
public:
    /**
     * Install the crash dump handler globally.
     * Should be called once during application startup.
     */
    static void install(WorldInterface* world);

    /**
     * Remove the crash dump handler.
     * Called during application shutdown.
     */
    static void uninstall();

    /**
     * Manually trigger a world state dump.
     * Useful for debugging or testing.
     */
    static void dumpWorldState(const char* reason = "Manual dump");

    /**
     * Set the output directory for crash dump files.
     * Default is current working directory.
     */
    static void setDumpDirectory(const std::string& directory);

    /**
     * Crash handler function called on assertion failure.
     * Internal use - called by modified ASSERT macro.
     */
    static void onAssertionFailure(
        const char* condition, const char* file, int line, const char* message);

private:
    static WorldInterface* world_;
    static std::string dump_directory_;
    static bool installed_;

    // Internal helper methods
    static std::string generateDumpFilename(const char* reason);
    static void writeWorldStateToFile(
        const std::string& filename,
        const char* reason,
        const char* condition = nullptr,
        const char* file = nullptr,
        int line = 0,
        const char* message = nullptr);
    static void logDumpSummary(const std::string& filename, const char* reason);
};