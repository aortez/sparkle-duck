#ifndef WORLD_DIAGRAM_GENERATOR_EMOJI_H
#define WORLD_DIAGRAM_GENERATOR_EMOJI_H

#include <string>

class WorldInterface;

class WorldDiagramGeneratorEmoji {
public:
    static std::string generateEmojiDiagram(const WorldInterface& world);
    static std::string generateMixedDiagram(const WorldInterface& world);
};

#endif // WORLD_DIAGRAM_GENERATOR_EMOJI_H