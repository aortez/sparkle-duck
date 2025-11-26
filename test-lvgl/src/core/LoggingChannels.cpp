#include "LoggingChannels.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace DirtSim {

// Static member initialization.
bool LoggingChannels::initialized_ = false;
std::vector<spdlog::sink_ptr> LoggingChannels::sharedSinks_;

void LoggingChannels::initialize(
    spdlog::level::level_enum consoleLevel, spdlog::level::level_enum fileLevel)
{
    if (initialized_) {
        spdlog::warn("LoggingChannels already initialized, skipping re-initialization");
        return;
    }

    // Create shared sinks.
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(consoleLevel);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("sparkle-duck.log", true);
    file_sink->set_level(fileLevel);

    sharedSinks_ = { console_sink, file_sink };

    // Set pattern to include channel name.
    spdlog::set_pattern("[%H:%M:%S.%e] [%n] [%^%l%$] %v");

    // Create channel-specific loggers with TRACE level (can be filtered later).
    // Physics channels.
    createLogger("physics", sharedSinks_, spdlog::level::trace);
    createLogger("swap", sharedSinks_, spdlog::level::trace);
    createLogger("cohesion", sharedSinks_, spdlog::level::trace);
    createLogger("pressure", sharedSinks_, spdlog::level::trace);
    createLogger("collision", sharedSinks_, spdlog::level::trace);
    createLogger("friction", sharedSinks_, spdlog::level::trace);
    createLogger("support", sharedSinks_, spdlog::level::trace);
    createLogger("viscosity", sharedSinks_, spdlog::level::trace);

    // System channels.
    createLogger("ui", sharedSinks_, spdlog::level::info);
    createLogger("network", sharedSinks_, spdlog::level::info);
    createLogger("state", sharedSinks_, spdlog::level::debug);
    createLogger("scenario", sharedSinks_, spdlog::level::info);
    createLogger("tree", sharedSinks_, spdlog::level::info);

    // Keep the default logger for general use.
    auto default_logger =
        std::make_shared<spdlog::logger>("default", sharedSinks_.begin(), sharedSinks_.end());
    default_logger->set_level(spdlog::level::info);
    spdlog::set_default_logger(default_logger);

    // Flush periodically.
    spdlog::flush_every(std::chrono::seconds(1));

    initialized_ = true;
    spdlog::info("LoggingChannels initialized successfully");
}

std::shared_ptr<spdlog::logger> LoggingChannels::get(const std::string& channel)
{
    auto logger = spdlog::get(channel);
    if (!logger) {
        // If not initialized or channel doesn't exist, return default logger.
        return spdlog::default_logger();
    }
    return logger;
}

void LoggingChannels::configureFromString(const std::string& spec)
{
    if (spec.empty()) return;

    // Parse format: "channel:level,channel2:level2"
    std::stringstream ss(spec);
    std::string item;

    while (std::getline(ss, item, ',')) {
        // Trim whitespace.
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);

        // Split by colon.
        size_t colonPos = item.find(':');
        if (colonPos == std::string::npos) {
            spdlog::warn("Invalid channel spec (missing colon): {}", item);
            continue;
        }

        std::string channel = item.substr(0, colonPos);
        std::string levelStr = item.substr(colonPos + 1);

        // Trim channel and level strings.
        channel.erase(0, channel.find_first_not_of(" \t"));
        channel.erase(channel.find_last_not_of(" \t") + 1);
        levelStr.erase(0, levelStr.find_first_not_of(" \t"));
        levelStr.erase(levelStr.find_last_not_of(" \t") + 1);

        // Parse level.
        auto level = parseLevelString(levelStr);

        // Apply to channel(s).
        if (channel == "*") {
            // Apply to all registered loggers.
            auto& registry = spdlog::details::registry::instance();
            registry.apply_all(
                [level](std::shared_ptr<spdlog::logger> logger) { logger->set_level(level); });
            spdlog::debug("Set all channels to level: {}", spdlog::level::to_string_view(level));
        }
        else {
            setChannelLevel(channel, level);
        }
    }
}

