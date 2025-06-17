# Plant Organism Design (PlantA)

## Overview

PlantA introduces living organisms to the WorldB physics simulation. Plants are multi-cellular organisms that grow from seed cells, consume and produce resources, and exhibit complex biological behaviors within the existing material physics system.

## Core Concept

- **Seed Origin**: Each plant grows from a single cell acting as the initial seed
- **Organic Growth**: Plants expand by converting adjacent cells into plant tissue
- **Energy Economy**: Plants balance energy consumption (growth) with energy production (photosynthesis)
- **Resource Dependencies**: Plants require light, water, nutrients, and air for survival and growth

## Plant Cell Types

### WOOD Cells (Structure/Seed)
- **Primary Role**: Structural support and initial seed
- **Capabilities**: Basic resource transport, minimal energy generation
- **Growth**: Can develop into any other plant cell type
- **Durability**: Resistant to damage, provides plant stability

### LEAF Cells (Photosynthesis)
- **Primary Role**: Maximum light capture and air processing
- **Capabilities**: High photosynthesis rate, efficient air exchange
- **Specialization**: Optimized for energy production over structure
- **Environmental**: Prefer exposure to open air and light sources

### ROOT Cells (Nutrient Absorption)
- **Primary Role**: Nutrient extraction from soil
- **Capabilities**: Extract nutrients from DIRT cells, anchor plant
- **Location**: Typically underground or in contact with DIRT
- **Function**: Primary nutrient gateway for entire plant organism

## Resource System

### Energy
- **Storage**: Each plant cell maintains local energy reserves
- **Transport**: Energy flows between connected plant cells
- **Consumption**: Required for growth, maintenance, and reproduction
- **Production**: Generated through photosynthesis equation

### Photosynthesis Equation
```
Energy = (Light × Light_Efficiency) + (Water × Water_Efficiency) + 
         (Nutrients × Nutrient_Efficiency) + (Air × Air_Efficiency)
```

### Resource Gathering Rates

#### Light Collection
- **WOOD cells**: 1x base light gathering
- **LEAF cells**: 5x base light gathering
- **ROOT cells**: 0.1x base light gathering (minimal)

#### Air Exchange
- **WOOD cells**: 1x base air exchange
- **LEAF cells**: 4x base air exchange
- **ROOT cells**: 0.2x base air exchange

#### Water Absorption
- **All plant cells**: Absorb water from adjacent WATER cells
- **ROOT cells**: 2x water absorption efficiency
- **Transport**: Water moves through plant tissue via connected cells

#### Nutrient Extraction
- **ROOT cells**: Extract nutrients from adjacent DIRT cells
- **Transport**: Nutrients distributed through plant network
- **WOOD/LEAF cells**: No direct nutrient gathering

## Growth Mechanics

### Growth Conditions
1. **Energy Threshold**: Cell must have sufficient energy reserves
2. **Adjacent Space**: Target cell must be convertible (DIRT, AIR, or low-density materials)
3. **Resource Access**: New cell type must be sustainable in location
4. **Network Connectivity**: New cell must connect to existing plant network

### Growth Process
1. **Target Selection**: Plant analyzes adjacent cells for growth potential
2. **Material Conversion**: Target cell material converted to plant tissue (atomic operation)
3. **Cell Specialization**: New cell becomes WOOD, LEAF, or ROOT based on environment
4. **Energy Cost**: Growth depletes energy from source cell

### Underground Growth Mechanics
Growing underground presents unique challenges due to soil physics and gravity:

#### Atomic Growth-Replace System
- **Simultaneous Operation**: Root growth and dirt consumption happen atomically
- **No Intermediate Empty Cells**: Prevents cascade of falling dirt particles
- **Physics Bypass**: Growth temporarily overrides normal material physics

#### Soil Structure Integrity
- **Structural Support**: ROOT cells provide stability to surrounding DIRT
- **Supported Soil**: DIRT cells adjacent to ROOT cells resist gravity collapse
- **Network Effect**: Connected root networks create stable underground zones
- **Gradual Loosening**: Soil only becomes unstable when root networks are damaged

#### Energy Costs for Underground Growth
- **Base Growth Cost**: Standard energy for converting materials
- **Displacement Cost**: Additional energy for "pushing through" dense materials
- **Depth Penalty**: Growing deeper underground requires more energy
- **Soil Density**: Denser/compacted soil costs more energy to penetrate

#### Root Network Pathways
- **Pioneer Roots**: First roots create permanent "tunnels" through soil
- **Network Expansion**: Subsequent roots can follow existing pathways at reduced cost
- **Structural Channels**: Established root networks provide stable growth corridors
- **Branching Strategy**: Roots branch from main channels to access nutrients

