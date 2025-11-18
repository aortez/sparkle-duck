#include "JuliaFractal.h"
#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

// Rendering performance.
// Detect hardware concurrency: use n/2 cores (capped at 8), fallback to 1 if detection fails.
const int RENDER_THREADS = []() {
    unsigned int hwThreads = std::thread::hardware_concurrency();
    if (hwThreads == 0) {
        spdlog::warn("JuliaFractal: hardware_concurrency() failed, using 1 thread");
        return 1;
    }
    int threads = static_cast<int>(hwThreads / 2);
    threads = std::min(threads, 8);  // Cap at 8 threads.
    threads = std::max(threads, 1);  // Minimum 1 thread.
    spdlog::info("JuliaFractal: Detected {} hardware threads, using {} render threads", hwThreads, threads);
    return threads;
}();
constexpr double MAX_CYCLE_SPEED = 0.05; // Maximum palette advance per frame.

// Palette extracted from pal.png (256x1).
constexpr int PALETTE_SIZE = 256;
constexpr uint32_t PALETTE[PALETTE_SIZE] = {
    0xFF000000, // 0: RGB(0,0,0)
    0xFF000000, // 1: RGB(0,0,0)
    0xFF040000, // 2: RGB(4,0,0)
    0xFF0C0000, // 3: RGB(12,0,0)
    0xFF100000, // 4: RGB(16,0,0)
    0xFF180000, // 5: RGB(24,0,0)
    0xFF200000, // 6: RGB(32,0,0)
    0xFF240000, // 7: RGB(36,0,0)
    0xFF2C0000, // 8: RGB(44,0,0)
    0xFF300000, // 9: RGB(48,0,0)
    0xFF380000, // 10: RGB(56,0,0)
    0xFF400000, // 11: RGB(64,0,0)
    0xFF440000, // 12: RGB(68,0,0)
    0xFF4C0000, // 13: RGB(76,0,0)
    0xFF500000, // 14: RGB(80,0,0)
    0xFF580000, // 15: RGB(88,0,0)
    0xFF600000, // 16: RGB(96,0,0)
    0xFF640000, // 17: RGB(100,0,0)
    0xFF6C0000, // 18: RGB(108,0,0)
    0xFF740000, // 19: RGB(116,0,0)
    0xFF780000, // 20: RGB(120,0,0)
    0xFF800000, // 21: RGB(128,0,0)
    0xFF840000, // 22: RGB(132,0,0)
    0xFF8C0000, // 23: RGB(140,0,0)
    0xFF940000, // 24: RGB(148,0,0)
    0xFF980000, // 25: RGB(152,0,0)
    0xFFA00000, // 26: RGB(160,0,0)
    0xFFA40000, // 27: RGB(164,0,0)
    0xFFAC0000, // 28: RGB(172,0,0)
    0xFFB40000, // 29: RGB(180,0,0)
    0xFFB80000, // 30: RGB(184,0,0)
    0xFFC00000, // 31: RGB(192,0,0)
    0xFFC80000, // 32: RGB(200,0,0)
    0xFFC80400, // 33: RGB(200,4,0)
    0xFFC80C00, // 34: RGB(200,12,0)
    0xFFCC1000, // 35: RGB(204,16,0)
    0xFFCC1800, // 36: RGB(204,24,0)
    0xFFD01C00, // 37: RGB(208,28,0)
    0xFFD02400, // 38: RGB(208,36,0)
    0xFFD02800, // 39: RGB(208,40,0)
    0xFFD43000, // 40: RGB(212,48,0)
    0xFFD43800, // 41: RGB(212,56,0)
    0xFFD83C00, // 42: RGB(216,60,0)
    0xFFD84400, // 43: RGB(216,68,0)
    0xFFD84800, // 44: RGB(216,72,0)
    0xFFDC5000, // 45: RGB(220,80,0)
    0xFFDC5400, // 46: RGB(220,84,0)
    0xFFE05C00, // 47: RGB(224,92,0)
    0xFFE06400, // 48: RGB(224,100,0)
    0xFFE06800, // 49: RGB(224,104,0)
    0xFFE47000, // 50: RGB(228,112,0)
    0xFFE47400, // 51: RGB(228,116,0)
    0xFFE87C00, // 52: RGB(232,124,0)
    0xFFE88000, // 53: RGB(232,128,0)
    0xFFE88800, // 54: RGB(232,136,0)
    0xFFEC8C00, // 55: RGB(236,140,0)
    0xFFEC9400, // 56: RGB(236,148,0)
    0xFFF09C00, // 57: RGB(240,156,0)
    0xFFF0A000, // 58: RGB(240,160,0)
    0xFFF0A800, // 59: RGB(240,168,0)
    0xFFF4AC00, // 60: RGB(244,172,0)
    0xFFF4B400, // 61: RGB(244,180,0)
    0xFFF8B800, // 62: RGB(248,184,0)
    0xFFF8C000, // 63: RGB(248,192,0)
    0xFFFCC800, // 64: RGB(252,200,0)
    0xFFFCC804, // 65: RGB(252,200,4)
    0xFFFCC80C, // 66: RGB(252,200,12)
    0xFFFCCC14, // 67: RGB(252,204,20)
    0xFFFCCC1C, // 68: RGB(252,204,28)
    0xFFFCD024, // 69: RGB(252,208,36)
    0xFFFCD02C, // 70: RGB(252,208,44)
    0xFFFCD034, // 71: RGB(252,208,52)
    0xFFFCD43C, // 72: RGB(252,212,60)
    0xFFFCD444, // 73: RGB(252,212,68)
    0xFFFCD84C, // 74: RGB(252,216,76)
    0xFFFCD854, // 75: RGB(252,216,84)
    0xFFFCD85C, // 76: RGB(252,216,92)
    0xFFFCDC64, // 77: RGB(252,220,100)
    0xFFFCDC6C, // 78: RGB(252,220,108)
    0xFFFCE074, // 79: RGB(252,224,116)
    0xFFFCE07C, // 80: RGB(252,224,124)
    0xFFFCE084, // 81: RGB(252,224,132)
    0xFFFCE48C, // 82: RGB(252,228,140)
    0xFFFCE494, // 83: RGB(252,228,148)
    0xFFFCE89C, // 84: RGB(252,232,156)
    0xFFFCE8A4, // 85: RGB(252,232,164)
    0xFFFCE8AC, // 86: RGB(252,232,172)
    0xFFFCECB4, // 87: RGB(252,236,180)
    0xFFFCECBC, // 88: RGB(252,236,188)
    0xFFFCF0C4, // 89: RGB(252,240,196)
    0xFFFCF0CC, // 90: RGB(252,240,204)
    0xFFFCF0D4, // 91: RGB(252,240,212)
    0xFFFCF4DC, // 92: RGB(252,244,220)
    0xFFFCF4E4, // 93: RGB(252,244,228)
    0xFFFCF8EC, // 94: RGB(252,248,236)
    0xFFFCF8F4, // 95: RGB(252,248,244)
    0xFFFCFCFC, // 96: RGB(252,252,252)
    0xFFFCFCF8, // 97: RGB(252,252,248)
    0xFFFCFCF4, // 98: RGB(252,252,244)
    0xFFFCFCF0, // 99: RGB(252,252,240)
    0xFFFCFCE8, // 100: RGB(252,252,232)
    0xFFFCFCE4, // 101: RGB(252,252,228)
    0xFFFCFCE0, // 102: RGB(252,252,224)
    0xFFFCFCD8, // 103: RGB(252,252,216)
    0xFFFCFCD4, // 104: RGB(252,252,212)
    0xFFFCFCD0, // 105: RGB(252,252,208)
    0xFFFCFCC8, // 106: RGB(252,252,200)
    0xFFFCFCC4, // 107: RGB(252,252,196)
    0xFFFCFCC0, // 108: RGB(252,252,192)
    0xFFFCFCB8, // 109: RGB(252,252,184)
    0xFFFCFCB4, // 110: RGB(252,252,180)
    0xFFFCFCB0, // 111: RGB(252,252,176)
    0xFFFCFCA8, // 112: RGB(252,252,168)
    0xFFFCFCA4, // 113: RGB(252,252,164)
    0xFFFCFCA0, // 114: RGB(252,252,160)
    0xFFFCFC9C, // 115: RGB(252,252,156)
    0xFFFCFC94, // 116: RGB(252,252,148)
    0xFFFCFC90, // 117: RGB(252,252,144)
    0xFFFCFC8C, // 118: RGB(252,252,140)
    0xFFFCFC84, // 119: RGB(252,252,132)
    0xFFFCFC80, // 120: RGB(252,252,128)
    0xFFFCFC7C, // 121: RGB(252,252,124)
    0xFFFCFC74, // 122: RGB(252,252,116)
    0xFFFCFC70, // 123: RGB(252,252,112)
    0xFFFCFC6C, // 124: RGB(252,252,108)
    0xFFFCFC64, // 125: RGB(252,252,100)
    0xFFFCFC60, // 126: RGB(252,252,96)
    0xFFFCFC5C, // 127: RGB(252,252,92)
    0xFFFCFC54, // 128: RGB(252,252,84)
    0xFFFCFC50, // 129: RGB(252,252,80)
    0xFFFCFC4C, // 130: RGB(252,252,76)
    0xFFFCFC48, // 131: RGB(252,252,72)
    0xFFFCFC40, // 132: RGB(252,252,64)
    0xFFFCFC3C, // 133: RGB(252,252,60)
    0xFFFCFC38, // 134: RGB(252,252,56)
    0xFFFCFC30, // 135: RGB(252,252,48)
    0xFFFCFC2C, // 136: RGB(252,252,44)
    0xFFFCFC28, // 137: RGB(252,252,40)
    0xFFFCFC20, // 138: RGB(252,252,32)
    0xFFFCFC1C, // 139: RGB(252,252,28)
    0xFFFCFC18, // 140: RGB(252,252,24)
    0xFFFCFC10, // 141: RGB(252,252,16)
    0xFFFCFC0C, // 142: RGB(252,252,12)
    0xFFFCFC08, // 143: RGB(252,252,8)
    0xFFFCFC00, // 144: RGB(252,252,0)
    0xFFFCF800, // 145: RGB(252,248,0)
    0xFFFCF400, // 146: RGB(252,244,0)
    0xFFFCF000, // 147: RGB(252,240,0)
    0xFFFCE800, // 148: RGB(252,232,0)
    0xFFFCE400, // 149: RGB(252,228,0)
    0xFFFCE000, // 150: RGB(252,224,0)
    0xFFFCD800, // 151: RGB(252,216,0)
    0xFFFCD400, // 152: RGB(252,212,0)
    0xFFFCD000, // 153: RGB(252,208,0)
    0xFFFCC800, // 154: RGB(252,200,0)
    0xFFFCC400, // 155: RGB(252,196,0)
    0xFFFCC000, // 156: RGB(252,192,0)
    0xFFFCB800, // 157: RGB(252,184,0)
    0xFFFCB400, // 158: RGB(252,180,0)
    0xFFFCB000, // 159: RGB(252,176,0)
    0xFFFCA800, // 160: RGB(252,168,0)
    0xFFFCA400, // 161: RGB(252,164,0)
    0xFFFCA000, // 162: RGB(252,160,0)
    0xFFFC9C00, // 163: RGB(252,156,0)
    0xFFFC9400, // 164: RGB(252,148,0)
    0xFFFC9000, // 165: RGB(252,144,0)
    0xFFFC8C00, // 166: RGB(252,140,0)
    0xFFFC8400, // 167: RGB(252,132,0)
    0xFFFC8000, // 168: RGB(252,128,0)
    0xFFFC7C00, // 169: RGB(252,124,0)
    0xFFFC7400, // 170: RGB(252,116,0)
    0xFFFC7000, // 171: RGB(252,112,0)
    0xFFFC6C00, // 172: RGB(252,108,0)
    0xFFFC6400, // 173: RGB(252,100,0)
    0xFFFC6000, // 174: RGB(252,96,0)
    0xFFFC5C00, // 175: RGB(252,92,0)
    0xFFFC5400, // 176: RGB(252,84,0)
    0xFFFC5000, // 177: RGB(252,80,0)
    0xFFFC4C00, // 178: RGB(252,76,0)
    0xFFFC4800, // 179: RGB(252,72,0)
    0xFFFC4000, // 180: RGB(252,64,0)
    0xFFFC3C00, // 181: RGB(252,60,0)
    0xFFFC3800, // 182: RGB(252,56,0)
    0xFFFC3000, // 183: RGB(252,48,0)
    0xFFFC2C00, // 184: RGB(252,44,0)
    0xFFFC2800, // 185: RGB(252,40,0)
    0xFFFC2000, // 186: RGB(252,32,0)
    0xFFFC1C00, // 187: RGB(252,28,0)
    0xFFFC1800, // 188: RGB(252,24,0)
    0xFFFC1000, // 189: RGB(252,16,0)
    0xFFFC0C00, // 190: RGB(252,12,0)
    0xFFFC0800, // 191: RGB(252,8,0)
    0xFFFC0000, // 192: RGB(252,0,0)
    0xFFF80000, // 193: RGB(248,0,0)
    0xFFF40000, // 194: RGB(244,0,0)
    0xFFF00000, // 195: RGB(240,0,0)
    0xFFEC0000, // 196: RGB(236,0,0)
    0xFFE80000, // 197: RGB(232,0,0)
    0xFFE40000, // 198: RGB(228,0,0)
    0xFFE00000, // 199: RGB(224,0,0)
    0xFFDC0000, // 200: RGB(220,0,0)
    0xFFD80000, // 201: RGB(216,0,0)
    0xFFD40000, // 202: RGB(212,0,0)
    0xFFD00000, // 203: RGB(208,0,0)
    0xFFCC0000, // 204: RGB(204,0,0)
    0xFFC80000, // 205: RGB(200,0,0)
    0xFFC40000, // 206: RGB(196,0,0)
    0xFFC00000, // 207: RGB(192,0,0)
    0xFFBC0000, // 208: RGB(188,0,0)
    0xFFB80000, // 209: RGB(184,0,0)
    0xFFB40000, // 210: RGB(180,0,0)
    0xFFB00000, // 211: RGB(176,0,0)
    0xFFAC0000, // 212: RGB(172,0,0)
    0xFFA80000, // 213: RGB(168,0,0)
    0xFFA40000, // 214: RGB(164,0,0)
    0xFFA00000, // 215: RGB(160,0,0)
    0xFF9C0000, // 216: RGB(156,0,0)
    0xFF980000, // 217: RGB(152,0,0)
    0xFF940000, // 218: RGB(148,0,0)
    0xFF900000, // 219: RGB(144,0,0)
    0xFF8C0000, // 220: RGB(140,0,0)
    0xFF880000, // 221: RGB(136,0,0)
    0xFF840000, // 222: RGB(132,0,0)
    0xFF800000, // 223: RGB(128,0,0)
    0xFF7C0000, // 224: RGB(124,0,0)
    0xFF780000, // 225: RGB(120,0,0)
    0xFF740000, // 226: RGB(116,0,0)
    0xFF700000, // 227: RGB(112,0,0)
    0xFF6C0000, // 228: RGB(108,0,0)
    0xFF680000, // 229: RGB(104,0,0)
    0xFF640000, // 230: RGB(100,0,0)
    0xFF600000, // 231: RGB(96,0,0)
    0xFF5C0000, // 232: RGB(92,0,0)
    0xFF580000, // 233: RGB(88,0,0)
    0xFF540000, // 234: RGB(84,0,0)
    0xFF500000, // 235: RGB(80,0,0)
    0xFF4C0000, // 236: RGB(76,0,0)
    0xFF480000, // 237: RGB(72,0,0)
    0xFF440000, // 238: RGB(68,0,0)
    0xFF400000, // 239: RGB(64,0,0)
    0xFF3C0000, // 240: RGB(60,0,0)
    0xFF380000, // 241: RGB(56,0,0)
    0xFF340000, // 242: RGB(52,0,0)
    0xFF300000, // 243: RGB(48,0,0)
    0xFF2C0000, // 244: RGB(44,0,0)
    0xFF280000, // 245: RGB(40,0,0)
    0xFF240000, // 246: RGB(36,0,0)
    0xFF200000, // 247: RGB(32,0,0)
    0xFF1C0000, // 248: RGB(28,0,0)
    0xFF180000, // 249: RGB(24,0,0)
    0xFF140000, // 250: RGB(20,0,0)
    0xFF100000, // 251: RGB(16,0,0)
    0xFF0C0000, // 252: RGB(12,0,0)
    0xFF080000, // 253: RGB(8,0,0)
    0xFF000000, // 254: RGB(0,0,0)
    0xFF000000, // 255: RGB(0,0,0)
};

