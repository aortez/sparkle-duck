#pragma once

#include "TreeCommands.h"
#include <string>

namespace DirtSim {

class Tree;
class World;

enum class CommandResult { SUCCESS, INSUFFICIENT_ENERGY, INVALID_TARGET, BLOCKED };

struct CommandExecutionResult {
    CommandResult result;
    std::string message;

    bool succeeded() const { return result == CommandResult::SUCCESS; }
};

class TreeCommandProcessor {
public:
    static CommandExecutionResult execute(Tree& tree, World& world, const TreeCommand& cmd);
};

} // namespace DirtSim
