#include "JuliaFractal.h"
#include <cmath>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

// Rendering performance.
constexpr int RESOLUTION_DIVISOR = 2;         // Render at 1/N resolution (2 = half, 4 = quarter).

// Animation constants.
constexpr double PHASE_SPEED = 0.05;          // Palette cycling oscillation speed.
constexpr double MAX_CYCLE_SPEED = 4.0;       // Maximum palette advance per frame.
constexpr double DETAIL_PHASE_SPEED = 0.01;   // Detail level oscillation speed (slower).
constexpr int MIN_ITERATIONS = 1;             // Minimum iteration count (less detail).
constexpr int MAX_ITERATIONS = 128;           // Maximum iteration count (more detail).

// Julia set constant (c) oscillation for shape morphing.
constexpr double C_PHASE_SPEED = 0.008;       // Very slow shape morphing.
constexpr double C_REAL_CENTER = -0.7;        // Center value for cReal.
constexpr double C_REAL_AMPLITUDE = 0.15;     // How far cReal oscillates (+/-).
constexpr double C_IMAG_CENTER = 0.27;        // Center value for cImag.
constexpr double C_IMAG_AMPLITUDE = 0.1;      // How far cImag oscillates (+/-).

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
    // Render at reduced resolution for performance.
    width_ = windowWidth / RESOLUTION_DIVISOR;
    height_ = windowHeight / RESOLUTION_DIVISOR;

    spdlog::info("JuliaFractal: Creating {}x{} fractal canvas (render), scaling to {}x{} (display)",
                 width_, height_, windowWidth, windowHeight);

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
    int scaleX = (windowWidth * 256) / width_;   // LVGL uses 256 = 1x scale.
    int scaleY = (windowHeight * 256) / height_;
    lv_obj_set_style_transform_scale_x(canvas_, scaleX, 0);
    lv_obj_set_style_transform_scale_y(canvas_, scaleY, 0);

    // Make canvas non-clickable so events pass through to widgets on top.
    lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(canvas_, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Render initial fractal.
    render();

    spdlog::info("JuliaFractal: Initialized successfully");
}

JuliaFractal::~JuliaFractal()
{
    // Delete canvas object first (this detaches it from parent).
    if (canvas_) {
        lv_obj_del(canvas_);
        canvas_ = nullptr;
    }

    // Then free the buffer.
    if (canvasBuffer_) {
        lv_free(canvasBuffer_);
        canvasBuffer_ = nullptr;
    }
}