JuliaFractal::JuliaFractal(lv_obj_t* parent, int windowWidth, int windowHeight)
{
    // Initialize random number generator with random seed.
    std::random_device rd;
    rng_.seed(rd());

    // Store base window dimensions for dynamic resolution scaling.
    baseWindowWidth_ = windowWidth;
    baseWindowHeight_ = windowHeight;

    // Render at reduced resolution for performance.
    width_ = windowWidth / currentResolutionDivisor_;
    height_ = windowHeight / currentResolutionDivisor_;

    spdlog::info(
        "JuliaFractal: Creating {}x{} fractal canvas (render), scaling to {}x{} (display)",
        width_,
        height_,
        windowWidth,
        windowHeight);

    // Create LVGL canvas.
    canvas_ = lv_canvas_create(parent);

    // Allocate canvas buffer at reduced resolution (ARGB8888 format).
    size_t bufferSize = LV_CANVAS_BUF_SIZE(width_, height_, 32, 64);
    canvasBuffer_ = static_cast<lv_color_t*>(lv_malloc(bufferSize));

    if (!canvasBuffer_) {
        spdlog::error("JuliaFractal: Failed to allocate canvas buffer");
        return;
    }

    // Set canvas buffer at render resolution.
    lv_canvas_set_buffer(canvas_, canvasBuffer_, width_, height_, LV_COLOR_FORMAT_ARGB8888);

    // Position in top-left corner.
    lv_obj_set_pos(canvas_, 0, 0);

    // Scale canvas to fill full window using LVGL transform.
    int scaleX = (windowWidth * 256) / width_; // LVGL uses 256 = 1x scale.
    int scaleY = (windowHeight * 256) / height_;
    lv_obj_set_style_transform_scale_x(canvas_, scaleX, 0);
    lv_obj_set_style_transform_scale_y(canvas_, scaleY, 0);

    // Make canvas non-clickable so events pass through to widgets on top.
    lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(canvas_, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Allocate two additional buffers for triple buffering.
    buffers_[0] = canvasBuffer_; // Front buffer (already allocated).
    buffers_[1] = static_cast<lv_color_t*>(lv_malloc(bufferSize));
    buffers_[2] = static_cast<lv_color_t*>(lv_malloc(bufferSize));

    if (!buffers_[1] || !buffers_[2]) {
        spdlog::error("JuliaFractal: Failed to allocate triple buffers");
        return;
    }

    // Render initial fractal to front buffer (synchronous for first frame).
    render();

    // Initialize buffer indices for triple buffering rotation.
    frontBufferIdx_ = 0;  // Buffer 0 displaying.
    readyBufferIdx_ = 1;  // Buffer 1 ready to swap.
    renderBufferIdx_ = 2; // Buffer 2 will be rendered.

    // Initialize timing for parameter changes and FPS tracking.
    lastUpdateTime_ =
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    lastFpsCheckTime_ = lastUpdateTime_;
    lastFpsLogTime_ = lastUpdateTime_;
    lastDisplayUpdateTime_ = lastUpdateTime_;

    // Generate initial random parameters (starts with smooth transition from defaults).
    generateRandomParameters();

    // Start background render thread.
    renderThread_ = std::thread(&JuliaFractal::renderThreadFunc, this);

    spdlog::info("JuliaFractal: Initialized with triple buffering and background thread");
}

JuliaFractal::~JuliaFractal()
{
    // Signal render thread to exit.
    shouldExit_ = true;

    // Wait for render thread to finish.
    if (renderThread_.joinable()) {
        renderThread_.join();
    }

    // Delete canvas object first (this detaches it from parent).
    if (canvas_) {
        lv_obj_del(canvas_);
        canvas_ = nullptr;
    }

    // Free all three buffers.
    for (int i = 0; i < 3; i++) {
        if (buffers_[i]) {
            lv_free(buffers_[i]);
            buffers_[i] = nullptr;
        }
    }

    // canvasBuffer_ is just an alias to buffers_[0], already freed.
    canvasBuffer_ = nullptr;
}

int JuliaFractal::calculateJuliaPoint(int x, int y, double cReal, double cImag, int maxIter) const
{
    // Map pixel coordinates to complex plane.
    double zx = xMin_ + (xMax_ - xMin_) * x / width_;
    double zy = yMin_ + (yMax_ - yMin_) * y / height_;

    // Julia set iteration: z_n+1 = z_n^2 + c.
    int iteration = 0;
    while (iteration < maxIter) {
        double zx2 = zx * zx;
        double zy2 = zy * zy;

        // Check escape condition: |z| > 2.
        if (zx2 + zy2 > 4.0) {
            break;
        }

        // z = z^2 + c.
        double temp = zx2 - zy2 + cReal;
        zy = 2.0 * zx * zy + cImag;
        zx = temp;

        iteration++;
    }

    return iteration;
}

uint32_t JuliaFractal::getPaletteColor(int iteration) const
{
    if (iteration >= maxIterations_) {
        return 0xFF000000; // Black for points in the set.
    }

    // Map iteration to palette with cycling offset (use integer part of floating offset).
    int paletteIndex = (iteration + static_cast<int>(paletteOffset_)) % PALETTE_SIZE;
    return PALETTE[paletteIndex];
}

void JuliaFractal::render()
{
    if (!canvasBuffer_) return;

    // Render to buffer 0 (front buffer during init/resize).
    // Use iteration cache 0 to match buffer 0.
    std::vector<int>& cache = iterationCaches_[0];

    // Resize iteration cache if needed.
    size_t totalPixels = width_ * height_;
    if (cache.size() != totalPixels) {
        cache.resize(totalPixels);
    }

    // Direct buffer access for fast rendering (ARGB8888 = 32 bits per pixel).
    uint32_t* buffer = reinterpret_cast<uint32_t*>(canvasBuffer_);

    // Calculate Julia set and cache iteration counts.
    // render() only called during init/resize when thread stopped, safe to use member vars.
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            int iteration = calculateJuliaPoint(x, y, cReal_, cImag_, maxIterations_);
            int idx = y * width_ + x;
            cache[idx] = iteration;

            // Write directly to buffer.
            buffer[idx] = getPaletteColor(iteration);
        }
    }

    // Mark canvas as dirty to trigger redraw.
    lv_obj_invalidate(canvas_);
}

