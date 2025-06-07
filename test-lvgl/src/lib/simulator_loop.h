#pragma once

#include "../World.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/misc/lv_timer.h"
#include <chrono>
#include <cstdio>

// Forward declarations
extern lv_obj_t* mass_label_ptr;
extern lv_obj_t* fps_label_ptr;

namespace SimulatorLoop {

// Shared simulation loop state
struct LoopState {
    using clock = std::chrono::steady_clock;
    clock::time_point last_dump;
    uint32_t frame_count = 0;
    uint32_t last_fps_update = 0;
    uint32_t fps = 0;
    bool is_running = true;
    bool needs_redraw = false;
    bool paused = false;
    uint32_t step_count = 0;     // Number of simulation steps executed
    uint32_t max_steps = 0;      // Maximum steps to run (0 = unlimited)
};

struct TimerUserData {
    World* world;
    LoopState* state;
    uint32_t period;
    TimerUserData(World* w, LoopState* s, uint32_t p) : world(w), state(s), period(p) {}
};

struct EventContext {
    LoopState* state;
    World* world;
    lv_obj_t* pause_button;
    lv_obj_t* reset_button;
    lv_obj_t* pause_label;
};

// Initialize the loop state
inline void initState(LoopState& state) {
    state.last_dump = LoopState::clock::now();
    state.frame_count = 0;
    state.last_fps_update = 0;
    state.fps = 0;
    state.is_running = true;
    state.needs_redraw = false;
    state.paused = false;
    state.step_count = 0;
    state.max_steps = 0;
}

// Process one frame of simulation
inline void processFrame(World& world, LoopState& state, uint32_t delta_time_ms = 16) {
    // Check if we've reached the step limit
    if (state.max_steps > 0 && state.step_count >= state.max_steps) {
        printf("Simulation completed after %u steps\n", state.step_count);
        state.is_running = false;
        return;
    }

    // Increment step counter
    state.step_count++;

    // Advance simulation
    world.advanceTime(delta_time_ms * world.timescale * 0.001);
    
    // Always draw every frame to avoid flicker
    world.draw();

    // Update mass label if it exists
    if (mass_label_ptr) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Total Mass: %.2f", world.getTotalMass());
        lv_label_set_text(mass_label_ptr, buf);
    }

    // Update FPS counter
    state.frame_count++;
    uint32_t current_time = lv_tick_get();
    if (current_time - state.last_fps_update >= 1000) { // Update every second
        state.fps = state.frame_count;
        state.frame_count = 0;
        state.last_fps_update = current_time;
        
        if (fps_label_ptr) {
            char buf[32];
            snprintf(buf, sizeof(buf), "FPS: %d", state.fps);
            lv_label_set_text(fps_label_ptr, buf);
        }
    }

    // Periodically dump timer stats every 10 seconds
    auto now = LoopState::clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - state.last_dump).count() >= 10) {
        world.dumpTimerStats();
        state.last_dump = now;
    }
}

// Create an event-driven simulation timer
inline lv_timer_t* createSimulationTimer(World& world, LoopState& state, uint32_t period_ms = 16) {
    TimerUserData* user_data = new TimerUserData(&world, &state, period_ms);
    return lv_timer_create([](lv_timer_t* timer) {
        TimerUserData* user_data = static_cast<TimerUserData*>(lv_timer_get_user_data(timer));
        processFrame(*user_data->world, *user_data->state, user_data->period);
    }, period_ms, user_data);
}

// Mark that a redraw is needed
inline void requestRedraw(LoopState& state) {
    state.needs_redraw = true;
}

static void event_handler(lv_event_t * e) {
    auto* ctx = static_cast<EventContext*>(lv_event_get_user_data(e));
    auto* state = ctx ? ctx->state : nullptr;
    auto* world = ctx ? ctx->world : nullptr;
    lv_obj_t* pause_button = ctx ? ctx->pause_button : nullptr;
    lv_obj_t* reset_button = ctx ? ctx->reset_button : nullptr;
    lv_obj_t* pause_label = ctx ? ctx->pause_label : nullptr;
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target_obj(e);
    
    // Debug logging for mouse events
    switch(code) {
        case LV_EVENT_PRESSED:
            LV_LOG_USER("Mouse PRESSED on object %p", obj);
            break;
        case LV_EVENT_PRESSING:
            LV_LOG_USER("Mouse PRESSING on object %p", obj);
            break;
        case LV_EVENT_PRESS_LOST:
            LV_LOG_USER("Mouse PRESS_LOST on object %p", obj);
            break;
        case LV_EVENT_CLICKED:
            LV_LOG_USER("Mouse CLICKED on object %p", obj);
            break;
        case LV_EVENT_RELEASED:
            LV_LOG_USER("Mouse RELEASED on object %p", obj);
            break;
        case LV_EVENT_SCROLL_BEGIN:
            LV_LOG_USER("Mouse SCROLL_BEGIN on object %p", obj);
            break;
        case LV_EVENT_SCROLL:
            LV_LOG_USER("Mouse SCROLL on object %p", obj);
            break;
        case LV_EVENT_SCROLL_END:
            LV_LOG_USER("Mouse SCROLL_END on object %p", obj);
            break;
        default:
            LV_LOG_USER("Other event %d on object %p", code, obj);
            break;
    }

    // Handle simulation control events
    if(code == LV_EVENT_CLICKED && state && world) {
        if(obj == pause_button) {
            LV_LOG_USER("Pause button clicked");
            state->paused = !state->paused;
            lv_label_set_text(pause_label, state->paused ? "Resume" : "Pause");
        }
        else if(obj == reset_button) {
            LV_LOG_USER("Reset button clicked");
            world->reset();
            state->paused = true;
            lv_label_set_text(pause_label, "Resume");
        }
    }
}

} // namespace SimulatorLoop 
