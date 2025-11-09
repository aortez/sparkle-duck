# AI Integration Ideas

Quick notes on potential uses of local Ollama setup (NVIDIA 4090) for Sparkle Duck.

## 1. Natural Language CLI with Physics DSL

**Concept**: Natural language → LLM generates simple DSL → CLI interprets DSL → WebSocket API commands

**Example DSL**:
```python
add_block WATER x=50 y=20 width=30 height=40
repeat 10:
    add_block DIRT x=random(10,150) y=0 width=5 height=5
    step 20
end
wait_until settled()
```

**Use cases**: Rapid scenario prototyping, integration testing in plain English

**Needs**: Material manipulation APIs (add/remove materials)

## 2. LLM as Tree Organism Brain

**Concept**: Use LLM for high-level tree decision making (growth, survival strategies)

**Three layers**:
- Reactive (hand-coded, every step): Physics, immediate responses
- Strategic (LLM, every 50 steps): Growth direction, resource prioritization
- Execution (hand-coded): Translate LLM decisions to cell operations

**Sensory input**: Scale-invariant 15×15 grid with material histograms (see plant.md)

**Action space**: GrowWoodCommand, GrowLeafCommand, GrowRootCommand, WaitCommand

**Interesting bits**: Different prompts = different tree personalities, emergent behavior, multi-tree competition

**Needs**: Tree organism implementation, resource systems

**Note**: Full sensory data architecture and brain interface defined in plant.md

## 2a. Neural Network Tree Brains (Integrated with plant.md)

**Concept**: Small neural networks as tree brains, evolved via genetic algorithms

**Scale-invariant input**:
- Fixed 15×15 grid regardless of tree size
- Material histograms per cell (8 values)
- Small trees: crisp one-hot encoding
- Large trees: smooth distributions (automatic downsampling)
- ~1800 input neurons total

**Output**:
- 675 neurons for grow commands (15×15×3 for WOOD/LEAF/ROOT)
- 1 neuron for WAIT
- Argmax selection, map back to world coordinates

**Evolution**:
- Genetic algorithms optimize network weights
- Fitness: survival time, growth efficiency, resource management
- Population evolves successful strategies over generations

**Benefits**:
- Fast inference (every timestep)
- Fixed network size (easy to evolve)
- Natural handling of tree scaling

**See**: plant.md Phase 5 for full architecture details

## 3. Interactive Gardener with Narrative

**Concept**: Chat with an AI "gardener" character embedded in the UI who tends to the physics world with personality and narrative

**Example interactions**:
- User: "It looks dry over there"
- Gardener: "You're right, let me add some water... *sprinkles water across the eastern section*"
- User: "What if we built a tower?"
- Gardener: "Interesting! I'll start stacking some blocks... this might get unstable though..."

**Personality**: Conversational, playful, comments on physics behavior, reacts to chaos/collapse

**Similar to CLI idea but**:
- Embedded in LVGL UI (text area + chat history)
- Character-driven interaction (gardener persona)
- Narrative responses, not just command execution
- Visual feedback immediate
- Could pause/resume sim while editing

**Needs**: Material manipulation APIs, LVGL text input widget