void JuliaFractal::updateColors()
{
    if (!canvasBuffer_ || iterationCache_.empty()) return;

    // Direct buffer access for fast color update (ARGB8888 = 32 bits per pixel).
    uint32_t* buffer = reinterpret_cast<uint32_t*>(canvasBuffer_);

    // Fast update - only recolor pixels using cached iteration counts.
    size_t totalPixels = width_ * height_;
    for (size_t idx = 0; idx < totalPixels; idx++) {
        int iteration = iterationCache_[idx];
        buffer[idx] = getPaletteColor(iteration);
    }

    // Mark canvas as dirty to trigger redraw.
    lv_obj_invalidate(canvas_);
}

void JuliaFractal::update()
{
    // Track calls vs actual swaps for debugging.
    static int totalCalls = 0;
    static int actualSwaps = 0;
    static double lastDebugLog = 0.0;
    totalCalls++;

    // Check if background thread has a new frame ready.
    if (!readyBufferAvailable_.load(std::memory_order_acquire)) {
        return; // Nothing to do, wait for next frame.
    }

    actualSwaps++;

    // Track display-side FPS (how often we actually display a new frame).
    double currentTime =
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();

    // Debug logging every 10 seconds.
    if (currentTime - lastDebugLog >= 10.0) {
        double swapRate = actualSwaps / (currentTime - lastDebugLog);
        double callRate = totalCalls / (currentTime - lastDebugLog);
        spdlog::info(
            "JuliaFractal: update() called {:.1f}/sec, swapped {:.1f}/sec", callRate, swapRate);
        totalCalls = 0;
        actualSwaps = 0;
        lastDebugLog = currentTime;
    }

    double displayDeltaTime = currentTime - lastDisplayUpdateTime_;
    lastDisplayUpdateTime_ = currentTime;

    if (displayDeltaTime > 0.0 && displayDeltaTime < 0.1) {
        double displayFps = 1.0 / displayDeltaTime;
        displayFpsSum_ += displayFps;
        displayFpsSampleCount_++;
    }

    // Swap ready buffer to front (main thread displays it).
    int oldFrontIdx = frontBufferIdx_.load(std::memory_order_relaxed);
    int newFrontIdx = readyBufferIdx_.load(std::memory_order_relaxed);

    // Update canvas to use the ready buffer (just pointer assignment, no copy).
    canvasBuffer_ = buffers_[newFrontIdx];
    lv_canvas_set_buffer(canvas_, canvasBuffer_, width_, height_, LV_COLOR_FORMAT_ARGB8888);

    // Swap indices: old front becomes new ready (for render thread to use).
    frontBufferIdx_.store(newFrontIdx, std::memory_order_release);
    readyBufferIdx_.store(oldFrontIdx, std::memory_order_release);

    // Mark canvas as dirty (let lv_wayland_timer_handler() do the actual repaint).
    // Do NOT call lv_refr_now() - it blocks for 15-26ms, killing performance.
    // The simulation uses the same approach (just invalidate, repaint in timer handler).
    lv_obj_invalidate(canvas_);

    // Signal that we consumed the ready buffer.
    readyBufferAvailable_.store(false, std::memory_order_release);

    // Check if dynamic resolution scaling triggered a resize.
    if (resizeNeeded_.load(std::memory_order_acquire)) {
        resizeNeeded_.store(false, std::memory_order_release);

        // Calculate new dimensions based on current resolution divisor.
        int newRenderWidth = static_cast<int>(baseWindowWidth_ / currentResolutionDivisor_);
        int newRenderHeight = static_cast<int>(baseWindowHeight_ / currentResolutionDivisor_);

        spdlog::info(
            "JuliaFractal: Dynamic resize triggered: {}x{} -> {}x{} (divisor={:.2f})",
            width_,
            height_,
            newRenderWidth,
            newRenderHeight,
            currentResolutionDivisor_);

        // Call resize to apply the new resolution.
        resize(baseWindowWidth_, baseWindowHeight_);
    }
}

