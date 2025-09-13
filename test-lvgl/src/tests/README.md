# Testing Guide for LVGL-based Components

This guide explains how to write tests for components that use LVGL (Light and Versatile Graphics Library).

## Key Requirements for LVGL Tests

### 1. Display Initialization is Mandatory

LVGL requires a display to exist before any UI objects can be created. Without a display, you'll encounter errors like:
```
[Error] lv_obj_create: No display created yet. No place to assign the new screen
```

**Solution**: Always create a display before creating any LVGL objects. Use the `LVGLTestBase` fixture which handles this automatically.

### 2. Time Management for Timer Tests

LVGL uses an internal tick counter for timing operations. In tests, real time doesn't advance automatically, so you must:
- Call `lv_tick_inc(milliseconds)` to advance LVGL's internal time
- Call `lv_timer_handler()` to process timers and events

**Without tick increments, timer-based tests will hang or fail.**

## Using LVGLTestBase

The `LVGLTestBase` class provides a reusable test fixture that handles LVGL initialization:

```cpp
#include "LVGLTestBase.h"

class MyTest : public LVGLTestBase {
protected:
    void SetUp() override {
        LVGLTestBase::SetUp();  // Creates display and screen
        // Your additional setup here
    }
};

TEST_F(MyTest, TestSomething) {
    // Create UI elements - display and screen already exist
    lv_obj_t* button = lv_btn_create(screen_);
    
    // Run LVGL for 100ms (processes events and timers)
    runLVGL(100);
    
    // Your test assertions here
}
```

## Common Testing Patterns

### Testing Timer-based Functionality

```cpp
TEST_F(MyTest, TimerTest) {
    // Create component with timer
    MyComponent component(screen_);
    
    // Run LVGL for specific duration
    runLVGL(1000);  // Run for 1 second
    
    // Or run until condition is met (with timeout)
    bool success = runLVGLUntil(
        [&]() { return component.isReady(); },
        5000  // 5 second timeout
    );
    EXPECT_TRUE(success);
}
```

### Testing UI Event Handling

```cpp
TEST_F(MyTest, ButtonClickTest) {
    // Create button with event handler
    lv_obj_t* btn = lv_btn_create(screen_);
    lv_obj_add_event_cb(btn, my_event_handler, LV_EVENT_CLICKED, nullptr);
    
    // Simulate button click
    lv_event_send(btn, LV_EVENT_CLICKED, nullptr);
    
    // Process events
    runLVGL(10);
    
    // Verify handler was called
}
```

## Writing Your Own LVGL Test Fixture

If `LVGLTestBase` doesn't meet your needs, here's the minimal setup required:

```cpp
class CustomLVGLTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 1. Initialize LVGL
        lv_init();
        
        // 2. Create display (REQUIRED!)
        display_ = lv_display_create(width, height);
        
        // 3. Set up display buffer
        static lv_color_t buf[width * 10];
        lv_display_set_buffers(display_, buf, NULL, sizeof(buf), 
                              LV_DISPLAY_RENDER_MODE_PARTIAL);
        
        // 4. Set flush callback (required even if it does nothing)
        lv_display_set_flush_cb(display_, flush_cb);
        
        // 5. Create and load screen
        screen_ = lv_obj_create(NULL);
        lv_scr_load(screen_);
    }
    
    void TearDown() override {
        lv_obj_del(screen_);
        lv_display_delete(display_);
        lv_deinit();
    }
    
private:
    static void flush_cb(lv_display_t* disp, const lv_area_t*, uint8_t*) {
        lv_display_flush_ready(disp);
    }
    
    lv_display_t* display_;
    lv_obj_t* screen_;
};
```

## Troubleshooting

### Test Hangs
- Ensure you're calling `lv_tick_inc()` before `lv_timer_handler()`
- Check for infinite loops in event handlers
- Verify display is properly initialized

### "No display created" Errors
- Ensure display is created before any UI objects
- Check that `lv_init()` is called before display creation
- Verify display isn't deleted while UI objects still exist

### Timer Callbacks Not Firing
- Verify `lv_tick_inc()` is being called to advance time
- Check that timer was created with correct period
- Ensure `lv_timer_handler()` is being called

## Best Practices

1. **Use LVGLTestBase** for consistency and to avoid boilerplate
2. **Keep displays small** (100x100) for tests - they're not rendered anyway
3. **Clean up in reverse order** - delete UI objects before display
4. **Use meaningful test names** that describe what's being tested
5. **Test one thing at a time** - create focused, isolated tests
6. **Mock external dependencies** rather than using real implementations