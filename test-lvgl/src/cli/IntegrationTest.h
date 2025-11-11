#pragma once

#include <string>

namespace DirtSim {
namespace Client {

/**
 * @brief Runs integration test (server + UI + commands + cleanup).
 *
 * Launches server and UI, runs a simple simulation step, then cleanly exits.
 * Returns 0 on success, non-zero on failure.
 */
class IntegrationTest {
public:
    IntegrationTest();
    ~IntegrationTest();

    /**
     * @brief Run the integration test.
     *
     * @param serverPath Path to server binary.
     * @param uiPath Path to UI binary.
     * @return Exit code (0 = success, non-zero = failure).
     */
    int run(const std::string& serverPath, const std::string& uiPath);
};

} // namespace Client
} // namespace DirtSim
