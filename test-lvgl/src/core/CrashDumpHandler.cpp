#include "CrashDumpHandler.h"
#include "World.h"
#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

// Static member initialization.
World* CrashDumpHandler::world_ = nullptr;
std::string CrashDumpHandler::dump_directory_ = "./";
bool CrashDumpHandler::installed_ = false;

void CrashDumpHandler::install(World* world)
{
    if (installed_) {
        spdlog::warn("CrashDumpHandler already installed");
        return;
    }

    world_ = world;
    installed_ = true;

    spdlog::info("CrashDumpHandler installed - crash dumps will be saved to: {}", dump_directory_);
}

void CrashDumpHandler::uninstall()
{
    if (!installed_) {
        return;
    }

    world_ = nullptr;
    installed_ = false;

    spdlog::info("CrashDumpHandler uninstalled");
}

void CrashDumpHandler::setDumpDirectory(const std::string& directory)
{
    dump_directory_ = directory;
    if (!dump_directory_.empty() && dump_directory_.back() != '/') {
        dump_directory_ += '/';
    }

    spdlog::info("CrashDumpHandler dump directory set to: {}", dump_directory_);
}

void CrashDumpHandler::dumpWorldState(const char* reason)
{
    if (!installed_ || !world_) {
        spdlog::error("CrashDumpHandler not installed or no world available for dump");
        return;
    }

    std::string filename = generateDumpFilename(reason);
    writeWorldStateToFile(filename, reason);
    logDumpSummary(filename, reason);
}

void CrashDumpHandler::onAssertionFailure(
    const char* condition, const char* file, int line, const char* message)
{
    if (!installed_ || !world_) {
        spdlog::error(
            "ASSERTION FAILURE: {} at {}:{} - {}", condition, file, line, message ? message : "");
        spdlog::error("CrashDumpHandler not available for crash dump");
        return;
    }

    spdlog::error("=== ASSERTION FAILURE DETECTED ===");
    spdlog::error("Condition: {}", condition);
    spdlog::error("Location: {}:{}", file, line);
    spdlog::error("Message: {}", message ? message : "No message");
    spdlog::error("Generating crash dump...");

    std::string filename = generateDumpFilename("assertion_failure");
    writeWorldStateToFile(filename, "Assertion Failure", condition, file, line, message);
    logDumpSummary(filename, "Assertion Failure");

    spdlog::error("=== CRASH DUMP COMPLETE ===");
    spdlog::error("Dump saved to: {}", filename);
    spdlog::error("Application will now terminate");
}

std::string CrashDumpHandler::generateDumpFilename(const char* reason)
{
    // Generate timestamp.
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream filename;
    filename << dump_directory_ << "crash-dump-";
    filename << std::put_time(std::localtime(&time_t), "%Y%m%d-%H%M%S");
    filename << "-" << std::setfill('0') << std::setw(3) << ms.count();
    filename << "-" << reason << ".json";

    return filename.str();
}

void CrashDumpHandler::writeWorldStateToFile(
    const std::string& filename,
    const char* reason,
    const char* condition,
    const char* file,
    int line,
    const char* message)
{
    try {
        if (!world_) {
            spdlog::error("No world available for crash dump");
            return;
        }

        // Create JSON document.
        nlohmann::json doc;

        // Add crash information.
        doc["crash_info"]["reason"] = reason;

        // Add timestamp.
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        char timestamp[64];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
        doc["crash_info"]["timestamp"] = timestamp;

        // Add assertion details if available.
        if (condition) {
            doc["crash_info"]["assertion_condition"] = condition;
        }
        if (file) {
            doc["crash_info"]["source_file"] = file;
        }
        if (line > 0) {
            doc["crash_info"]["source_line"] = line;
        }
        if (message) {
            doc["crash_info"]["assertion_message"] = message;
        }

        // Add basic world information.
        doc["world_info"]["width"] = world_->getWidth();
        doc["world_info"]["height"] = world_->getHeight();
        doc["world_info"]["timestep"] = world_->getTimestep();
        doc["world_info"]["total_mass"] = world_->getTotalMass();
        doc["world_info"]["removed_mass"] = world_->getRemovedMass();
        doc["world_info"]["world_type"] = "World";

        // Serialize complete world state using world_->toJSON().
        doc["world_state"] = world_->toJSON();

        // Write to file.
        std::ofstream file_stream(filename);
        if (!file_stream.is_open()) {
            spdlog::error("Failed to open crash dump file: {}", filename);
            return;
        }

        std::string json_str = doc.dump(2);  // Indented with 2 spaces.
        file_stream << json_str;
        file_stream.close();

        spdlog::info("Crash dump written successfully: {} bytes", json_str.size());
    }
    catch (const std::exception& e) {
        spdlog::error("Exception while writing crash dump: {}", e.what());
    }
    catch (...) {
        spdlog::error("Unknown exception while writing crash dump");
    }
}

void CrashDumpHandler::logDumpSummary(const std::string& filename, const char* reason)
{
    if (!world_) {
        return;
    }

    spdlog::info("=== CRASH DUMP SUMMARY ===");
    spdlog::info("Reason: {}", reason);
    spdlog::info("File: {}", filename);
    spdlog::info(
        "World: {}x{} cells, {} timesteps",
        world_->getWidth(),
        world_->getHeight(),
        world_->getTimestep());
    spdlog::info(
        "Mass: {:.3f} total, {:.3f} removed", world_->getTotalMass(), world_->getRemovedMass());

    const char* worldTypeName = "World";
    spdlog::info("Physics: {}", worldTypeName);
    spdlog::info("=========================");
}