#include "WorldState.h"
#include <stdexcept>

rapidjson::Value WorldState::CellData::toJson(rapidjson::Document::AllocatorType& allocator) const
{
    rapidjson::Value json(rapidjson::kObjectType);

    json.AddMember("material_mass", material_mass, allocator);
    json.AddMember(
        "dominant_material", materialTypeToJson(dominant_material, allocator), allocator);
    json.AddMember("velocity", velocity.toJson(allocator), allocator);
    json.AddMember("com", com.toJson(allocator), allocator);

    return json;
}

WorldState::CellData WorldState::CellData::fromJson(const rapidjson::Value& json)
{
    if (!json.IsObject()) {
        throw std::runtime_error("CellData::fromJson: JSON value must be an object");
    }

    // Validate required fields
    if (!json.HasMember("material_mass") || !json.HasMember("dominant_material")
        || !json.HasMember("velocity") || !json.HasMember("com")) {
        throw std::runtime_error("CellData::fromJson: Missing required fields");
    }

    if (!json["material_mass"].IsNumber()) {
        throw std::runtime_error("CellData::fromJson: 'material_mass' must be a number");
    }

    CellData data;
    data.material_mass = json["material_mass"].GetDouble();
    data.dominant_material = materialTypeFromJson(json["dominant_material"]);
    data.velocity = Vector2d::fromJson(json["velocity"]);
    data.com = Vector2d::fromJson(json["com"]);

    return data;
}

rapidjson::Value WorldState::toJson(rapidjson::Document::AllocatorType& allocator) const
{
    rapidjson::Value json(rapidjson::kObjectType);

    // Add metadata
    rapidjson::Value metadata(rapidjson::kObjectType);
    metadata.AddMember("version", "1.0", allocator);
    metadata.AddMember("generator", "SparkluDuck", allocator);
    json.AddMember("metadata", metadata, allocator);

    // Add grid dimensions
    rapidjson::Value grid(rapidjson::kObjectType);
    grid.AddMember("width", width, allocator);
    grid.AddMember("height", height, allocator);
    grid.AddMember("timestep", timestep, allocator);
    json.AddMember("grid", grid, allocator);

    // Add physics parameters
    rapidjson::Value physics(rapidjson::kObjectType);
    physics.AddMember("gravity", gravity, allocator);
    physics.AddMember("timescale", timescale, allocator);
    physics.AddMember("elasticity_factor", elasticity_factor, allocator);
    physics.AddMember("pressure_scale", pressure_scale, allocator);
    physics.AddMember("dirt_fragmentation_factor", dirt_fragmentation_factor, allocator);
    physics.AddMember("water_pressure_threshold", water_pressure_threshold, allocator);
    json.AddMember("physics", physics, allocator);

    // Add world setup flags
    rapidjson::Value setup(rapidjson::kObjectType);
    setup.AddMember("left_throw_enabled", left_throw_enabled, allocator);
    setup.AddMember("right_throw_enabled", right_throw_enabled, allocator);
    setup.AddMember("lower_right_quadrant_enabled", lower_right_quadrant_enabled, allocator);
    setup.AddMember("walls_enabled", walls_enabled, allocator);
    setup.AddMember("rain_rate", rain_rate, allocator);
    setup.AddMember("time_reversal_enabled", time_reversal_enabled, allocator);
    setup.AddMember("add_particles_enabled", add_particles_enabled, allocator);
    setup.AddMember("cursor_force_enabled", cursor_force_enabled, allocator);
    json.AddMember("setup", setup, allocator);

    // Add grid cell data (only non-empty cells for efficiency)
    rapidjson::Value cells(rapidjson::kArrayType);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const CellData& cell = grid_data[y][x];
            // Only serialize cells with material content
            if (cell.material_mass > 0.0 || cell.dominant_material != MaterialType::AIR) {
                rapidjson::Value cellJson(rapidjson::kObjectType);
                cellJson.AddMember("x", x, allocator);
                cellJson.AddMember("y", y, allocator);
                cellJson.AddMember("data", cell.toJson(allocator), allocator);
                cells.PushBack(cellJson, allocator);
            }
        }
    }
    json.AddMember("cells", cells, allocator);

    return json;
}