void LoggingChannels::setChannelLevel(const std::string& channel, spdlog::level::level_enum level)
{
    auto logger = spdlog::get(channel);
    if (logger) {
        logger->set_level(level);
        spdlog::debug(
            "Set channel '{}' to level: {}", channel, spdlog::level::to_string_view(level));
    }
    else {
        spdlog::warn("Channel '{}' not found, cannot set level", channel);
    }
}

void LoggingChannels::createLogger(
    const std::string& name,
    const std::vector<spdlog::sink_ptr>& sinks,
    spdlog::level::level_enum level)
{
    auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger->set_level(level);
    spdlog::register_logger(logger);
}

spdlog::level::level_enum LoggingChannels::parseLevelString(const std::string& levelStr)
{
    std::string lower = levelStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "trace") {
        return spdlog::level::trace;
    }
    else if (lower == "debug") {
        return spdlog::level::debug;
    }
    else if (lower == "info") {
        return spdlog::level::info;
    }
    else if (lower == "warn" || lower == "warning") {
        return spdlog::level::warn;
    }
    else if (lower == "error" || lower == "err") {
        return spdlog::level::err;
    }
    else if (lower == "critical") {
        return spdlog::level::critical;
    }
    else if (lower == "off") {
        return spdlog::level::off;
    }
    else {
        spdlog::warn("Unknown log level '{}', defaulting to info", levelStr);
        return spdlog::level::info;
    }
}

bool LoggingChannels::initializeFromConfig(const std::string& configPath)
{
    if (initialized_) {
        spdlog::warn("LoggingChannels already initialized, skipping re-initialization");
        return false;
    }

    // Load config with .local override support.
    auto config = loadConfigFile(configPath);

    // Apply configuration.
    applyConfig(config);

    initialized_ = true;
    return true;
}

bool LoggingChannels::createDefaultConfigFile(const std::string& path)
{
    // Default configuration.
    nlohmann::json defaultConfig = {
        { "defaults",
          { { "console_level", "info" },
            { "file_level", "debug" },
            { "pattern", "[%H:%M:%S.%e] [%n] [%^%l%$] %v" },
            { "flush_interval_ms", 1000 } } },
        { "sinks",
          { { "console", { { "enabled", true }, { "level", "info" }, { "colored", true } } },
            { "file",
              { { "enabled", true },
                { "level", "debug" },
                { "path", "sparkle-duck.log" },
                { "truncate", true },
                { "max_size_mb", 100 },
                { "max_files", 3 } } },
            { "specialized",
              { { "swap_trace",
                  { { "enabled", false },
                    { "channel_filter", nlohmann::json::array({ "swap" }) },
                    { "path", "swap-trace.log" },
                    { "level", "trace" } } },
                { "physics_deep",
                  { { "enabled", false },
                    { "channel_filter",
                      nlohmann::json::array({ "physics", "collision", "cohesion" }) },
                    { "path", "physics-deep.log" },
                    { "level", "trace" } } } } } } },
        { "channels",
          { { "collision", "info" },
            { "cohesion", "info" },
            { "friction", "info" },
            { "network", "info" },
            { "physics", "info" },
            { "pressure", "info" },
            { "scenario", "info" },
            { "state", "debug" },
            { "support", "info" },
            { "swap", "info" },
            { "ui", "info" },
            { "viscosity", "info" } } },
        { "runtime",
          { { "allow_reload", true }, { "watch_config", false }, { "reload_signal", "SIGUSR1" } } }
    };

    try {
        std::ofstream configFile(path);
        if (!configFile.is_open()) {
            spdlog::error("Failed to create config file: {}", path);
            return false;
        }
        configFile << defaultConfig.dump(2) << std::endl;
        spdlog::info("Created default logging config file: {}", path);
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to write default config file {}: {}", path, e.what());
        return false;
    }
}

