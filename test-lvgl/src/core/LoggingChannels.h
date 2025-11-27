#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace DirtSim {

/**
 * @brief Centralized logging channel management for fine-grained log filtering.
 *
 * Provides named loggers for different subsystems to enable focused debugging
 * without log flooding from unrelated components.
 */
class LoggingChannels {
public:
    /**
     * @brief Initialize the logging system with shared sinks.
     * @param consoleLevel Default log level for console output
     * @param fileLevel Default log level for file output
     */
    static void initialize(
        spdlog::level::level_enum consoleLevel = spdlog::level::info,
        spdlog::level::level_enum fileLevel = spdlog::level::debug);

    /**
     * @brief Initialize the logging system from a JSON config file.
     * Looks for <configPath>.local first, falls back to <configPath> if not found.
     * @param configPath Path to the JSON config file (default: "logging-config.json")
     * @return true if config was loaded successfully, false if using defaults
     */
    static bool initializeFromConfig(const std::string& configPath = "logging-config.json");

    /**
     * @brief Get a specific channel logger.
     * @param channel Name of the channel
     * @return Logger for the channel, or default logger if channel doesn't exist
     */
    static std::shared_ptr<spdlog::logger> get(const std::string& channel);

    /**
     * @brief Configure channels from a specification string.
     * @param spec Format: "channel:level,channel2:level2" or "*:level" for all
     * Examples:
     *   "swap:trace,physics:debug" - Set swap to trace, physics to debug
     *   "*:error" - Set all channels to error
     *   "*:off,swap:trace" - Disable all except swap at trace level
     */
    static void configureFromString(const std::string& spec);

    /**
     * @brief Set the log level for a specific channel.
     * @param channel Name of the channel
     * @param level Log level to set
     */
    static void setChannelLevel(const std::string& channel, spdlog::level::level_enum level);

    // Convenience accessors for common channels.
    static std::shared_ptr<spdlog::logger> physics() { return get("physics"); }
    static std::shared_ptr<spdlog::logger> swap() { return get("swap"); }
    static std::shared_ptr<spdlog::logger> cohesion() { return get("cohesion"); }
    static std::shared_ptr<spdlog::logger> pressure() { return get("pressure"); }
    static std::shared_ptr<spdlog::logger> collision() { return get("collision"); }
    static std::shared_ptr<spdlog::logger> friction() { return get("friction"); }
    static std::shared_ptr<spdlog::logger> support() { return get("support"); }
    static std::shared_ptr<spdlog::logger> viscosity() { return get("viscosity"); }
    static std::shared_ptr<spdlog::logger> ui() { return get("ui"); }
    static std::shared_ptr<spdlog::logger> network() { return get("network"); }
    static std::shared_ptr<spdlog::logger> state() { return get("state"); }
    static std::shared_ptr<spdlog::logger> scenario() { return get("scenario"); }
    static std::shared_ptr<spdlog::logger> tree() { return get("tree"); }

private:
    /**
     * @brief Create a logger with the given name and sinks.
     */
    static void createLogger(
        const std::string& name,
        const std::vector<spdlog::sink_ptr>& sinks,
        spdlog::level::level_enum level);

    /**
     * @brief Parse a log level string to enum.
     */
    static spdlog::level::level_enum parseLevelString(const std::string& levelStr);

    /**
     * @brief Load JSON config from file, with .local override support.
     * Creates default config if file doesn't exist.
     * Exits on error if file exists but cannot be read.
     */
    static nlohmann::json loadConfigFile(const std::string& configPath);

    /**
     * @brief Create default config file at the given path.
     */
    static bool createDefaultConfigFile(const std::string& path);

    /**
     * @brief Apply configuration from JSON object.
     */
    static void applyConfig(const nlohmann::json& config);

    /**
     * @brief Create specialized sinks from config.
     */
    static void createSpecializedSinks(const nlohmann::json& specializedConfig);

    static bool initialized_;
    static std::vector<spdlog::sink_ptr> sharedSinks_;
};

} // namespace DirtSim