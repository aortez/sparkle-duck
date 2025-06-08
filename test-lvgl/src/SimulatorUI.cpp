#include "SimulatorUI.h"
#include "Cell.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

SimulatorUI::SimulatorUI(World* world, lv_obj_t* screen)
    : world_(world),
      screen_(screen),
      draw_area_(nullptr),
      mass_label_(nullptr),
      fps_label_(nullptr),
      pause_label_(nullptr),
      timescale_(1.0),
      is_paused_(false)
{}

SimulatorUI::CallbackData* SimulatorUI::createCallbackData(lv_obj_t* label)
{
    auto data = std::make_unique<CallbackData>();
    data->ui = this;
    data->world = world_;
    data->associated_label = label;
    CallbackData* ptr = data.get();
    callback_data_storage_.push_back(std::move(data));
    return ptr;
}

SimulatorUI::~SimulatorUI() = default;

void SimulatorUI::initialize()
{
    createDrawArea();
    createLabels();
    createControlButtons();
    createSliders();
    setupDrawAreaEvents();
}

void SimulatorUI::createDrawArea()
{
    draw_area_ = lv_obj_create(screen_);
    lv_obj_set_size(draw_area_, DRAW_AREA_SIZE, DRAW_AREA_SIZE);
    lv_obj_align(draw_area_, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_pad_all(draw_area_, 0, 0);
}

void SimulatorUI::createLabels()
{
    // Create mass label
    mass_label_ = lv_label_create(screen_);
    lv_label_set_text(mass_label_, "Total Mass: 0.00");
    lv_obj_align(mass_label_, LV_ALIGN_TOP_LEFT, 820, 10);

    // Create FPS label
    fps_label_ = lv_label_create(screen_);
    lv_label_set_text(fps_label_, "FPS: 0");
    lv_obj_align(fps_label_, LV_ALIGN_TOP_LEFT, 820, 40);
}

void SimulatorUI::createControlButtons()
{
    // Create reset button
    lv_obj_t* reset_btn = lv_btn_create(screen_);
    lv_obj_set_size(reset_btn, CONTROL_WIDTH, 50);
    lv_obj_align(reset_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_t* reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Reset");
    lv_obj_center(reset_label);
    lv_obj_add_event_cb(reset_btn, resetBtnEventCb, LV_EVENT_CLICKED, world_);

    // Create pause button
    lv_obj_t* pause_btn = lv_btn_create(screen_);
    lv_obj_set_size(pause_btn, CONTROL_WIDTH, 50);
    lv_obj_align(pause_btn, LV_ALIGN_TOP_RIGHT, -10, 70);
    pause_label_ = lv_label_create(pause_btn);
    lv_label_set_text(pause_label_, "Pause");
    lv_obj_center(pause_label_);
    lv_obj_add_event_cb(pause_btn, pauseBtnEventCb, LV_EVENT_CLICKED, createCallbackData());

    // Create debug toggle button
    lv_obj_t* debug_btn = lv_btn_create(screen_);
    lv_obj_set_size(debug_btn, CONTROL_WIDTH, 50);
    lv_obj_align(debug_btn, LV_ALIGN_TOP_RIGHT, -10, 130);
    lv_obj_t* debug_label = lv_label_create(debug_btn);
    lv_label_set_text(debug_label, "Debug: Off");
    lv_obj_center(debug_label);
    lv_obj_add_event_cb(debug_btn, debugBtnEventCb, LV_EVENT_CLICKED, nullptr);

    // Create cursor force toggle button
    lv_obj_t* force_btn = lv_btn_create(screen_);
    lv_obj_set_size(force_btn, CONTROL_WIDTH, 50);
    lv_obj_align(force_btn, LV_ALIGN_TOP_RIGHT, -10, 190);
    lv_obj_t* force_label = lv_label_create(force_btn);
    lv_label_set_text(force_label, "Force: Off");
    lv_obj_center(force_label);
    lv_obj_add_event_cb(force_btn, forceBtnEventCb, LV_EVENT_CLICKED, world_);

    // Create gravity toggle button
    lv_obj_t* gravity_btn = lv_btn_create(screen_);
    lv_obj_set_size(gravity_btn, CONTROL_WIDTH, 50);
    lv_obj_align(gravity_btn, LV_ALIGN_TOP_RIGHT, -10, 250);
    lv_obj_t* gravity_label = lv_label_create(gravity_btn);
    lv_label_set_text(gravity_label, "Gravity: On");
    lv_obj_center(gravity_label);
    lv_obj_add_event_cb(gravity_btn, gravityBtnEventCb, LV_EVENT_CLICKED, world_);

    // Create quit button
    lv_obj_t* quit_btn = lv_btn_create(screen_);
    lv_obj_set_size(quit_btn, CONTROL_WIDTH, 50);
    lv_obj_align(quit_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(quit_btn, lv_color_hex(0xFF0000), 0);
    lv_obj_t* quit_label = lv_label_create(quit_btn);
    lv_label_set_text(quit_label, "Quit");
    lv_obj_center(quit_label);
    lv_obj_add_event_cb(quit_btn, quitBtnEventCb, LV_EVENT_CLICKED, nullptr);
}

void SimulatorUI::createSliders()
{
    // Timescale slider
    lv_obj_t* slider_label = lv_label_create(screen_);
    lv_label_set_text(slider_label, "Timescale");
    lv_obj_align(slider_label, LV_ALIGN_TOP_RIGHT, -10, 290);

    lv_obj_t* timescale_value_label = lv_label_create(screen_);
    lv_label_set_text(timescale_value_label, "1.0x");
    lv_obj_align(timescale_value_label, LV_ALIGN_TOP_RIGHT, -120, 290);

    lv_obj_t* slider = lv_slider_create(screen_);
    lv_obj_set_size(slider, CONTROL_WIDTH, 10);
    lv_obj_align(slider, LV_ALIGN_TOP_RIGHT, -10, 310);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(
        slider, timescaleSliderEventCb, LV_EVENT_ALL, createCallbackData(timescale_value_label));

    // Elasticity slider
    lv_obj_t* elasticity_label = lv_label_create(screen_);
    lv_label_set_text(elasticity_label, "Elasticity");
    lv_obj_align(elasticity_label, LV_ALIGN_TOP_RIGHT, -10, 330);

    lv_obj_t* elasticity_value_label = lv_label_create(screen_);
    lv_label_set_text(elasticity_value_label, "0.8");
    lv_obj_align(elasticity_value_label, LV_ALIGN_TOP_RIGHT, -120, 330);

    lv_obj_t* elasticity_slider = lv_slider_create(screen_);
    lv_obj_set_size(elasticity_slider, CONTROL_WIDTH, 10);
    lv_obj_align(elasticity_slider, LV_ALIGN_TOP_RIGHT, -10, 350);
    lv_slider_set_range(elasticity_slider, 0, 200);
    lv_slider_set_value(elasticity_slider, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(
        elasticity_slider,
        elasticitySliderEventCb,
        LV_EVENT_ALL,
        createCallbackData(elasticity_value_label));

    // Dirt fragmentation slider
    lv_obj_t* fragmentation_label = lv_label_create(screen_);
    lv_label_set_text(fragmentation_label, "Dirt Fragmentation");
    lv_obj_align(fragmentation_label, LV_ALIGN_TOP_RIGHT, -10, 370);

    lv_obj_t* fragmentation_value_label = lv_label_create(screen_);
    lv_label_set_text(fragmentation_value_label, "0.00");
    lv_obj_align(fragmentation_value_label, LV_ALIGN_TOP_RIGHT, -165, 370);

    lv_obj_t* fragmentation_slider = lv_slider_create(screen_);
    lv_obj_set_size(fragmentation_slider, CONTROL_WIDTH, 10);
    lv_obj_align(fragmentation_slider, LV_ALIGN_TOP_RIGHT, -10, 390);
    lv_slider_set_range(fragmentation_slider, 0, 100);
    lv_slider_set_value(fragmentation_slider, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(
        fragmentation_slider,
        fragmentationSliderEventCb,
        LV_EVENT_ALL,
        createCallbackData(fragmentation_value_label));

    // Cell size slider
    lv_obj_t* cell_size_label = lv_label_create(screen_);
    lv_label_set_text(cell_size_label, "Cell Size");
    lv_obj_align(cell_size_label, LV_ALIGN_TOP_RIGHT, -10, 410);

    lv_obj_t* cell_size_value_label = lv_label_create(screen_);
    lv_label_set_text(cell_size_value_label, "50");
    lv_obj_align(cell_size_value_label, LV_ALIGN_TOP_RIGHT, -120, 410);

    lv_obj_t* cell_size_slider = lv_slider_create(screen_);
    lv_obj_set_size(cell_size_slider, CONTROL_WIDTH, 10);
    lv_obj_align(cell_size_slider, LV_ALIGN_TOP_RIGHT, -10, 430);
    lv_slider_set_range(cell_size_slider, 10, 50);
    lv_slider_set_value(cell_size_slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(
        cell_size_slider,
        cellSizeSliderEventCb,
        LV_EVENT_ALL,
        createCallbackData(cell_size_value_label));

    // Pressure scale slider
    lv_obj_t* pressure_label = lv_label_create(screen_);
    lv_label_set_text(pressure_label, "Pressure Scale");
    lv_obj_align(pressure_label, LV_ALIGN_TOP_RIGHT, -10, 450);

    lv_obj_t* pressure_value_label = lv_label_create(screen_);
    lv_label_set_text(pressure_value_label, "1.0");
    lv_obj_align(pressure_value_label, LV_ALIGN_TOP_RIGHT, -120, 450);

    lv_obj_t* pressure_slider = lv_slider_create(screen_);
    lv_obj_set_size(pressure_slider, CONTROL_WIDTH, 10);
    lv_obj_align(pressure_slider, LV_ALIGN_TOP_RIGHT, -10, 470);
    lv_slider_set_range(pressure_slider, 0, 200);
    lv_slider_set_value(pressure_slider, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(
        pressure_slider,
        pressureSliderEventCb,
        LV_EVENT_ALL,
        createCallbackData(pressure_value_label));
}

void SimulatorUI::setupDrawAreaEvents()
{
    lv_obj_add_event_cb(draw_area_, drawAreaEventCb, LV_EVENT_PRESSED, world_);
    lv_obj_add_event_cb(draw_area_, drawAreaEventCb, LV_EVENT_PRESSING, world_);
    lv_obj_add_event_cb(draw_area_, drawAreaEventCb, LV_EVENT_RELEASED, world_);
}

void SimulatorUI::updateMassLabel(double totalMass)
{
    if (mass_label_) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Total Mass: %.2f", totalMass);
        lv_label_set_text(mass_label_, buf);
    }
}

void SimulatorUI::updateFPSLabel(uint32_t fps)
{
    if (fps_label_) {
        char buf[32];
        snprintf(buf, sizeof(buf), "FPS: %u", fps);
        lv_label_set_text(fps_label_, buf);
    }
}

// Static event callback implementations
void SimulatorUI::drawAreaEventCb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    World* world_ptr = static_cast<World*>(lv_event_get_user_data(e));
    if (!world_ptr) return;

    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);

    lv_area_t area;
    lv_obj_get_coords(static_cast<lv_obj_t*>(lv_event_get_target(e)), &area);

    point.x -= area.x1;
    point.y -= area.y1;

    if (code == LV_EVENT_PRESSED) {
        world_ptr->addWaterAtPixel(point.x, point.y);
        world_ptr->startDragging(point.x, point.y);
        world_ptr->updateCursorForce(point.x, point.y, true);
    }
    else if (code == LV_EVENT_PRESSING) {
        world_ptr->updateDrag(point.x, point.y);
        world_ptr->updateCursorForce(point.x, point.y, true);
    }
    else if (code == LV_EVENT_RELEASED) {
        world_ptr->endDragging(point.x, point.y);
        world_ptr->clearCursorForce();
    }
}

void SimulatorUI::pauseBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->ui) {
            data->ui->is_paused_ = !data->ui->is_paused_;
            lv_label_set_text(data->ui->pause_label_, data->ui->is_paused_ ? "Resume" : "Pause");
            if (data->world) {
                data->world->setTimescale(data->ui->is_paused_ ? 0.0 : data->ui->timescale_);
            }
        }
    }
}

void SimulatorUI::timescaleSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double timescale = pow(10.0, (value - 50) / 50.0);
        data->ui->timescale_ = timescale;
        if (data->world && !data->ui->is_paused_) {
            data->world->setTimescale(timescale);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2fx", timescale);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::resetBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        World* world_ptr = static_cast<World*>(lv_event_get_user_data(e));
        if (world_ptr) {
            world_ptr->reset();
        }
    }
}