int JuliaFractal::calculateJuliaPoint(int x, int y) const
{
    // Map pixel coordinates to complex plane.
    double zx = xMin_ + (xMax_ - xMin_) * x / width_;
    double zy = yMin_ + (yMax_ - yMin_) * y / height_;

    // Julia set iteration: z_n+1 = z_n^2 + c.
    int iteration = 0;
    while (iteration < maxIterations_) {
        double zx2 = zx * zx;
        double zy2 = zy * zy;

        // Check escape condition: |z| > 2.
        if (zx2 + zy2 > 4.0) {
            break;
        }

        // z = z^2 + c.
        double temp = zx2 - zy2 + cReal_;
        zy = 2.0 * zx * zy + cImag_;
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

    // Resize iteration cache if needed.
    size_t totalPixels = width_ * height_;
    if (iterationCache_.size() != totalPixels) {
        iterationCache_.resize(totalPixels);
    }

    // Direct buffer access for fast rendering (ARGB8888 = 32 bits per pixel).
    uint32_t* buffer = reinterpret_cast<uint32_t*>(canvasBuffer_);

    // Calculate Julia set and cache iteration counts.
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            int iteration = calculateJuliaPoint(x, y);
            int idx = y * width_ + x;
            iterationCache_[idx] = iteration;

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
    // Advance animation phase for sinusoidal palette speed variation.
    animationPhase_ += PHASE_SPEED;
    if (animationPhase_ > 2.0 * M_PI) {
        animationPhase_ -= 2.0 * M_PI;
    }

    // Calculate current cycling speed using sine wave.
    // sin ranges from -1 to 1, so (sin + 1)/2 gives 0 to 1.
    double speedFactor = (std::sin(animationPhase_) + 1.0) / 2.0;
    double cycleSpeed = speedFactor * MAX_CYCLE_SPEED;

    // Advance palette offset by the current speed.
    paletteOffset_ += cycleSpeed;
    if (paletteOffset_ >= PALETTE_SIZE) {
        paletteOffset_ -= PALETTE_SIZE;
    }

    // Advance detail phase for iteration count variation.
    detailPhase_ += DETAIL_PHASE_SPEED;
    if (detailPhase_ > 2.0 * M_PI) {
        detailPhase_ -= 2.0 * M_PI;
    }

    // Calculate current iteration count using sine wave.
    // Varies between MIN_ITERATIONS and MAX_ITERATIONS.
    double detailFactor = (std::sin(detailPhase_) + 1.0) / 2.0;
    int newMaxIterations = MIN_ITERATIONS + static_cast<int>(detailFactor * (MAX_ITERATIONS - MIN_ITERATIONS));

    // Advance Julia constant phase for shape morphing.
    cPhase_ += C_PHASE_SPEED;
    if (cPhase_ > 2.0 * M_PI) {
        cPhase_ -= 2.0 * M_PI;
    }

    // Calculate current Julia constant values using sine waves.
    // Use different phase offsets for cReal and cImag to create complex morphing.
    double cRealFactor = std::sin(cPhase_);
    double cImagFactor = std::sin(cPhase_ + M_PI / 2.0); // 90 degree phase shift.

    double newCReal = C_REAL_CENTER + cRealFactor * C_REAL_AMPLITUDE;
    double newCImag = C_IMAG_CENTER + cImagFactor * C_IMAG_AMPLITUDE;

    // Check if Julia constant changed significantly (requires recalculation).
    bool cChanged = (std::abs(newCReal - cReal_) > 0.01) || (std::abs(newCImag - cImag_) > 0.01);
    bool iterationsChanged = std::abs(newMaxIterations - lastRenderedMaxIterations_) >= 4;

    if (cChanged || iterationsChanged) {
        // Update parameters.
        cReal_ = newCReal;
        cImag_ = newCImag;
        maxIterations_ = newMaxIterations;
        lastRenderedMaxIterations_ = newMaxIterations;

        // Recalculate fractal with new parameters.
        render();
    }
    else {
        // Fast update - only recolor pixels, don't recalculate fractal.
        updateColors();
    }
}

void JuliaFractal::resize(int newWidth, int newHeight)
{
    // Calculate render resolution.
    int renderWidth = newWidth / RESOLUTION_DIVISOR;
    int renderHeight = newHeight / RESOLUTION_DIVISOR;

    // Check if resize is needed.
    if (renderWidth == width_ && renderHeight == height_) {
        return;
    }

    spdlog::info("JuliaFractal: Resizing from {}x{} to {}x{} (render), scaling to {}x{} (display)",
                 width_, height_, renderWidth, renderHeight, newWidth, newHeight);

    // Update render dimensions.
    width_ = renderWidth;
    height_ = renderHeight;

    // Free old buffer.
    if (canvasBuffer_) {
        lv_free(canvasBuffer_);
        canvasBuffer_ = nullptr;
    }

    // Allocate new buffer at render resolution.
    size_t bufferSize = LV_CANVAS_BUF_SIZE(width_, height_, 32, 64);
    canvasBuffer_ = static_cast<lv_color_t*>(lv_malloc(bufferSize));

    if (!canvasBuffer_) {
        spdlog::error("JuliaFractal: Failed to allocate new canvas buffer during resize");
        return;
    }

    // Update canvas buffer at render resolution.
    lv_canvas_set_buffer(canvas_, canvasBuffer_, width_, height_, LV_COLOR_FORMAT_ARGB8888);

    // Update transform scale to fill new display size.
    int scaleX = (newWidth * 256) / width_;   // LVGL uses 256 = 1x scale.
    int scaleY = (newHeight * 256) / height_;
    lv_obj_set_style_transform_scale_x(canvas_, scaleX, 0);
    lv_obj_set_style_transform_scale_y(canvas_, scaleY, 0);

    // Re-render at new size.
    render();

    spdlog::info("JuliaFractal: Resize complete");
}

} // namespace Ui
} // namespace DirtSim
