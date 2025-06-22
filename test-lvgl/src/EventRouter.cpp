#include "EventRouter.h"
#include "DirtSimStateMachine.h"
#include <spdlog/spdlog.h>

void EventRouter::processImmediateEvent(const GetFPSCommand& /*cmd*/) {
    // Get current FPS from shared state
    float fps = sharedState_.getCurrentFPS();
    
    spdlog::info("Processing GetFPSCommand - Current FPS: {:.1f}", fps);
    
    // TODO: If this command has a callback, send the response
    // For now, just log the value
}

void EventRouter::processImmediateEvent(const GetSimStatsCommand& /*cmd*/) {
    // Get simulation statistics from shared state
    auto stats = sharedState_.getStats();
    
    spdlog::info("Processing GetSimStatsCommand - Total cells: {}, Active cells: {}, Step: {}", 
                 stats.totalCells, stats.activeCells, stats.stepCount);
    
    // TODO: If this command has a callback, send the response
    // For now, just log the stats
}

void EventRouter::processImmediateEvent(const PauseCommand& /*cmd*/) {
    // Set pause state
    bool wasPaused = sharedState_.getIsPaused();
    sharedState_.setIsPaused(true);
    
    spdlog::info("Processing PauseCommand - Was paused: {}, Now paused: true", wasPaused);
    
    // Note: The actual pausing of the simulation loop happens
    // when the simulation thread checks the pause state
}

void EventRouter::processImmediateEvent(const ResumeCommand& /*cmd*/) {
    // Clear pause state
    bool wasPaused = sharedState_.getIsPaused();
    sharedState_.setIsPaused(false);
    
    spdlog::info("Processing ResumeCommand - Was paused: {}, Now paused: false", wasPaused);
    
    // Note: The actual resuming of the simulation loop happens
    // when the simulation thread checks the pause state
}