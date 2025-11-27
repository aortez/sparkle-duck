/**
 * @file test_command_registry.cpp
 * @brief Demonstration of compile-time command registry extraction.
 *
 * This file shows how the auto-generated command registry works.
 * Compile and run to see all available commands.
 */

#include "CommandRegistry.h"
#include <iostream>

int main()
{
    std::cout << "=== Auto-Generated Command Registry ===\n\n";

    std::cout << "Server API Commands (" << DirtSim::Client::SERVER_COMMAND_NAMES.size()
              << " total):\n";
    for (const auto& cmd : DirtSim::Client::SERVER_COMMAND_NAMES) {
        std::cout << "  - " << cmd << "\n";
    }

    std::cout << "\nUI API Commands (" << DirtSim::Client::UI_COMMAND_NAMES.size() << " total):\n";
    for (const auto& cmd : DirtSim::Client::UI_COMMAND_NAMES) {
        std::cout << "  - " << cmd << "\n";
    }

    std::cout << "\n=== Command Validation ===\n";
    std::cout << "Is 'StateGet' a server command? "
              << (DirtSim::Client::isServerCommand("StateGet") ? "YES" : "NO") << "\n";
    std::cout << "Is 'DrawDebugToggle' a UI command? "
              << (DirtSim::Client::isUiCommand("DrawDebugToggle") ? "YES" : "NO") << "\n";
    std::cout << "Is 'FakeCommand' a server command? "
              << (DirtSim::Client::isServerCommand("FakeCommand") ? "YES" : "NO") << "\n";

    return 0;
}