void JuliaFractal::resize(int newWidth, int newHeight)
{
    // Update base window dimensions (in case this is a real window resize).
    baseWindowWidth_ = newWidth;
    baseWindowHeight_ = newHeight;

    // Calculate render resolution using current divisor.
    int renderWidth = static_cast<int>(newWidth / currentResolutionDivisor_);
    int renderHeight = static_cast<int>(newHeight / currentResolutionDivisor_);

    // Check if resize is needed.
    if (renderWidth == width_ && renderHeight == height_) {
        return;
    }

    spdlog::info(
        "JuliaFractal: Resizing from {}x{} to {}x{} (render), scaling to {}x{} (display), "
        "divisor={:.2f}",
        width_,
        height_,
        renderWidth,
        renderHeight,
        newWidth,
        newHeight,
        currentResolutionDivisor_);

    // Stop render thread temporarily during resize to avoid race conditions.
    shouldExit_ = true;
    if (renderThread_.joinable()) {
        renderThread_.join();
    }

    // Now safe to resize without mutex (thread stopped).
    std::lock_guard<std::mutex> lock(bufferMutex_);

    // Update render dimensions.
    width_ = renderWidth;
    height_ = renderHeight;

    // Free old buffers.
    for (int i = 0; i < 3; i++) {
        if (buffers_[i]) {
            lv_free(buffers_[i]);
            buffers_[i] = nullptr;
        }
    }

    // Allocate new buffers at render resolution.
    size_t bufferSize = LV_CANVAS_BUF_SIZE(width_, height_, 32, 64);
    for (int i = 0; i < 3; i++) {
        buffers_[i] = static_cast<lv_color_t*>(lv_malloc(bufferSize));
        if (!buffers_[i]) {
            spdlog::error("JuliaFractal: Failed to allocate buffer {} during resize", i);
            return;
        }
    }

    // Update canvasBuffer to point to buffer 0.
    canvasBuffer_ = buffers_[0];

    // Update canvas buffer at render resolution.
    lv_canvas_set_buffer(canvas_, canvasBuffer_, width_, height_, LV_COLOR_FORMAT_ARGB8888);

    // Update transform scale to fill new display size.
    int scaleX = (newWidth * 256) / width_; // LVGL uses 256 = 1x scale.
    int scaleY = (newHeight * 256) / height_;
    lv_obj_set_style_transform_scale_x(canvas_, scaleX, 0);
    lv_obj_set_style_transform_scale_y(canvas_, scaleY, 0);

    // Re-render at new size (uses buffer 0).
    render();

    // Reset buffer indices for triple buffering.
    frontBufferIdx_ = 0;
    readyBufferIdx_ = 1;
    renderBufferIdx_ = 2;

    // Restart render thread with new dimensions.
    shouldExit_ = false;
    readyBufferAvailable_ = false;
    renderThread_ = std::thread(&JuliaFractal::renderThreadFunc, this);

    spdlog::info("JuliaFractal: Resize complete, render thread restarted");
}

