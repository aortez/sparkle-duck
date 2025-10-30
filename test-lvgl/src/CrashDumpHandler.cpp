#include "CrashDumpHandler.h"
#include "SimulationManager.h"
#include "WorldInterface.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include "lvgl/src/libs/thorvg/rapidjson/stringbuffer.h"
#include "lvgl/src/libs/thorvg/rapidjson/writer.h"
#include "spdlog/spdlog.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

// Static member initialization.
SimulationManager* CrashDumpHandler::manager_ = nullptr;
std::string CrashDumpHandler::dump_directory_ = "./";
bool CrashDumpHandler::installed_ = false;

void CrashDumpHandler::install(SimulationManager* manager)
{
    if (installed_) {
        spdlog::warn("CrashDumpHandler already installed");
        return;
    }

    manager_ = manager;
    installed_ = true;

    spdlog::info("CrashDumpHandler installed - crash dumps will be saved to: {}", dump_directory_);
}

void CrashDumpHandler::uninstall()
{
    if (!installed_) {
        return;
    }

    manager_ = nullptr;
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
    if (!installed_ || !manager_) {
        spdlog::error("CrashDumpHandler not installed or no manager available for dump");
        return;
    }

    std::string filename = generateDumpFilename(reason);
    writeWorldStateToFile(filename, reason);
    logDumpSummary(filename, reason);
}

void CrashDumpHandler::onAssertionFailure(
    const char* condition, const char* file, int line, const char* message)
{
    if (!installed_ || !manager_) {
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
        WorldInterface* world = manager_->getWorld();
        if (!world) {
            spdlog::error("No world available for crash dump");
            return;
        }

        // Create JSON document.
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        // Add crash information.
        rapidjson::Value crashInfo(rapidjson::kObjectType);
        crashInfo.AddMember("reason", rapidjson::Value(reason, allocator), allocator);

        // Add timestamp.
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        char timestamp[64];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
        crashInfo.AddMember("timestamp", rapidjson::Value(timestamp, allocator), allocator);

        // Add assertion details if available.
        if (condition) {
            crashInfo.AddMember(
                "assertion_condition", rapidjson::Value(condition, allocator), allocator);
        }
        if (file) {
            crashInfo.AddMember("source_file", rapidjson::Value(file, allocator), allocator);
        }
        if (line > 0) {
            crashInfo.AddMember("source_line", line, allocator);
        }
        if (message) {
            crashInfo.AddMember(
                "assertion_message", rapidjson::Value(message, allocator), allocator);
        }

        doc.AddMember("crash_info", crashInfo, allocator);

        // Add basic world information.
        rapidjson::Value worldInfo(rapidjson::kObjectType);
        worldInfo.AddMember("width", world->getWidth(), allocator);
        worldInfo.AddMember("height", world->getHeight(), allocator);
        worldInfo.AddMember("timestep", world->getTimestep(), allocator);
        worldInfo.AddMember("total_mass", world->getTotalMass(), allocator);
        worldInfo.AddMember("removed_mass", world->getRemovedMass(), allocator);

        // Add world type information.
        worldInfo.AddMember("world_type", "World", allocator);

        doc.AddMember("world_info", worldInfo, allocator);

        // TODO: Serialize complete world state using world->toJSON()
        // For now, just save basic info.
        rapidjson::Value worldState(rapidjson::kObjectType);
        worldState.AddMember("width", world->getWidth(), allocator);
        worldState.AddMember("height", world->getHeight(), allocator);
        worldState.AddMember("timestep", world->getTimestep(), allocator);
        doc.AddMember("world_state", worldState, allocator);

        // Write to file.
        std::ofstream file_stream(filename);
        if (!file_stream.is_open()) {
            spdlog::error("Failed to open crash dump file: {}", filename);
            return;
        }

        // Use writer for JSON output.
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        file_stream << buffer.GetString();
        file_stream.close();

        spdlog::info("Crash dump written successfully: {} bytes", buffer.GetSize());
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
    if (!manager_ || !manager_->getWorld()) {
        return;
    }

    WorldInterface* world = manager_->getWorld();

    spdlog::info("=== CRASH DUMP SUMMARY ===");
    spdlog::info("Reason: {}", reason);
    spdlog::info("File: {}", filename);
    spdlog::info(
        "World: {}x{} cells, {} timesteps",
        world->getWidth(),
        world->getHeight(),
        world->getTimestep());
    spdlog::info(
        "Mass: {:.3f} total, {:.3f} removed", world->getTotalMass(), world->getRemovedMass());

    const char* worldTypeName = "World";
    spdlog::info("Physics: {}", worldTypeName);
    spdlog::info("=========================");
}