nlohmann::json LoggingChannels::loadConfigFile(const std::string& configPath)
{
    namespace fs = std::filesystem;

    // Default configuration (in-memory fallback).
    nlohmann::json defaultConfig = {
        { "defaults",
          { { "console_level", "info" },
            { "file_level", "debug" },
            { "pattern", "[%H:%M:%S.%e] [%n] [%^%l%$] %v" },
            { "flush_interval_ms", 1000 } } },
        { "sinks",
          { { "console", { { "enabled", true }, { "level", "info" }, { "colored", true } } },
            { "file",
              { { "enabled", true },
                { "level", "debug" },
                { "path", "sparkle-duck.log" },
                { "truncate", true } } } } },
        { "channels",
          { { "collision", "info" },
            { "cohesion", "info" },
            { "friction", "info" },
            { "network", "info" },
            { "physics", "info" },
            { "pressure", "info" },
            { "scenario", "info" },
            { "state", "debug" },
            { "support", "info" },
            { "swap", "info" },
            { "ui", "info" },
            { "viscosity", "info" } } }
    };

    // Try .local version first.
    std::string localPath = configPath + ".local";
    std::string pathToUse;

    if (fs::exists(localPath)) {
        pathToUse = localPath;
        spdlog::info("Using local config override: {}", localPath);
    }
    else if (fs::exists(configPath)) {
        pathToUse = configPath;
        spdlog::info("Using default config: {}", configPath);
    }
    else {
        // Neither file exists - create default config file.
        spdlog::info("Config file not found, creating default: {}", configPath);
        if (createDefaultConfigFile(configPath)) {
            pathToUse = configPath;
        }
        else {
            spdlog::warn("Could not create config file, using built-in defaults");
            return defaultConfig;
        }
    }

    // Try to load JSON.
    try {
        std::ifstream configFile(pathToUse);
        if (!configFile.is_open()) {
            spdlog::error("FATAL: Cannot open config file: {}", pathToUse);
            spdlog::error("Check file permissions or delete the file to regenerate defaults.");
            std::exit(1);
        }

        nlohmann::json config = nlohmann::json::parse(configFile);
        spdlog::info("Loaded logging config from {}", pathToUse);
        return config;
    }
    catch (const nlohmann::json::parse_error& e) {
        spdlog::error("FATAL: Failed to parse config file {}: {}", pathToUse, e.what());
        spdlog::error("Fix the JSON syntax or delete the file to regenerate defaults.");
        std::exit(1);
    }
    catch (const std::exception& e) {
        spdlog::error("FATAL: Error reading config file {}: {}", pathToUse, e.what());
        spdlog::error("Check file permissions or delete the file to regenerate defaults.");
        std::exit(1);
    }
}

