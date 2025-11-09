#pragma once

#include "lvgl/lvgl.h"

namespace DirtSim {
namespace Ui {

// Forward declarations.
class WebSocketClient;

/**
 * @brief Physics parameter controls for tuning simulation behavior.
 *
 * Provides toggle sliders for: timescale, gravity, elasticity, air resistance,
 * pressure systems (hydrostatic, dynamic, diffusion), and material forces
 * (cohesion, adhesion, viscosity, friction).
 */
class PhysicsControls {
public:
    PhysicsControls(lv_obj_t* container, WebSocketClient* wsClient);
    ~PhysicsControls();

private:
    lv_obj_t* container_;
    WebSocketClient* wsClient_;

    // Column containers.
    lv_obj_t* column1_ = nullptr;
    lv_obj_t* column2_ = nullptr;
    lv_obj_t* column3_ = nullptr;

    // Column 1: General Physics widgets.
    lv_obj_t* timescaleControl_ = nullptr;
    lv_obj_t* gravityControl_ = nullptr;
    lv_obj_t* elasticityControl_ = nullptr;
    lv_obj_t* airResistanceControl_ = nullptr;

    // Column 2: Pressure widgets.
    lv_obj_t* hydrostaticPressureControl_ = nullptr;
    lv_obj_t* dynamicPressureControl_ = nullptr;
    lv_obj_t* pressureDiffusionControl_ = nullptr;

    // Column 3: Forces widgets.
    lv_obj_t* cohesionForceControl_ = nullptr;
    lv_obj_t* adhesionControl_ = nullptr;
    lv_obj_t* viscosityControl_ = nullptr;
    lv_obj_t* frictionControl_ = nullptr;

    // Event handlers for Column 1 (General Physics).
    static void onTimescaleToggled(lv_event_t* e);
    static void onTimescaleChanged(lv_event_t* e);
    static void onGravityToggled(lv_event_t* e);
    static void onGravityChanged(lv_event_t* e);
    static void onElasticityToggled(lv_event_t* e);
    static void onElasticityChanged(lv_event_t* e);
    static void onAirResistanceToggled(lv_event_t* e);
    static void onAirResistanceChanged(lv_event_t* e);

    // Event handlers for Column 2 (Pressure).
    static void onHydrostaticPressureToggled(lv_event_t* e);
    static void onHydrostaticPressureChanged(lv_event_t* e);
    static void onDynamicPressureToggled(lv_event_t* e);
    static void onDynamicPressureChanged(lv_event_t* e);
    static void onPressureDiffusionToggled(lv_event_t* e);
    static void onPressureDiffusionChanged(lv_event_t* e);

    // Event handlers for Column 3 (Forces).
    static void onCohesionForceToggled(lv_event_t* e);
    static void onCohesionForceChanged(lv_event_t* e);
    static void onAdhesionToggled(lv_event_t* e);
    static void onAdhesionChanged(lv_event_t* e);
    static void onViscosityToggled(lv_event_t* e);
    static void onViscosityChanged(lv_event_t* e);
    static void onFrictionToggled(lv_event_t* e);
    static void onFrictionChanged(lv_event_t* e);

    /**
     * @brief Send physics parameter command to server.
     */
    void sendPhysicsCommand(const char* commandName, double value);
};

} // namespace Ui
} // namespace DirtSim
