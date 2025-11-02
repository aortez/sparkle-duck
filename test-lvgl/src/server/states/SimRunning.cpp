#include "../../core/Cell.h"
#include "../../core/World.h"
#include "../../core/WorldEventGenerator.h"
#include "../StateMachine.h"
#include "../scenarios/Scenario.h"
#include "../scenarios/ScenarioRegistry.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {
namespace State {

void SimRunning::onEnter(StateMachine& dsm)
{
    spdlog::info("SimRunning: Entering simulation state");

    // World is created in StateMachine constructor.
    if (!dsm.world) {
        spdlog::error("SimRunning: No World available!");
        return;
    }

    // Initialize step count from shared state (preserves count when resuming from pause).
    stepCount = dsm.getSharedState().getCurrentStep();

    // Log whether we're starting fresh or resuming.
    if (stepCount == 0) {
        spdlog::info("SimRunning: Starting fresh simulation");
    } else {
        spdlog::info("SimRunning: Resuming simulation at step {}", stepCount);
    }

    spdlog::info("SimRunning: Ready to run simulation");
}

void SimRunning::onExit(StateMachine& /*dsm. */)
{
    spdlog::info("SimRunning: Exiting state");
}

State::Any SimRunning::onEvent(const AdvanceSimulationCommand& /*cmd. */, StateMachine& dsm)
{
    // NOTE: When in SimRunning state, the simulation is advanced by the UI thread's
    // processFrame loop (simulator_loop.h:83). We should NOT call advanceTime here
    // because that would create a race condition with two threads advancing physics
    // simultaneously. This handler exists for compatibility but is a no-op in SimRunning.
    // The UI loop handles step counting and stat updates automatically.

    spdlog::trace("SimRunning: AdvanceSimulationCommand received but ignored (UI loop drives simulation)");

    // Suppress unused parameter warning.
    (void)dsm;

    return *this;  // Stay in SimRunning.
}

State::Any SimRunning::onEvent(const ApplyScenarioCommand& cmd, StateMachine& dsm)
{
    spdlog::info("SimRunning: Applying scenario: {}", cmd.scenarioName);

    auto& registry = ScenarioRegistry::getInstance();
    auto* scenario = registry.getScenario(cmd.scenarioName);

    if (!scenario) {
        spdlog::error("Scenario not found: {}", cmd.scenarioName);
        return *this;
    }

    const auto& metadata = scenario->getMetadata();

    // Resize world if needed.
    if (metadata.requiredWidth > 0 && metadata.requiredHeight > 0) {
        dsm.resizeWorldIfNeeded(metadata.requiredWidth, metadata.requiredHeight);
    }
    else {
        dsm.resizeWorldIfNeeded(0, 0); // Restore defaults.
    }

    // Apply scenario's WorldEventGenerator.
    auto setup = scenario->createWorldEventGenerator();
    if (dsm.world) {
        dsm.world->setWorldEventGenerator(std::move(setup));
    }

    return *this;
}

State::Any SimRunning::onEvent(const ResizeWorldCommand& cmd, StateMachine& dsm)
{
    spdlog::info("SimRunning: Resizing world to {}x{}", cmd.width, cmd.height);
    dsm.resizeWorldIfNeeded(cmd.width, cmd.height);
    return *this;
}

State::Any SimRunning::onEvent(const Api::CellGet::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::CellGet::Response;

    // Validate coordinates.
    if (!dsm.world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return *this;
    }

    auto* world = dynamic_cast<World*>(dsm.world.get());
    if (!world) {
        cwc.sendResponse(Response::error(ApiError("World type mismatch")));
        return *this;
    }

    if (cwc.command.x < 0 || cwc.command.y < 0 ||
        static_cast<uint32_t>(cwc.command.x) >= world->getWidth() ||
        static_cast<uint32_t>(cwc.command.y) >= world->getHeight()) {
        cwc.sendResponse(Response::error(ApiError("Invalid coordinates")));
        return *this;
    }

    // Get cell.
    const Cell& cell = world->at(cwc.command.x, cwc.command.y);

    cwc.sendResponse(Response::okay({ cell }));
    return *this;
}

State::Any SimRunning::onEvent(const Api::CellSet::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::CellSet::Response;

    // Validate world availability.
    if (!dsm.world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return *this;
    }

    WorldInterface* worldInterface = dsm.world.get();

    // Validate coordinates.
    if (cwc.command.x < 0 || cwc.command.y < 0 ||
        static_cast<uint32_t>(cwc.command.x) >= worldInterface->getWidth() ||
        static_cast<uint32_t>(cwc.command.y) >= worldInterface->getHeight()) {
        cwc.sendResponse(Response::error(ApiError("Invalid coordinates")));
        return *this;
    }

    // Validate fill ratio.
    if (cwc.command.fill < 0.0 || cwc.command.fill > 1.0) {
        cwc.sendResponse(Response::error(ApiError("Fill must be between 0.0 and 1.0")));
        return *this;
    }

    // Place material.
    worldInterface->addMaterialAtCell(cwc.command.x, cwc.command.y, cwc.command.material, cwc.command.fill);

    cwc.sendResponse(Response::okay(std::monostate{}));
    return *this;
}

State::Any SimRunning::onEvent(const Api::GravitySet::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::GravitySet::Response;

    if (!dsm.world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return *this;
    }

    dsm.world.get()->setGravity(cwc.command.gravity);
    spdlog::info("SimRunning: API set gravity to {}", cwc.command.gravity);

    cwc.sendResponse(Response::okay(std::monostate{}));
    return *this;
}

State::Any SimRunning::onEvent(const Api::Reset::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::Reset::Response;

    spdlog::info("SimRunning: API reset simulation");

    if (dsm.world) {
        dsm.world->setup();
    }

    stepCount = 0;
    dsm.getSharedState().setCurrentStep(0);

    cwc.sendResponse(Response::okay(std::monostate{}));
    return *this;
}

State::Any SimRunning::onEvent(const Api::StateGet::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::StateGet::Response;

    if (!dsm.world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return *this;
    }

    auto* world = dynamic_cast<World*>(dsm.world.get());
    if (!world) {
        cwc.sendResponse(Response::error(ApiError("World type mismatch")));
        return *this;
    }

    // Return complete world state (copy).
    cwc.sendResponse(Response::okay({ *world }));
    return *this;
}

State::Any SimRunning::onEvent(const Api::StepN::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::StepN::Response;

    if (!dsm.world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return *this;
    }

    // Validate frames parameter.
    if (cwc.command.frames <= 0) {
        cwc.sendResponse(Response::error(ApiError("Frames must be positive")));
        return *this;
    }

    // Step simulation.
    for (int i = 0; i < cwc.command.frames; ++i) {
        dsm.world->advanceTime(0.016); // ~60 FPS timestep.
    }

    uint32_t timestep = dsm.world.get()->getTimestep();
    spdlog::debug("SimRunning: API stepped {} frames, timestep now {}", cwc.command.frames, timestep);

    cwc.sendResponse(Response::okay({timestep}));
    return *this;
}

State::Any SimRunning::onEvent(const PauseCommand& /*cmd*/, StateMachine& /*dsm. */)
{
    spdlog::info("SimRunning: Pausing at step {}", stepCount);
    
    // Move the current state into SimPaused.
    return SimPaused{std::move(*this)};
}

State::Any SimRunning::onEvent(const ResetSimulationCommand& /*cmd. */, StateMachine& dsm)
{
    spdlog::info("SimRunning: Resetting simulation");

    if (dsm.world) {
        dsm.world->setup();
    }

    stepCount = 0;
    dsm.getSharedState().setCurrentStep(0);
    
    return *this;  // Stay in SimRunning.
}

State::Any SimRunning::onEvent(const SaveWorldCommand& cmd, StateMachine& /*dsm. */)
{
    Saving saveState;
    saveState.filepath = cmd.filepath;
    return saveState;
}

State::Any SimRunning::onEvent(const StepBackwardCommand& /*cmd*/, StateMachine& dsm)
{
    spdlog::debug("SimRunning: Stepping simulation backward by one timestep");

    if (!dsm.world) {
        spdlog::warn("SimRunning: Cannot step backward - no world available");
        return *this;
    }

    auto world = dsm.world.get();

    // TODO: Implement world->goBackward() method for time reversal.
    (void)world;  // Silence unused variable warning until implemented.
    spdlog::info("StepBackwardCommand: Time reversal not yet implemented");

    return *this;  // Stay in SimRunning.
}

State::Any SimRunning::onEvent(const StepForwardCommand& /*cmd*/, StateMachine& dsm)
{
    if (!dsm.world) {
        spdlog::warn("SimRunning: Cannot step forward - no world available");
        return *this;
    }

    auto world = dsm.world.get();

    // TODO: Implement world->goForward() method for time reversal.
    (void)world;  // Silence unused variable warning until implemented.
    spdlog::info("SimRunning: Step forward requested");

    return *this;  // Stay in SimRunning.
}

State::Any SimRunning::onEvent(const ToggleTimeReversalCommand& /*cmd*/, StateMachine& dsm)
{
    if (!dsm.world) {
        spdlog::warn("SimRunning: Cannot toggle time reversal - no world available");
        return *this;
    }

    auto world = dsm.world.get();

    // TODO: Implement world->toggleTimeReversal() method.
    (void)world;  // Silence unused variable warning until implemented.
    spdlog::info("SimRunning: Toggle time reversal requested");

    return *this;  // Stay in SimRunning.
}

State::Any SimRunning::onEvent(const SetWaterCohesionCommand& cmd, StateMachine& /*dsm*/)
{
    // Cell::setCohesionStrength(cmd.cohesion_value);
    spdlog::info("SimRunning: Set water cohesion to {}", cmd.cohesion_value);
    return *this;
}

State::Any SimRunning::onEvent(const SetWaterViscosityCommand& cmd, StateMachine& /*dsm*/)
{
    // Cell::setViscosityFactor(cmd.viscosity_value);
    spdlog::info("SimRunning: Set water viscosity to {}", cmd.viscosity_value);
    return *this;
}

State::Any SimRunning::onEvent(const SetWaterPressureThresholdCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setWaterPressureThreshold(cmd.threshold_value);
        spdlog::info("SimRunning: Set water pressure threshold to {}", cmd.threshold_value);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetWaterBuoyancyCommand& cmd, StateMachine& /*dsm*/)
{
    // Cell::setBuoyancyStrength(cmd.buoyancy_value);
    spdlog::info("SimRunning: Set water buoyancy to {}", cmd.buoyancy_value);
    return *this;
}

State::Any SimRunning::onEvent(const LoadWorldCommand& cmd, StateMachine& /*dsm*/)
{
    Loading loadState;
    loadState.filepath = cmd.filepath;
    spdlog::info("SimRunning: Loading world from {}", cmd.filepath);
    return loadState;
}

State::Any SimRunning::onEvent(const SetTimestepCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        // TODO: Implement world->setTimestep() method when available.
        (void)world;
    }
    spdlog::debug("SimRunning: Set timestep to {}", cmd.timestep_value);
    return *this;
}