void LoggingChannels::applyConfig(const nlohmann::json& config)
{
    // Extract defaults with fallbacks.
    auto consoleLevel = spdlog::level::info;
    auto fileLevel = spdlog::level::debug;
    std::string pattern = "[%H:%M:%S.%e] [%n] [%^%l%$] %v";
    int flushIntervalMs = 1000;

    try {
        if (config.contains("defaults")) {
            auto& defaults = config["defaults"];
            if (defaults.contains("console_level")) {
                consoleLevel = parseLevelString(defaults["console_level"].get<std::string>());
            }
            if (defaults.contains("file_level")) {
                fileLevel = parseLevelString(defaults["file_level"].get<std::string>());
            }
            if (defaults.contains("pattern")) {
                pattern = defaults["pattern"].get<std::string>();
            }
            if (defaults.contains("flush_interval_ms")) {
                flushIntervalMs = defaults["flush_interval_ms"].get<int>();
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::warn("Error reading defaults from config: {}, using built-in defaults", e.what());
    }

    // Create sinks.
    std::vector<spdlog::sink_ptr> sinks;

    try {
        if (config.contains("sinks")) {
            auto& sinksConfig = config["sinks"];

            // Console sink.
            if (sinksConfig.contains("console")) {
                auto& consoleCfg = sinksConfig["console"];
                bool enabled = consoleCfg.value("enabled", true);
                if (enabled) {
                    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                    auto level = parseLevelString(consoleCfg.value("level", "info"));
                    console_sink->set_level(level);
                    sinks.push_back(console_sink);
                }
            }

            // File sink.
            if (sinksConfig.contains("file")) {
                auto& fileCfg = sinksConfig["file"];
                bool enabled = fileCfg.value("enabled", true);
                if (enabled) {
                    std::string path = fileCfg.value("path", "sparkle-duck.log");
                    auto level = parseLevelString(fileCfg.value("level", "debug"));

                    // Use rotating sink if max_size_mb is specified, otherwise basic sink.
                    std::shared_ptr<spdlog::sinks::sink> file_sink;
                    if (fileCfg.contains("max_size_mb")) {
                        size_t maxSizeMB = fileCfg.value("max_size_mb", 100);
                        size_t maxFiles = fileCfg.value("max_files", 3);
                        size_t maxSizeBytes = maxSizeMB * 1024 * 1024;
                        file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                            path, maxSizeBytes, maxFiles);
                        spdlog::info(
                            "Using rotating file sink: {} (max {} MB, {} files)",
                            path,
                            maxSizeMB,
                            maxFiles);
                    }
                    else {
                        bool truncate = fileCfg.value("truncate", true);
                        file_sink =
                            std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, truncate);
                    }

                    file_sink->set_level(level);
                    sinks.push_back(file_sink);
                }
            }

            // Specialized sinks.
            if (sinksConfig.contains("specialized")) {
                createSpecializedSinks(sinksConfig["specialized"]);
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Error creating sinks from config: {}, using defaults", e.what());
        // Create default sinks if config failed.
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(consoleLevel);
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>("sparkle-duck.log", true);
        file_sink->set_level(fileLevel);
        sinks = { console_sink, file_sink };
    }

    sharedSinks_ = sinks;

    // Set pattern.
    spdlog::set_pattern(pattern);

    // Create channel loggers.
    createLogger("collision", sharedSinks_, spdlog::level::trace);
    createLogger("cohesion", sharedSinks_, spdlog::level::trace);
    createLogger("friction", sharedSinks_, spdlog::level::trace);
    createLogger("network", sharedSinks_, spdlog::level::trace);
    createLogger("physics", sharedSinks_, spdlog::level::trace);
    createLogger("pressure", sharedSinks_, spdlog::level::trace);
    createLogger("scenario", sharedSinks_, spdlog::level::trace);
    createLogger("state", sharedSinks_, spdlog::level::trace);
    createLogger("support", sharedSinks_, spdlog::level::trace);
    createLogger("swap", sharedSinks_, spdlog::level::trace);
    createLogger("tree", sharedSinks_, spdlog::level::trace);
    createLogger("ui", sharedSinks_, spdlog::level::trace);
    createLogger("viscosity", sharedSinks_, spdlog::level::trace);

    // Apply channel levels from config.
    try {
        if (config.contains("channels")) {
            auto& channels = config["channels"];
            for (auto& [channel, levelStr] : channels.items()) {
                auto level = parseLevelString(levelStr.get<std::string>());
                setChannelLevel(channel, level);
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::warn("Error applying channel levels from config: {}", e.what());
    }

    // Create default logger.
    auto default_logger =
        std::make_shared<spdlog::logger>("default", sharedSinks_.begin(), sharedSinks_.end());
    default_logger->set_level(spdlog::level::info);
    spdlog::set_default_logger(default_logger);

    // Set flush interval.
    spdlog::flush_every(std::chrono::milliseconds(flushIntervalMs));

    spdlog::info("LoggingChannels initialized from config successfully");
}

void LoggingChannels::createSpecializedSinks(const nlohmann::json& specializedConfig)
{
    try {
        for (auto& [name, cfg] : specializedConfig.items()) {
            bool enabled = cfg.value("enabled", false);
            if (!enabled) {
                spdlog::debug("Specialized sink '{}' is disabled", name);
                continue;
            }

            std::string path = cfg.value("path", name + ".log");
            auto level = parseLevelString(cfg.value("level", "trace"));

            // Create file sink for this specialized logger.
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true);
            sink->set_level(level);

            // Get channel filters.
            std::vector<std::string> channelFilters;
            if (cfg.contains("channel_filter")) {
                channelFilters = cfg["channel_filter"].get<std::vector<std::string>>();
            }

            // Create a logger for each filtered channel.
            for (const auto& channel : channelFilters) {
                std::string loggerName = channel + "_" + name;
                auto logger = std::make_shared<spdlog::logger>(loggerName, sink);
                logger->set_level(level);
                spdlog::register_logger(logger);
                spdlog::info(
                    "Created specialized sink '{}' for channel '{}' -> {}", name, channel, path);
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Error creating specialized sinks: {}", e.what());
    }
}

} // namespace DirtSim