WorldState WorldState::fromJson(const rapidjson::Value& json)
{
    if (!json.IsObject()) {
        throw std::runtime_error("WorldState::fromJson: JSON value must be an object");
    }

    // Validate required top-level fields
    if (!json.HasMember("grid") || !json.HasMember("physics") || !json.HasMember("setup")
        || !json.HasMember("cells")) {
        throw std::runtime_error("WorldState::fromJson: Missing required top-level fields");
    }

    const rapidjson::Value& grid = json["grid"];
    const rapidjson::Value& physics = json["physics"];
    const rapidjson::Value& setup = json["setup"];
    const rapidjson::Value& cells = json["cells"];

    // Validate grid section
    if (!grid.IsObject() || !grid.HasMember("width") || !grid.HasMember("height")
        || !grid.HasMember("timestep")) {
        throw std::runtime_error("WorldState::fromJson: Invalid grid section");
    }

    if (!grid["width"].IsUint() || !grid["height"].IsUint() || !grid["timestep"].IsUint()) {
        throw std::runtime_error("WorldState::fromJson: Grid dimensions must be positive integers");
    }

    uint32_t w = grid["width"].GetUint();
    uint32_t h = grid["height"].GetUint();

    // Create WorldState with proper dimensions
    WorldState state(w, h);
    state.timestep = grid["timestep"].GetUint();

    // Load physics parameters
    if (physics.IsObject()) {
        if (physics.HasMember("gravity") && physics["gravity"].IsNumber()) {
            state.gravity = physics["gravity"].GetDouble();
        }
        if (physics.HasMember("timescale") && physics["timescale"].IsNumber()) {
            state.timescale = physics["timescale"].GetDouble();
        }
        if (physics.HasMember("elasticity_factor") && physics["elasticity_factor"].IsNumber()) {
            state.elasticity_factor = physics["elasticity_factor"].GetDouble();
        }
        if (physics.HasMember("pressure_scale") && physics["pressure_scale"].IsNumber()) {
            state.pressure_scale = physics["pressure_scale"].GetDouble();
        }
        if (physics.HasMember("dirt_fragmentation_factor")
            && physics["dirt_fragmentation_factor"].IsNumber()) {
            state.dirt_fragmentation_factor = physics["dirt_fragmentation_factor"].GetDouble();
        }
        if (physics.HasMember("water_pressure_threshold")
            && physics["water_pressure_threshold"].IsNumber()) {
            state.water_pressure_threshold = physics["water_pressure_threshold"].GetDouble();
        }
    }

    // Load setup flags
    if (setup.IsObject()) {
        if (setup.HasMember("left_throw_enabled") && setup["left_throw_enabled"].IsBool()) {
            state.left_throw_enabled = setup["left_throw_enabled"].GetBool();
        }
        if (setup.HasMember("right_throw_enabled") && setup["right_throw_enabled"].IsBool()) {
            state.right_throw_enabled = setup["right_throw_enabled"].GetBool();
        }
        if (setup.HasMember("lower_right_quadrant_enabled")
            && setup["lower_right_quadrant_enabled"].IsBool()) {
            state.lower_right_quadrant_enabled = setup["lower_right_quadrant_enabled"].GetBool();
        }
        if (setup.HasMember("walls_enabled") && setup["walls_enabled"].IsBool()) {
            state.walls_enabled = setup["walls_enabled"].GetBool();
        }
        if (setup.HasMember("rain_rate") && setup["rain_rate"].IsNumber()) {
            state.rain_rate = setup["rain_rate"].GetDouble();
        }
        if (setup.HasMember("time_reversal_enabled") && setup["time_reversal_enabled"].IsBool()) {
            state.time_reversal_enabled = setup["time_reversal_enabled"].GetBool();
        }
        if (setup.HasMember("add_particles_enabled") && setup["add_particles_enabled"].IsBool()) {
            state.add_particles_enabled = setup["add_particles_enabled"].GetBool();
        }
        if (setup.HasMember("cursor_force_enabled") && setup["cursor_force_enabled"].IsBool()) {
            state.cursor_force_enabled = setup["cursor_force_enabled"].GetBool();
        }
    }

    // Load cell data
    if (!cells.IsArray()) {
        throw std::runtime_error("WorldState::fromJson: 'cells' must be an array");
    }

    for (rapidjson::SizeType i = 0; i < cells.Size(); ++i) {
        const rapidjson::Value& cellEntry = cells[i];

        if (!cellEntry.IsObject() || !cellEntry.HasMember("x") || !cellEntry.HasMember("y")
            || !cellEntry.HasMember("data")) {
            throw std::runtime_error("WorldState::fromJson: Invalid cell entry format");
        }

        if (!cellEntry["x"].IsUint() || !cellEntry["y"].IsUint()) {
            throw std::runtime_error(
                "WorldState::fromJson: Cell coordinates must be positive integers");
        }

        uint32_t x = cellEntry["x"].GetUint();
        uint32_t y = cellEntry["y"].GetUint();

        if (!state.isValidCoordinate(x, y)) {
            throw std::runtime_error("WorldState::fromJson: Cell coordinates out of bounds");
        }

        CellData cellData = CellData::fromJson(cellEntry["data"]);
        state.setCellData(x, y, cellData);
    }

    return state;
}