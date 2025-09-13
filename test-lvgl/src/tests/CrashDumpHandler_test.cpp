#include <gtest/gtest.h>
#include "src/CrashDumpHandler.h"
#include "src/SparkleAssert.h"
#include "src/SimulationManager.h"
#include "src/WorldFactory.h"
#include "src/WorldSetup.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

class CrashDumpHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test directory for dumps.
        test_dir_ = "./test_dumps/";
        std::filesystem::create_directories(test_dir_);
        
        // Create a small simulation for testing.
        manager_ = std::make_unique<SimulationManager>(WorldType::RulesB, 10, 10, nullptr);
        manager_->initialize();
        
        // Install crash dump handler.
        CrashDumpHandler::install(manager_.get());
        CrashDumpHandler::setDumpDirectory(test_dir_);
    }
    
    void TearDown() override {
        // Uninstall handler.
        CrashDumpHandler::uninstall();
        
        // Clean up test dumps.
        try {
            std::filesystem::remove_all(test_dir_);
        } catch (...) {
            // Ignore cleanup errors.
        }
    }
    
    std::vector<std::string> getTestDumpFiles() {
        std::vector<std::string> files;
        for (const auto& entry : std::filesystem::directory_iterator(test_dir_)) {
            if (entry.path().extension() == ".json") {
                files.push_back(entry.path().filename().string());
            }
        }
        return files;
    }
    
    bool validateJsonFile(const std::string& filename) {
        std::ifstream file(test_dir_ + filename);
        if (!file.is_open()) {
            return false;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        // Basic JSON validation - should start with { and end with }
        // and contain expected sections.
        return content.find("{") != std::string::npos &&
               content.find("}") != std::string::npos &&
               content.find("crash_info") != std::string::npos &&
               content.find("world_info") != std::string::npos &&
               content.find("world_state") != std::string::npos;
    }
    
    std::string test_dir_;
    std::unique_ptr<SimulationManager> manager_;
};

TEST_F(CrashDumpHandlerTest, ManualDumpGeneration) {
    // Trigger a manual dump.
    std::cout << "=== TESTING MANUAL CRASH DUMP ===" << std::endl;
    CrashDumpHandler::dumpWorldState("test_manual_dump");
    
    // Check that a dump file was created.
    auto files = getTestDumpFiles();
    std::cout << "Found " << files.size() << " dump files" << std::endl;
    
    EXPECT_EQ(files.size(), 1);
    
    if (!files.empty()) {
        std::cout << "Dump file: " << files[0] << std::endl;
        EXPECT_TRUE(validateJsonFile(files[0]));
        EXPECT_TRUE(files[0].find("test_manual_dump") != std::string::npos);
        
        // Let's also copy the dump to the main directory to inspect.
        std::string src = test_dir_ + files[0];
        std::string dst = "./" + files[0];
        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
        std::cout << "Copied dump to: " << dst << std::endl;
    }
}

TEST_F(CrashDumpHandlerTest, SparkleAssertDumpGeneration) {
    // This test validates that SPARKLE_ASSERT would generate a dump.
    // We can't actually test the assertion failure without terminating the test.
    // But we can test the crash dump handler directly.
    
    CrashDumpHandler::onAssertionFailure("test_condition", "test_file.cpp", 42, "Test assertion message");
    
    // Check that a dump file was created.
    auto files = getTestDumpFiles();
    EXPECT_EQ(files.size(), 1);
    
    if (!files.empty()) {
        EXPECT_TRUE(validateJsonFile(files[0]));
        EXPECT_TRUE(files[0].find("assertion_failure") != std::string::npos);
        
        // Read the file and check for assertion details.
        std::ifstream file(test_dir_ + files[0]);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        EXPECT_TRUE(content.find("test_condition") != std::string::npos);
        EXPECT_TRUE(content.find("test_file.cpp") != std::string::npos);
        EXPECT_TRUE(content.find("Test assertion message") != std::string::npos);
        EXPECT_TRUE(content.find("\"source_line\":42") != std::string::npos);
    }
}

TEST_F(CrashDumpHandlerTest, MultipleSerialDumps) {
    // Generate multiple dumps to test naming and file management.
    CrashDumpHandler::dumpWorldState("dump1");
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Ensure different timestamps.
    CrashDumpHandler::dumpWorldState("dump2");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CrashDumpHandler::dumpWorldState("dump3");
    
    // Check that all dumps were created.
    auto files = getTestDumpFiles();
    EXPECT_EQ(files.size(), 3);
    
    // Validate each file.
    for (const auto& filename : files) {
        EXPECT_TRUE(validateJsonFile(filename));
    }
}

TEST_F(CrashDumpHandlerTest, DumpContainsWorldState) {
    // Modify world state before dumping.
    auto* world = manager_->getWorld();
    ASSERT_NE(world, nullptr);
    
    // Add some material to make the dump more interesting.
    if (world->getWorldType() == WorldType::RulesB) {
        // Add material at a few locations for RulesB.
        world->addMaterialAtPixel(50, 50, MaterialType::DIRT);
        world->addMaterialAtPixel(100, 100, MaterialType::WATER);
    }
    
    // Advance a few timesteps.
    for (int i = 0; i < 5; ++i) {
        world->advanceTime(0.016); // ~60 FPS timestep.
    }
    
    // Generate dump.
    CrashDumpHandler::dumpWorldState("state_test");
    
    // Validate the dump contains expected world information.
    auto files = getTestDumpFiles();
    EXPECT_EQ(files.size(), 1);
    
    if (!files.empty()) {
        std::ifstream file(test_dir_ + files[0]);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        // Check for world dimensions in world_info section.
        EXPECT_TRUE(content.find("\"width\":10") != std::string::npos);
        EXPECT_TRUE(content.find("\"height\":10") != std::string::npos);
        
        // Check for physics system type.
        EXPECT_TRUE(content.find("RulesB") != std::string::npos);
        
        // Check for timestep advancement (should be 5 after running 5 timesteps).
        EXPECT_TRUE(content.find("\"timestep\":5") != std::string::npos);
        
        // Should contain grid data structure.
        EXPECT_TRUE(content.find("grid_data") != std::string::npos || 
                   content.find("cells") != std::string::npos);
    }
}

TEST_F(CrashDumpHandlerTest, HandlerInstallationState) {
    // Test installation/uninstallation behavior.
    CrashDumpHandler::uninstall();
    
    // Should not create dumps when uninstalled.
    CrashDumpHandler::dumpWorldState("should_not_create");
    auto files = getTestDumpFiles();
    EXPECT_EQ(files.size(), 0);
    
    // Reinstall and test.
    CrashDumpHandler::install(manager_.get());
    CrashDumpHandler::setDumpDirectory(test_dir_);
    
    CrashDumpHandler::dumpWorldState("should_create");
    files = getTestDumpFiles();
    EXPECT_EQ(files.size(), 1);
}

// Note: We cannot easily test actual SPARKLE_ASSERT macro triggering. 
// because it would terminate the test process. The assertion logic is 
// tested through direct calls to CrashDumpHandler::onAssertionFailure().