void SimulatorUI::debugBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        Cell::debugDraw = !Cell::debugDraw;
        const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
        lv_obj_t* label = lv_obj_get_child(btn, 0);
        lv_label_set_text(label, Cell::debugDraw ? "Debug: On" : "Debug: Off");
    }
}

void SimulatorUI::forceBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        World* world_ptr = static_cast<World*>(lv_event_get_user_data(e));
        if (world_ptr) {
            // Toggle cursor force
            static bool force_enabled = false;
            force_enabled = !force_enabled;
            world_ptr->setCursorForceEnabled(force_enabled);
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(label, force_enabled ? "Force: On" : "Force: Off");
        }
    }
}

void SimulatorUI::gravityBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        World* world_ptr = static_cast<World*>(lv_event_get_user_data(e));
        if (world_ptr) {
            static bool gravity_enabled = true;
            gravity_enabled = !gravity_enabled;
            world_ptr->setGravity(gravity_enabled ? 9.81 : 0.0);
            const lv_obj_t* btn = static_cast<const lv_obj_t*>(lv_event_get_target(e));
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            lv_label_set_text(label, gravity_enabled ? "Gravity: On" : "Gravity: Off");
        }
    }
}

void SimulatorUI::elasticitySliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double elasticity = value / 100.0;
        if (data->world) {
            data->world->setElasticityFactor(elasticity);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", elasticity);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::fragmentationSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double fragmentation_factor = value / 100.0;
        if (data->world) {
            data->world->setDirtFragmentationFactor(fragmentation_factor);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", fragmentation_factor);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::pressureSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        double pressure_scale = value / 100.0;
        if (data->world) {
            data->world->setPressureScale(pressure_scale);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", pressure_scale);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::cellSizeSliderEventCb(lv_event_t* e)
{
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    CallbackData* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED && data) {
        int32_t value = lv_slider_get_value(slider);
        Cell::setSize(value);
        // Note: Cell size change logic from main.cpp would need to be moved here
        // For now, just update the label
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        lv_label_set_text(data->associated_label, buf);
    }
}

void SimulatorUI::quitBtnEventCb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        exit(0);
    }
}