void JuliaFractal::generateRandomParameters()
{
    std::lock_guard<std::mutex> lock(parameterMutex_);

    // Save old parameters for smooth transition.
    oldCRealCenter_ = cRealCenter_;
    oldCRealAmplitude_ = cRealAmplitude_;
    oldCImagCenter_ = cImagCenter_;
    oldCImagAmplitude_ = cImagAmplitude_;
    oldDetailPhaseSpeed_ = detailPhaseSpeed_;
    oldCPhaseSpeed_ = cPhaseSpeed_;
    oldMinIterationBound_ = minIterationBound_;
    oldMaxIterationBound_ = maxIterationBound_;

    // 80% curated regions, 20% random exploration.
    std::uniform_real_distribution<double> modeDist(0.0, 1.0);
    bool useCuratedRegion = modeDist(rng_) < 0.8;

    if (useCuratedRegion) {
        // Pick a random interesting region.
        std::uniform_int_distribution<int> regionDist(0, NUM_REGIONS - 1);
        currentRegionIdx_ = regionDist(rng_);
        auto [centerReal, centerImag] = INTERESTING_REGIONS[currentRegionIdx_];

        // Add small random variation around the region center.
        std::normal_distribution<double> variation(0.0, 0.03); // Small jitter.
        cRealCenter_ = centerReal + variation(rng_);
        cImagCenter_ = centerImag + variation(rng_);

        spdlog::info(
            "JuliaFractal: Selected curated region {} ({}) - c = {:.4f} + {:.4f}i",
            currentRegionIdx_,
            REGION_NAMES[currentRegionIdx_],
            cRealCenter_,
            cImagCenter_);
    }
    else {
        // Random exploration - stay in reasonable bounds.
        currentRegionIdx_ = -1; // Mark as random exploration.
        std::uniform_real_distribution<double> realDist(-1.2, 0.5);
        std::uniform_real_distribution<double> imagDist(-0.8, 0.8);
        cRealCenter_ = realDist(rng_);
        cImagCenter_ = imagDist(rng_);

        spdlog::info(
            "JuliaFractal: Random exploration - c = {:.4f} + {:.4f}i", cRealCenter_, cImagCenter_);
    }

    // Use smaller oscillation amplitudes to stay near interesting regions.
    std::uniform_real_distribution<double> ampDist(0.05, 0.15);
    cRealAmplitude_ = ampDist(rng_);
    cImagAmplitude_ = ampDist(rng_);

    // Randomize animation speeds.
    std::uniform_real_distribution<double> cSpeedDist(0.001, 0.025);
    cPhaseSpeed_ = cSpeedDist(rng_);

    // Slower detail oscillation speed (30-120 second cycles at 60fps).
    std::uniform_real_distribution<double> detailSpeedDist(0.0015, 0.006);
    detailPhaseSpeed_ = detailSpeedDist(rng_);

    // Randomize iteration range for detail oscillation.
    // 5% chance of going to 0, otherwise start at 20-50.
    std::uniform_real_distribution<double> minModeDist(0.0, 1.0);
    if (minModeDist(rng_) < 0.05) {
        minIterationBound_ = 0; // Cool minimal look sometimes.
    }
    else {
        std::uniform_int_distribution<int> minIterDist(20, 50);
        minIterationBound_ = minIterDist(rng_);
    }

    std::uniform_int_distribution<int> maxIterDist(280, 500);
    maxIterationBound_ = maxIterDist(rng_);

    spdlog::info(
        "JuliaFractal: New iteration range [{}, {}]", minIterationBound_, maxIterationBound_);

    phaseSpeed_ = 0.1;

    // Next parameter change in 30-60 seconds.
    std::uniform_real_distribution<double> intervalDist(10.0, 20.0);
    currentChangeInterval_ = intervalDist(rng_);
    changeTimer_ = 0.0;

    // Start smooth transition.
    transitionProgress_ = 0.0;
}