#### Alternative Growth Strategies
- **Surface Spreading**: Roots prefer growing horizontally near surface where possible
- **Opportunistic Growth**: Roots exploit existing cracks, loose soil, or water channels
- **Depth Targeting**: Deep growth only when shallow nutrients are exhausted
- **Soil Preparation**: Roots may "soften" soil over multiple timesteps before growth

### Growth Patterns
- **ROOT Growth**: Prefers DIRT cells, grows downward/outward
- **LEAF Growth**: Prefers AIR cells, grows toward light sources
- **WOOD Growth**: Structural expansion, connects other cell types

## Environmental Factors

### Light Distribution
- **Source**: Top of simulation world provides maximum light
- **Gradient**: Light intensity decreases with depth
- **Shadows**: Dense materials (METAL, WALL) block light transmission
- **Calculation**: Each cell receives light based on path to surface

### Nutrient Distribution
- **Source**: DIRT cells contain base nutrient levels
- **Depletion**: ROOT cells gradually deplete nutrients from soil
- **Regeneration**: Nutrients slowly regenerate in DIRT over time
- **Concentration**: Some areas may have higher nutrient density

### Water Availability
- **Source**: Adjacent WATER cells provide moisture
- **Transport**: Water moves through plant tissue network
- **Requirements**: All plant cells need minimum water for survival

## Plant Lifecycle

### Growth Phase
- **Rapid Expansion**: High energy investment in new cell production
- **Specialization**: Cells differentiate based on environmental needs
- **Network Building**: Establishing efficient resource transport pathways

### Mature Phase
- **Energy Balance**: Production matches consumption
- **Reproduction**: Excess energy invested in seed production
- **Maintenance**: Ongoing cellular repair and resource management

### Stress/Death Phase
- **Resource Shortage**: Insufficient light, water, or nutrients
- **Cell Death**: Individual cells may die and convert to DIRT
- **Plant Death**: Complete organism failure if critical cells lost

## Implementation Considerations

### Data Structures
- **Plant ID**: Each plant organism needs unique identifier
- **Cell Network**: Track which cells belong to each plant
- **Resource Pools**: Energy and resource storage per cell
- **Growth State**: Track growth potential and timing

### Physics Integration
- **Material Properties**: Plant cells have specific density, elasticity
- **Collision Behavior**: Plant tissue interacts with other materials
- **Growth Forces**: Growing cells exert pressure on surroundings

### Performance Optimization
- **Growth Timing**: Limit growth calculations to active plants
- **Resource Caching**: Cache light/nutrient calculations where possible
- **Network Efficiency**: Optimize resource transport algorithms

## Future Enhancements

### Advanced Behaviors
- **Tropism**: Growth toward/away from stimuli (light, gravity, nutrients)
- **Competition**: Multiple plants competing for limited resources
- **Symbiosis**: Plants sharing resources or providing mutual benefits
- **Seasonal Cycles**: Periodic growth/dormancy patterns

### Ecosystem Interactions
- **Decomposition**: Dead plant material enriches soil nutrients
- **Oxygen Production**: Plants generate AIR cells through photosynthesis
- **Habitat Creation**: Plants modify local environment for other organisms

### Genetic Variation
- **Plant Species**: Different plant types with unique characteristics
- **Mutation**: Random variations in plant properties over generations
- **Evolution**: Natural selection pressure based on survival success

## Questions for Further Development

1. **Energy Values**: What are the specific numeric values for energy production/consumption?
2. **Growth Rate**: How frequently should plants attempt growth (every N timesteps)?
3. **Resource Limits**: Should there be maximum carrying capacity for resources per cell?
4. **Plant Death**: How quickly should dying plants decompose back to base materials?
5. **User Interaction**: Should players be able to plant seeds directly or only through natural reproduction?
6. **Visual Representation**: How should plant cells be visually distinguished from static materials?

## Integration with Existing Systems

### WorldB Compatibility
- **Material System**: Plant cells fit within existing MaterialType enum
- **Physics Engine**: Plant growth works within current cell-based physics
- **UI Integration**: Material picker can include plant seed option
- **Smart Grabber**: Can interact with plant cells like other materials

### Testing Framework
- **Growth Tests**: Verify plant expansion under various conditions
- **Resource Tests**: Validate photosynthesis and resource transport
- **Stress Tests**: Confirm proper behavior under resource scarcity
- **Performance Tests**: Ensure plant calculations don't impact simulation speed