State::Any SimRunning::onEvent(const MouseDownEvent& evt, StateMachine& dsm)
{
    if (!dsm.world) {
        return *this;
    }

    auto* world = dsm.world.get();

    // Always enter GRAB_MODE - either grab existing material or create new and grab it.
    interactionMode = InteractionMode::GRAB_MODE;

    if (world->hasMaterialAtPixel(evt.pixelX, evt.pixelY)) {
        // Cell has material - grab it.
        world->startDragging(evt.pixelX, evt.pixelY);
        spdlog::debug("MouseDown: Grabbing existing material at ({}, {})", evt.pixelX, evt.pixelY);
    } else {
        // Cell is empty - add material first, then grab it.
        auto material = dsm.getSharedState().getSelectedMaterial();
        world->addMaterialAtPixel(evt.pixelX, evt.pixelY, material);
        world->startDragging(evt.pixelX, evt.pixelY);
        spdlog::debug("MouseDown: Creating and grabbing new {} at ({}, {})",
                      static_cast<int>(material), evt.pixelX, evt.pixelY);
    }

    return *this;
}

State::Any SimRunning::onEvent(const MouseMoveEvent& evt, StateMachine& dsm)
{
    if (!dsm.world) {
        return *this;
    }

    auto* world = dsm.world.get();

    // Only update drag position if we're in GRAB_MODE.
    // No more continuous painting in PAINT_MODE.
    if (interactionMode == InteractionMode::GRAB_MODE) {
        world->updateDrag(evt.pixelX, evt.pixelY);
    }

    return *this;
}