void JuliaFractal::renderThreadFunc()
{
    spdlog::info("JuliaFractal: Render thread started");

    while (!shouldExit_) {
        // Calculate delta time for smooth animations.
        double currentTime =
            std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        double deltaTime = currentTime - lastUpdateTime_;
        lastUpdateTime_ = currentTime;

        // Clamp deltaTime to prevent huge jumps (e.g., after resize).
        if (deltaTime > 0.1) {
            deltaTime = 0.1;
        }

        // Update parameter change timer.
        changeTimer_ += deltaTime;
        if (changeTimer_ >= currentChangeInterval_) {
            generateRandomParameters();
        }

        // Update smooth transition progress.
        if (transitionProgress_ < 1.0) {
            transitionProgress_ += deltaTime / transitionDuration_;
            if (transitionProgress_ > 1.0) {
                transitionProgress_ = 1.0;
            }
        }

        // Read all animation parameters atomically and compute transition values.
        double activeCRealCenter, activeCRealAmplitude, activeCImagCenter, activeCImagAmplitude;
        double activeDetailPhaseSpeed, activeCPhaseSpeed;
        int activeMinIterations, activeMaxIterations;
        int newMaxIterations;
        double newCReal, newCImag;

        {
            std::lock_guard<std::mutex> lock(parameterMutex_);

            // Smooth cubic easing for transitions.
            double t = transitionProgress_;
            double smoothT = t * t * (3.0 - 2.0 * t);

            // Interpolate parameters during transition.
            activeCRealCenter = oldCRealCenter_ + (cRealCenter_ - oldCRealCenter_) * smoothT;
            activeCRealAmplitude =
                oldCRealAmplitude_ + (cRealAmplitude_ - oldCRealAmplitude_) * smoothT;
            activeCImagCenter = oldCImagCenter_ + (cImagCenter_ - oldCImagCenter_) * smoothT;
            activeCImagAmplitude =
                oldCImagAmplitude_ + (cImagAmplitude_ - oldCImagAmplitude_) * smoothT;
            activeDetailPhaseSpeed =
                oldDetailPhaseSpeed_ + (detailPhaseSpeed_ - oldDetailPhaseSpeed_) * smoothT;
            activeCPhaseSpeed = oldCPhaseSpeed_ + (cPhaseSpeed_ - oldCPhaseSpeed_) * smoothT;
            activeMinIterations = oldMinIterationBound_
                + static_cast<int>((minIterationBound_ - oldMinIterationBound_) * smoothT);
            activeMaxIterations = oldMaxIterationBound_
                + static_cast<int>((maxIterationBound_ - oldMaxIterationBound_) * smoothT);

            // Read current values for this frame.
            newMaxIterations = maxIterations_;
            newCReal = cReal_;
            newCImag = cImag_;
        }

        bool needsUpdate = false;
        bool cChanged = false;
        bool iterationsChanged = false;

        // Palette cycling animation (constant speed - no pulsing).
        if (phaseSpeed_ > 0.0) {
            // Direct constant rotation (no sine wave pulsing).
            double cycleSpeed = phaseSpeed_;
            paletteOffset_ += cycleSpeed;
            if (paletteOffset_ >= PALETTE_SIZE) {
                paletteOffset_ -= PALETTE_SIZE;
            }

            // Debug: Log palette rotation.
            static double lastPaletteLog = 0.0;
            if (currentTime - lastPaletteLog >= 5.0) {
                spdlog::info(
                    "JuliaFractal: Palette offset={:.1f}, speed={:.3f}/frame",
                    paletteOffset_,
                    cycleSpeed);
                lastPaletteLog = currentTime;
            }

            needsUpdate = true; // Colors changed.
        }

        // Detail level animation (only if enabled).
        if (activeDetailPhaseSpeed > 0.0) {
            detailPhase_ += activeDetailPhaseSpeed;
            if (detailPhase_ > 2.0 * M_PI) {
                detailPhase_ -= 2.0 * M_PI;
            }

            // Sine wave gives raw 0-1 oscillation.
            double rawFactor = (std::sin(detailPhase_) + 1.0) / 2.0;

            // Apply inverted parabola to spend MORE time in middle, LESS at extremes.
            // This peaks at 0.5 and drops off toward 0 and 1.
            double centered = rawFactor - 0.5;                 // Shift to [-0.5, 0.5].
            double parabola = 1.0 - 4.0 * centered * centered; // Peak at center.
            // Map parabola [0,1] back to iteration range (biased toward middle).
            double detailFactor = 0.2 + parabola * 0.6; // Range [0.2, 0.8] favors middle.

            // Map to randomized iteration range.
            newMaxIterations = activeMinIterations
                + static_cast<int>(detailFactor * (activeMaxIterations - activeMinIterations));
            iterationsChanged = (newMaxIterations != maxIterations_); // Render on ANY change.
            needsUpdate = true; // Always update for smooth animation.
        }

        // Shape morphing animation (only if enabled).
        if (activeCPhaseSpeed > 0.0 && (activeCRealAmplitude > 0.0 || activeCImagAmplitude > 0.0)) {
            cPhase_ += activeCPhaseSpeed;
            if (cPhase_ > 2.0 * M_PI) {
                cPhase_ -= 2.0 * M_PI;
            }

            double cRealFactor = std::sin(cPhase_);
            double cImagFactor = std::sin(cPhase_ + M_PI / 2.0);
            newCReal = activeCRealCenter + cRealFactor * activeCRealAmplitude;
            newCImag = activeCImagCenter + cImagFactor * activeCImagAmplitude;
            cChanged = true;    // Always render for smooth morphing (no threshold).
            needsUpdate = true; // Always update.
        }

        // If nothing is animating, sleep and skip rendering.
        if (!needsUpdate && !cChanged && !iterationsChanged) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Get render buffer (our exclusive buffer for rendering).
        lv_color_t* renderBuf = buffers_[renderBufferIdx_];
        std::vector<int>& renderCache = iterationCaches_[renderBufferIdx_];

        // Resize cache if needed.
        size_t totalPixels = width_ * height_;
        if (renderCache.size() != totalPixels) {
            renderCache.resize(totalPixels);
        }

        uint32_t* renderBufPtr = reinterpret_cast<uint32_t*>(renderBuf);

        // Capture all parameters for this frame (prevents data races - all threads use same
        // values).
        int currentPaletteOffset = static_cast<int>(paletteOffset_);
        int currentMaxIterations;
        double currentCReal;
        double currentCImag;

        {
            std::lock_guard<std::mutex> lock(parameterMutex_);
            currentMaxIterations = maxIterations_;
            currentCReal = cReal_;
            currentCImag = cImag_;
        }

        if (cChanged || iterationsChanged) {
            // Update parameters for next frame (thread-safe).
            {
                std::lock_guard<std::mutex> lock(parameterMutex_);
                cReal_ = newCReal;
                cImag_ = newCImag;
                maxIterations_ = newMaxIterations;
            }

            // Update captured values to use for THIS frame.
            currentMaxIterations = newMaxIterations;
            currentCReal = newCReal;
            currentCImag = newCImag;

            // Recalculate Julia set to render buffer using multiple threads.
            std::vector<std::thread> workers;
            int rowsPerThread = height_ / RENDER_THREADS;

            for (int t = 0; t < RENDER_THREADS; t++) {
                int startRow = t * rowsPerThread;
                int endRow = (t == RENDER_THREADS - 1) ? height_ : (t + 1) * rowsPerThread;

                workers.emplace_back([this,
                                      &renderCache,
                                      renderBufPtr,
                                      startRow,
                                      endRow,
                                      currentPaletteOffset,
                                      currentMaxIterations,
                                      currentCReal,
                                      currentCImag]() {
                    for (int y = startRow; y < endRow; y++) {
                        for (int x = 0; x < width_; x++) {
                            int iteration = calculateJuliaPoint(
                                x, y, currentCReal, currentCImag, currentMaxIterations);
                            int idx = y * width_ + x;
                            renderCache[idx] = iteration;

                            // Black for points in the set.
                            if (iteration >= currentMaxIterations) {
                                renderBufPtr[idx] = 0xFF000000;
                            }
                            else {
                                // Normalize iteration to [0,255] to use full palette range
                                // smoothly.
                                int normalizedIteration = (iteration * 255) / currentMaxIterations;
                                int paletteIndex =
                                    (normalizedIteration + currentPaletteOffset) % PALETTE_SIZE;
                                renderBufPtr[idx] = PALETTE[paletteIndex];
                            }
                        }
                    }
                });
            }

            // Wait for all threads to complete.
            for (auto& worker : workers) {
                worker.join();
            }
        }
        else {
            // Fast update - only recolor using cached iterations (also parallelized).
            std::vector<std::thread> workers;
            int pixelsPerThread = totalPixels / RENDER_THREADS;

            for (int t = 0; t < RENDER_THREADS; t++) {
                size_t startIdx = t * pixelsPerThread;
                size_t endIdx = (t == RENDER_THREADS - 1) ? totalPixels : (t + 1) * pixelsPerThread;

                workers.emplace_back([this,
                                      &renderCache,
                                      renderBufPtr,
                                      startIdx,
                                      endIdx,
                                      currentPaletteOffset,
                                      currentMaxIterations]() {
                    for (size_t idx = startIdx; idx < endIdx; idx++) {
                        int iteration = renderCache[idx];

                        // Black for points in the set.
                        if (iteration >= currentMaxIterations) {
                            renderBufPtr[idx] = 0xFF000000;
                        }
                        else {
                            // Normalize iteration to [0,255] to use full palette range smoothly.
                            int normalizedIteration = (iteration * 255) / currentMaxIterations;
                            int paletteIndex =
                                (normalizedIteration + currentPaletteOffset) % PALETTE_SIZE;
                            renderBufPtr[idx] = PALETTE[paletteIndex];
                        }
                    }
                });
            }

            // Wait for all threads to complete.
            for (auto& worker : workers) {
                worker.join();
            }
        }

        // Wait until ready buffer is consumed before we can promote our rendered frame.
        while (readyBufferAvailable_.load(std::memory_order_acquire) && !shouldExit_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (shouldExit_) break;

        // Swap render buffer to ready position (our frame is now ready to display).
        int oldReadyIdx = readyBufferIdx_.load(std::memory_order_relaxed);
        readyBufferIdx_.store(renderBufferIdx_, std::memory_order_release);
        renderBufferIdx_ = oldReadyIdx; // Old ready buffer becomes our next render target.

        // Signal that a new ready buffer is available.
        readyBufferAvailable_.store(true, std::memory_order_release);

        // Track FPS for dynamic resolution scaling.
        if (deltaTime > 0.0) {
            double currentFps = 1.0 / deltaTime;
            fpsSum_ += currentFps;
            fpsSampleCount_++;

            // Log FPS occasionally.
            if (currentTime - lastFpsLogTime_ >= FPS_LOG_INTERVAL) {
                double renderFps = fpsSum_ / fpsSampleCount_;
                double displayFps =
                    (displayFpsSampleCount_ > 0) ? (displayFpsSum_ / displayFpsSampleCount_) : 0.0;
                spdlog::info(
                    "JuliaFractal: Render FPS = {:.1f}, Display FPS = {:.1f}, Resolution = {}x{} "
                    "(divisor={:.2f})",
                    renderFps,
                    displayFps,
                    width_,
                    height_,
                    currentResolutionDivisor_);
                lastFpsLogTime_ = currentTime;

                // Reset display FPS tracking.
                displayFpsSum_ = 0.0;
                displayFpsSampleCount_ = 0;
            }

            // Check FPS every few seconds for dynamic resolution adjustment.
            if (currentTime - lastFpsCheckTime_ >= FPS_CHECK_INTERVAL) {
                double renderFps = fpsSum_ / fpsSampleCount_;
                [[maybe_unused]] double displayFps =
                    (displayFpsSampleCount_ > 0) ? (displayFpsSum_ / displayFpsSampleCount_) : 0.0;

                // Adaptive resolution scaling: smooth adjustments based on FPS.
                // Target 30fps minimum, use render FPS as primary metric.
                constexpr double TARGET_MIN_FPS = 30.0;
                constexpr double TARGET_COMFORT_FPS = 55.0;
                constexpr double MIN_DIVISOR = 2.0;
                constexpr double MAX_DIVISOR = 8.0;

                // Use render FPS (display FPS is LVGL-limited).
                double effectiveFps = renderFps;

                // Calculate adjustment based on FPS deficit/surplus.
                double divisorAdjustment = 0.0;

                if (effectiveFps < TARGET_MIN_FPS) {
                    // Below 30fps: increase divisor (lower resolution).
                    // Adjustment proportional to how far below target.
                    double deficit = (TARGET_MIN_FPS - effectiveFps) / TARGET_MIN_FPS;
                    divisorAdjustment = 0.2 * deficit; // Up to +0.2 per check.
                }
                else if (effectiveFps > TARGET_COMFORT_FPS) {
                    // Above 45fps: decrease divisor (higher resolution).
                    // Smaller adjustment to prevent oscillation.
                    double surplus = (effectiveFps - TARGET_COMFORT_FPS) / effectiveFps;
                    divisorAdjustment = -0.1 * surplus; // Up to -0.1 per check.
                }

                // Apply adjustment if significant.
                if (std::abs(divisorAdjustment) > 0.01) {
                    double newDivisor = currentResolutionDivisor_ + divisorAdjustment;
                    newDivisor = std::clamp(newDivisor, MIN_DIVISOR, MAX_DIVISOR);

                    // Only resize if divisor changed by at least 0.1 (avoid tiny adjustments).
                    if (std::abs(newDivisor - currentResolutionDivisor_) >= 0.1) {
                        spdlog::info(
                            "JuliaFractal: Adaptive scaling (FPS={:.1f}): divisor {:.2f} -> {:.2f}",
                            effectiveFps,
                            currentResolutionDivisor_,
                            newDivisor);

                        currentResolutionDivisor_ = newDivisor;
                        resizeNeeded_.store(true, std::memory_order_release);
                    }
                }

                // Reset FPS tracking.
                fpsSum_ = 0.0;
                fpsSampleCount_ = 0;
                lastFpsCheckTime_ = currentTime;
            }
        }
    }

    spdlog::info("JuliaFractal: Render thread exiting");
}

void JuliaFractal::advanceToNextFractal()
{
    spdlog::info("JuliaFractal: Manual advance to next fractal requested");
    generateRandomParameters();
}

double JuliaFractal::getCReal() const
{
    std::lock_guard<std::mutex> lock(parameterMutex_);
    return cReal_;
}

double JuliaFractal::getCImag() const
{
    std::lock_guard<std::mutex> lock(parameterMutex_);
    return cImag_;
}

const char* JuliaFractal::getRegionName() const
{
    if (currentRegionIdx_ >= 0 && currentRegionIdx_ < NUM_REGIONS) {
        return REGION_NAMES[currentRegionIdx_];
    }
    return "Random Exploration";
}

int JuliaFractal::getTransitioningMinIterations() const
{
    std::lock_guard<std::mutex> lock(parameterMutex_);
    if (transitionProgress_ < 1.0) {
        // Smoothly transition from old to new minimum.
        double t_smooth =
            transitionProgress_ * transitionProgress_ * (3.0 - 2.0 * transitionProgress_);
        return static_cast<int>(
            oldMinIterationBound_ + (minIterationBound_ - oldMinIterationBound_) * t_smooth);
    }
    return minIterationBound_;
}

int JuliaFractal::getTransitioningMaxIterations() const
{
    std::lock_guard<std::mutex> lock(parameterMutex_);
    if (transitionProgress_ < 1.0) {
        // Smoothly transition from old to new maximum.
        double t_smooth =
            transitionProgress_ * transitionProgress_ * (3.0 - 2.0 * transitionProgress_);
        return static_cast<int>(
            oldMaxIterationBound_ + (maxIterationBound_ - oldMaxIterationBound_) * t_smooth);
    }
    return maxIterationBound_;
}

int JuliaFractal::getCurrentIterations() const
{
    std::lock_guard<std::mutex> lock(parameterMutex_);

    // Calculate current iteration count based on detail oscillation.
    // detailPhase_ oscillates [0, 2π], we use sin to go [0, 1].
    double t = (std::sin(detailPhase_) + 1.0) / 2.0; // [0, 1].

    // Get transitioning min/max bounds (already under lock).
    int minIter, maxIter;
    if (transitionProgress_ < 1.0) {
        double t_smooth =
            transitionProgress_ * transitionProgress_ * (3.0 - 2.0 * transitionProgress_);
        minIter = static_cast<int>(
            oldMinIterationBound_ + (minIterationBound_ - oldMinIterationBound_) * t_smooth);
        maxIter = static_cast<int>(
            oldMaxIterationBound_ + (maxIterationBound_ - oldMaxIterationBound_) * t_smooth);
    }
    else {
        minIter = minIterationBound_;
        maxIter = maxIterationBound_;
    }

    return static_cast<int>(minIter + (maxIter - minIter) * t);
}

void JuliaFractal::getIterationInfo(int& outMin, int& outCurrent, int& outMax) const
{
    std::lock_guard<std::mutex> lock(parameterMutex_);

    // Calculate transitioning min/max bounds.
    int minIter, maxIter;
    if (transitionProgress_ < 1.0) {
        double t_smooth =
            transitionProgress_ * transitionProgress_ * (3.0 - 2.0 * transitionProgress_);
        minIter = static_cast<int>(
            oldMinIterationBound_ + (minIterationBound_ - oldMinIterationBound_) * t_smooth);
        maxIter = static_cast<int>(
            oldMaxIterationBound_ + (maxIterationBound_ - oldMaxIterationBound_) * t_smooth);
    }
    else {
        minIter = minIterationBound_;
        maxIter = maxIterationBound_;
    }

    // Calculate current iteration based on detail oscillation.
    double t = (std::sin(detailPhase_) + 1.0) / 2.0; // [0, 1].
    int current = static_cast<int>(minIter + (maxIter - minIter) * t);

    // All three values computed under a single lock - guaranteed consistent.
    outMin = minIter;
    outCurrent = current;
    outMax = maxIter;
}

double JuliaFractal::getDisplayFps() const
{
    if (displayFpsSampleCount_ > 0) {
        return displayFpsSum_ / displayFpsSampleCount_;
    }
    return 0.0;
}

} // namespace Ui
} // namespace DirtSim