State::Any SimRunning::onEvent(const MouseUpEvent& evt, StateMachine& dsm)
{
    if (!dsm.world) {
        return *this;
    }

    auto* world = dsm.world.get();

    if (interactionMode == InteractionMode::GRAB_MODE) {
        // End dragging and release material with velocity.
        world->endDragging(evt.pixelX, evt.pixelY);
        spdlog::debug("MouseUp: Ending GRAB_MODE at ({}, {})", evt.pixelX, evt.pixelY);
    }

    // Reset interaction mode.
    interactionMode = InteractionMode::NONE;

    return *this;
}

State::Any SimRunning::onEvent(const SelectMaterialCommand& cmd, StateMachine& dsm)
{
    dsm.getSharedState().setSelectedMaterial(cmd.material);
    if (dsm.world) {
        dsm.world.get()->setSelectedMaterial(cmd.material);
    }
    spdlog::debug("SimRunning: Selected material {}", static_cast<int>(cmd.material));
    return *this;
}

State::Any SimRunning::onEvent(const SetTimescaleCommand& cmd, StateMachine& dsm)
{
    // Update world directly (source of truth).
    if (auto* world = dsm.world.get()) {
        world->setTimescale(cmd.timescale);
        spdlog::info("SimRunning: Set timescale to {}", cmd.timescale);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetElasticityCommand& cmd, StateMachine& dsm)
{
    // Update world directly (source of truth).
    if (auto* world = dsm.world.get()) {
        world->setElasticityFactor(cmd.elasticity);
        spdlog::info("SimRunning: Set elasticity to {}", cmd.elasticity);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetDynamicStrengthCommand& cmd, StateMachine& dsm)
{
    // Update world directly (source of truth).
    if (auto* world = dsm.world.get()) {
        world->setDynamicPressureStrength(cmd.strength);
        spdlog::info("SimRunning: Set dynamic strength to {:.1f}", cmd.strength);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetGravityCommand& cmd, StateMachine& dsm)
{
    // Update world directly (source of truth).
    if (auto* world = dsm.world.get()) {
        world->setGravity(cmd.gravity);
        spdlog::info("SimRunning: Set gravity to {}", cmd.gravity);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetPressureScaleCommand& cmd, StateMachine& dsm)
{
    // Apply to world.
    if (auto* world = dsm.world.get()) {
        world->setPressureScale(cmd.scale);
    }

    spdlog::debug("SimRunning: Set pressure scale to {}", cmd.scale);
    return *this;
}

State::Any SimRunning::onEvent(const SetPressureScaleWorldBCommand& cmd, StateMachine& dsm)
{
    // Apply to world.
    if (auto* world = dsm.world.get()) {
        world->setPressureScale(cmd.scale);
    }

    spdlog::debug("SimRunning: Set World pressure scale to {}", cmd.scale);
    return *this;
}

State::Any SimRunning::onEvent(const SetCohesionForceStrengthCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setCohesionComForceStrength(cmd.strength);
        spdlog::info("SimRunning: Set cohesion force strength to {}", cmd.strength);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetAdhesionStrengthCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setAdhesionStrength(cmd.strength);
        spdlog::info("SimRunning: Set adhesion strength to {}", cmd.strength);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetViscosityStrengthCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setViscosityStrength(cmd.strength);
        spdlog::info("SimRunning: Set viscosity strength to {}", cmd.strength);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetFrictionStrengthCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setFrictionStrength(cmd.strength);
        spdlog::info("SimRunning: Set friction strength to {}", cmd.strength);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetContactFrictionStrengthCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dynamic_cast<World*>(dsm.world.get())) {
        world->getFrictionCalculator().setFrictionStrength(cmd.strength);
        spdlog::info("SimRunning: Set contact friction strength to {}", cmd.strength);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetCOMCohesionRangeCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setCOMCohesionRange(cmd.range);
        spdlog::info("SimRunning: Set COM cohesion range to {}", cmd.range);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetAirResistanceCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setAirResistanceStrength(cmd.strength);
        spdlog::info("SimRunning: Set air resistance to {}", cmd.strength);
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleHydrostaticPressureCommand& /*cmd*/, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        bool newValue = !world->isHydrostaticPressureEnabled();
        world->setHydrostaticPressureEnabled(newValue);
        spdlog::info("SimRunning: Toggle hydrostatic pressure - now: {}", newValue);
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleDynamicPressureCommand& /*cmd*/, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        bool newValue = !world->isDynamicPressureEnabled();
        world->setDynamicPressureEnabled(newValue);
        spdlog::info("SimRunning: Toggle dynamic pressure - now: {}", newValue);
    }
    return *this;
}

State::Any SimRunning::onEvent(const TogglePressureDiffusionCommand& /*cmd*/, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        bool newValue = !world->isPressureDiffusionEnabled();
        world->setPressureDiffusionEnabled(newValue);
        spdlog::info("SimRunning: Toggle pressure diffusion - now: {}", newValue);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetHydrostaticPressureStrengthCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setHydrostaticPressureStrength(cmd.strength);
        spdlog::info("SimRunning: Set hydrostatic pressure strength to {}", cmd.strength);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetDynamicPressureStrengthCommand& cmd, StateMachine& dsm)
{
    // Apply to world.
    if (auto* world = dsm.world.get()) {
        // TODO: Need to add setDynamicPressureStrength method to WorldInterface.
        // For now, suppress unused warning.
        (void)world;
    }

    spdlog::debug("SimRunning: Set dynamic pressure strength to {}", cmd.strength);
    return *this;
}

State::Any SimRunning::onEvent(const SetRainRateCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setRainRate(cmd.rate);
        spdlog::info("SimRunning: Set rain rate to {}", cmd.rate);
    }
    return *this;
}

// Handle immediate events routed through push system.
State::Any SimRunning::onEvent(const GetFPSCommand& /*cmd. */, StateMachine& dsm)
{
    // FPS is already tracked in shared state and will be in next push update.
    spdlog::debug("SimRunning: GetFPSCommand - FPS will be in next update");

    // Force a push update with FPS dirty flag.
    UiUpdateEvent update = dsm.buildUIUpdate();
    dsm.getSharedState().pushUIUpdate(std::move(update));

    return *this;
}

State::Any SimRunning::onEvent(const GetSimStatsCommand& /*cmd. */, StateMachine& dsm)
{
    // Stats are already tracked and will be in next push update.
    spdlog::debug("SimRunning: GetSimStatsCommand - Stats will be in next update");

    // Force a push update with stats dirty flag.
    UiUpdateEvent update = dsm.buildUIUpdate();
    dsm.getSharedState().pushUIUpdate(std::move(update));

    return *this;
}

State::Any SimRunning::onEvent(const ToggleDebugCommand& /*cmd. */, StateMachine& dsm)
{
    // Toggle debug draw state in world (source of truth).
    if (dsm.world) {
        auto* world = dsm.world.get();
        bool newValue = !world->isDebugDrawEnabled();
        world->setDebugDrawEnabled(newValue);
        spdlog::info("SimRunning: ToggleDebugCommand - Debug draw now: {}", newValue);

        // Push UI update with uiState dirty flag.
        UiUpdateEvent update = dsm.buildUIUpdate();
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }

    return *this;
}

State::Any SimRunning::onEvent(const ToggleCohesionForceCommand& /*cmd. */, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        bool newValue = !world->isCohesionComForceEnabled();
        world->setCohesionComForceEnabled(newValue);
        spdlog::info("SimRunning: ToggleCohesionForceCommand - Cohesion force now: {}", newValue);

        UiUpdateEvent update = dsm.buildUIUpdate();
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleTimeHistoryCommand& /*cmd. */, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        bool newValue = !world->isTimeReversalEnabled();
        world->enableTimeReversal(newValue);
        spdlog::info("SimRunning: ToggleTimeHistoryCommand - Time history now: {}", newValue);

        UiUpdateEvent update = dsm.buildUIUpdate();
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    return *this;
}

State::Any SimRunning::onEvent(const PrintAsciiDiagramCommand& /*cmd. */, StateMachine& dsm)
{
    // Get the current world and print ASCII diagram.
    if (dsm.world) {
        std::string ascii_diagram = dsm.world.get()->toAsciiDiagram();
        spdlog::info("Current world state (ASCII diagram):\n{}", ascii_diagram);
    }
    else {
        spdlog::warn("PrintAsciiDiagramCommand: No world available");
    }

    return *this;
}

State::Any SimRunning::onEvent(const SpawnDirtBallCommand& /*cmd. */, StateMachine& dsm)
{
    // Get the current world and spawn a 5x5 ball at top center.
    if (dsm.world) {
        auto* world = dsm.world.get();

        // Calculate the top center position.
        uint32_t centerX = world->getWidth() / 2;
        uint32_t topY = 2; // Start at row 2 to avoid the very top edge.

        // Spawn a 5x5 ball of the currently selected material.
        MaterialType selectedMaterial = world->getSelectedMaterial();
        world->spawnMaterialBall(selectedMaterial, centerX, topY, 2);
    }
    else {
        spdlog::warn("SpawnDirtBallCommand: No world available");
    }

    return *this;
}

State::Any SimRunning::onEvent(const SetFragmentationCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setDirtFragmentationFactor(cmd.factor);
        spdlog::info("SimRunning: Set fragmentation factor to {}", cmd.factor);
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetPressureSystemCommand& cmd, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        world->setPressureSystem(cmd.system);
        spdlog::info("SimRunning: Set pressure system to {}", static_cast<int>(cmd.system));
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleWallsCommand& /*cmd*/, StateMachine& dsm)
{
    // Apply to world.
    if (auto* world = dsm.world.get()) {
        // TODO: Need to add toggleWalls method to WorldInterface.
        // For now, suppress unused warning.
        (void)world;
    }

    spdlog::info("SimRunning: Toggle walls");
    return *this;
}

State::Any SimRunning::onEvent(const ToggleWaterColumnCommand& /*cmd*/, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        bool newValue = !world->isWaterColumnEnabled();
        world->setWaterColumnEnabled(newValue);

        // For World, we can manipulate cells directly.
        World* worldB = dynamic_cast<World*>(world);
        if (worldB) {
            if (newValue) {
                // Add water column (5 wide × 20 tall) on left side.
                spdlog::info("SimRunning: Adding water column (5 wide × 20 tall) at runtime");
                for (uint32_t y = 0; y < 20 && y < worldB->getHeight(); ++y) {
                    for (uint32_t x = 1; x <= 5 && x < worldB->getWidth(); ++x) {
                        Cell& cell = worldB->at(x, y);
                        // Only add water to non-wall cells.
                        if (!cell.isWall()) {
                            cell.setMaterialType(MaterialType::WATER);
                            cell.setFillRatio(1.0);
                            cell.setCOM(Vector2d(0.0, 0.0));
                            cell.setVelocity(Vector2d(0.0, 0.0));
                        }
                    }
                }
            }
            else {
                // Remove water from column area (only water cells).
                spdlog::info("SimRunning: Removing water from water column area at runtime");
                for (uint32_t y = 0; y < 20 && y < worldB->getHeight(); ++y) {
                    for (uint32_t x = 1; x <= 5 && x < worldB->getWidth(); ++x) {
                        Cell& cell = worldB->at(x, y);
                        // Only clear water cells, leave walls and other materials.
                        if (cell.getMaterialType() == MaterialType::WATER && !cell.isWall()) {
                            cell.setMaterialType(MaterialType::AIR);
                            cell.setFillRatio(0.0);
                            cell.setCOM(Vector2d(0.0, 0.0));
                            cell.setVelocity(Vector2d(0.0, 0.0));
                        }
                    }
                }
            }
        }

        spdlog::info("SimRunning: Water column toggled - now: {}", newValue);
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleLeftThrowCommand& /*cmd*/, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        bool newValue = !world->isLeftThrowEnabled();
        world->setLeftThrowEnabled(newValue);
        spdlog::info("SimRunning: Toggle left throw - now: {}", newValue);
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleRightThrowCommand& /*cmd*/, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        bool newValue = !world->isRightThrowEnabled();
        world->setRightThrowEnabled(newValue);
        spdlog::info("SimRunning: Toggle right throw - now: {}", newValue);
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleQuadrantCommand& /*cmd*/, StateMachine& dsm)
{
    if (auto* world = dsm.world.get()) {
        bool newValue = !world->isLowerRightQuadrantEnabled();
        world->setLowerRightQuadrantEnabled(newValue);

        // For World, manipulate cells directly for immediate feedback.
        World* worldB = dynamic_cast<World*>(world);
        if (worldB) {
            uint32_t startX = worldB->getWidth() / 2;
            uint32_t startY = worldB->getHeight() / 2;

            if (newValue) {
                // Add dirt quadrant immediately.
                spdlog::info(
                    "SimRunning: Adding lower right quadrant ({}x{}) at runtime",
                    worldB->getWidth() - startX,
                    worldB->getHeight() - startY);
                for (uint32_t y = startY; y < worldB->getHeight(); ++y) {
                    for (uint32_t x = startX; x < worldB->getWidth(); ++x) {
                        Cell& cell = worldB->at(x, y);
                        // Only add dirt to non-wall cells.
                        if (!cell.isWall()) {
                            cell.setMaterialType(MaterialType::DIRT);
                            cell.setFillRatio(1.0);
                            cell.setCOM(Vector2d(0.0, 0.0));
                            cell.setVelocity(Vector2d(0.0, 0.0));
                        }
                    }
                }
            }
            else {
                // Remove dirt from quadrant area (only dirt cells).
                spdlog::info("SimRunning: Removing dirt from lower right quadrant at runtime");
                for (uint32_t y = startY; y < worldB->getHeight(); ++y) {
                    for (uint32_t x = startX; x < worldB->getWidth(); ++x) {
                        Cell& cell = worldB->at(x, y);
                        // Only clear dirt cells, leave walls and other materials.
                        if (cell.getMaterialType() == MaterialType::DIRT && !cell.isWall()) {
                            cell.setMaterialType(MaterialType::AIR);
                            cell.setFillRatio(0.0);
                            cell.setCOM(Vector2d(0.0, 0.0));
                            cell.setVelocity(Vector2d(0.0, 0.0));
                        }
                    }
                }
            }
        }

        spdlog::info("SimRunning: Toggle quadrant - now: {}", newValue);
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleFrameLimitCommand& /*cmd*/, StateMachine& dsm)
{
    // Apply to world.
    if (auto* world = dsm.world.get()) {
        // TODO: Need to add toggleFrameLimit method to WorldInterface.
        // For now, suppress unused warning.
        (void)world;
    }

    spdlog::info("SimRunning: Toggle frame limit");
    return *this;
}

State::Any SimRunning::onEvent(const QuitApplicationCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    spdlog::info("Server::SimRunning: Quit application requested");

    // TODO: Add CaptureScreenshotCommand that ui/StateMachine can handle.
    // Screenshots are UI concerns, not server concerns.

    // Transition to Shutdown state.
    return Shutdown{};
}

} // namespace State
} // namespace Server
} // namespace DirtSim