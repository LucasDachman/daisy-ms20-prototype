// =============================================================================
// eye_renderer.cpp — Animated almond-eye rendering for 128×64 OLED
// =============================================================================

#include "eye_renderer.h"
#include "params.h"
#include <cstring>

// ── Deterministic hash for textures ────────────────────────────────────────

uint32_t EyeRenderer::Hash(int x, int y, uint32_t seed) {
    uint32_t h = (uint32_t)x * 374761393u
               + (uint32_t)y * 668265263u
               + seed * 2654435761u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// ── Init / NoteOn / NoteOff ───────────────────────────────────────────────

void EyeRenderer::Init() {
    std::memset(buffer_, 0, BUF_SIZE);
    ray_env_ = 0.0f;
    lid_env_ = 0.0f;
    gate_ = false;
    ripple_phase_ = 0.0f;
    std::memset(ripple_offsets_, 0, sizeof(ripple_offsets_));
    pupil_cx_ = EYE_CX;
    pupil_cy_ = EYE_CY;
    frame_count_ = 0;
}

void EyeRenderer::NoteOn() {
    lid_env_ = 1.0f;
    gate_ = true;
}

void EyeRenderer::NoteOff() {
    gate_ = false;
}

// ── Pixel operations (with shake offset + bounds check) ───────────────────

void EyeRenderer::PxSet(int x, int y) {
    if (y >= 0 && y < H) x += ripple_offsets_[y];
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    buffer_[x + (y / 8) * W] |= (1 << (y & 7));
}

void EyeRenderer::PxClear(int x, int y) {
    if (y >= 0 && y < H) x += ripple_offsets_[y];
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    buffer_[x + (y / 8) * W] &= ~(1 << (y & 7));
}

// ── Almond shape ──────────────────────────────────────────────────────────

float EyeRenderer::AlmondShape(float dx_norm) const {
    float x2 = dx_norm * dx_norm;
    if (x2 >= 1.0f) return 0.0f;
    // sqrt(1 - x^2) — circle
    return std::sqrt(1.0f - x2);
}

// ── Drawing primitives ─────────────────────────────────────────────────────

void EyeRenderer::DrawLine(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0, dy = y1 - y0;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;
    dx = (dx > 0) ? dx : -dx;
    dy = (dy > 0) ? dy : -dy;

    if (dx >= dy) {
        int err = dx / 2;
        for (int i = 0; i <= dx; i++) {
            PxSet(x0, y0);
            err -= dy;
            if (err < 0) { y0 += sy; err += dx; }
            x0 += sx;
        }
    } else {
        int err = dy / 2;
        for (int i = 0; i <= dy; i++) {
            PxSet(x0, y0);
            err -= dx;
            if (err < 0) { x0 += sx; err += dy; }
            y0 += sy;
        }
    }
}

// ── Fill sclera (white between lid curves) ────────────────────────────────

void EyeRenderer::FillSclera(float open_top, float open_bot) {
    for (int x = 0; x < W; x++) {
        float dx_norm = (float)(x - EYE_CX) / EYE_HALF_W;
        float shape = AlmondShape(dx_norm);
        if (shape <= 0.0f) continue;

        int top_y = (int)(EYE_CY - open_top * shape);
        int bot_y = (int)(EYE_CY + open_bot * shape);

        for (int y = top_y; y <= bot_y; y++) {
            PxSet(x, y);
        }
    }
}

// ── Blood vessels (dark lines from iris outward toward lids) ──────────────

void EyeRenderer::DrawVessels(float fold, float open_top, float open_bot) {
    if (fold < 0.01f) return;

    int count = 6 + (int)(fold * 4.0f);           // 6-10 vessels
    int thickness = 1 + (int)(fold * 2.0f);        // 1-3px wide
    float start_r = 16.0f;                            // start just outside typical iris
    float max_r = EYE_HALF_W;                       // full length; ClipToLids trims

    for (int v = 0; v < count; v++) {
        float angle = (float)v * 6.2832f / (float)count;
        float ca = std::cos(angle);
        float sa = std::sin(angle);

        for (float r = start_r; r < max_r; r += 1.0f) {
            float px = (float)EYE_CX + r * ca;
            float py = (float)EYE_CY + r * sa;

            // Mild squiggle for organic feel
            int h = (int)(Hash(v, (int)r, 0xB100D) & 7);
            float offset = ((float)h - 3.5f) * 0.5f;
            px += -sa * offset;
            py += ca * offset;

            // Draw with thickness perpendicular to vessel direction
            for (int tw = 0; tw < thickness; tw++) {
                float perp = (float)tw - (float)(thickness - 1) * 0.5f;
                PxClear((int)(px - sa * perp), (int)(py + ca * perp));
            }
        }
    }
}

// ── Limbal ring (dark circle at outer iris edge) ─────────────────────────

void EyeRenderer::DrawLimbalRing(int pupil_r) {
    int iris_r = pupil_r + IRIS_PAD;
    int r2_outer = iris_r * iris_r;
    int r2_inner = (iris_r - 2) * (iris_r - 2);

    for (int y = pupil_cy_ - iris_r; y <= pupil_cy_ + iris_r; y++) {
        for (int x = pupil_cx_ - iris_r; x <= pupil_cx_ + iris_r; x++) {
            int dx = x - pupil_cx_;
            int dy = y - pupil_cy_;
            int d2 = dx * dx + dy * dy;
            if (d2 <= r2_outer && d2 >= r2_inner) {
                PxClear(x, y);
            }
        }
    }
}

// ── Dithered iris texture (stippled gray zone between pupil and limbal ring)

void EyeRenderer::DrawIrisTexture(int pupil_r) {
    int iris_r = pupil_r + IRIS_PAD;
    int r2_outer = (iris_r - 2) * (iris_r - 2);  // inside the limbal ring
    int r2_inner = pupil_r * pupil_r;
    float range = (float)(iris_r - 2 - pupil_r);
    if (range < 1.0f) return;

    for (int y = pupil_cy_ - iris_r; y <= pupil_cy_ + iris_r; y++) {
        for (int x = pupil_cx_ - iris_r; x <= pupil_cx_ + iris_r; x++) {
            int dx = x - pupil_cx_;
            int dy = y - pupil_cy_;
            int d2 = dx * dx + dy * dy;
            if (d2 > r2_outer || d2 < r2_inner) continue;

            // Radial position: 0 at pupil edge, 1 at limbal ring
            float dist = std::sqrt((float)d2);
            float t = (dist - (float)pupil_r) / range;

            // Density: 70% black near pupil, 25% black near sclera
            float density = 0.70f - 0.45f * t;
            uint32_t h = Hash(x, y, 0x1215) & 0xFF;
            if ((float)h < density * 255.0f) {
                PxClear(x, y);
            }
        }
    }
}

// ── Pupil (filled black circle) ───────────────────────────────────────────

void EyeRenderer::ClearPupil(int pupil_r) {
    int r2 = pupil_r * pupil_r;
    for (int y = pupil_cy_ - pupil_r; y <= pupil_cy_ + pupil_r; y++) {
        for (int x = pupil_cx_ - pupil_r; x <= pupil_cx_ + pupil_r; x++) {
            int dx = x - pupil_cx_;
            int dy = y - pupil_cy_;
            if (dx * dx + dy * dy <= r2) {
                PxClear(x, y);
            }
        }
    }
}

// ── Catchlight (specular highlight straddling pupil-iris boundary) ────────

void EyeRenderer::DrawCatchlight(int pupil_r) {
    // Moves 1:1 with pupil — the catchlight is on the cornea, which rotates
    // with the eyeball. Positioned at the pupil edge (upper-right, ~2 o'clock).
    float angle = -0.78f;  // ~45° upper-right
    float edge_x = (float)pupil_cx_ + (float)pupil_r * std::cos(angle);
    float edge_y = (float)pupil_cy_ + (float)pupil_r * std::sin(angle);

    // Size scales with pupil: ~25% of diameter, minimum 2px
    int size = pupil_r / 2;
    if (size < 2) size = 2;
    int r2 = size * size;

    // Primary: filled circle straddling the pupil-iris boundary
    for (int dy = -size; dy <= size; dy++) {
        for (int dx = -size; dx <= size; dx++) {
            if (dx * dx + dy * dy <= r2) {
                PxSet((int)edge_x + dx, (int)edge_y + dy);
            }
        }
    }

    // Secondary: single pixel, opposite quadrant (lower-left), inside pupil
    int sx = pupil_cx_ - pupil_r / 3;
    int sy = pupil_cy_ + pupil_r / 3;
    PxSet(sx, sy);
}

// ── Clip to lids (erase outside + draw lid outlines) ──────────────────────

void EyeRenderer::ClipToLids(float open_top, float open_bot) {
    for (int x = 0; x < W; x++) {
        float dx_norm = (float)(x - EYE_CX) / EYE_HALF_W;
        float shape = AlmondShape(dx_norm);

        int top_y, bot_y;
        if (shape <= 0.0f) {
            top_y = EYE_CY;
            bot_y = EYE_CY;
        } else {
            top_y = (int)(EYE_CY - open_top * shape);
            bot_y = (int)(EYE_CY + open_bot * shape);
        }

        // Clear above top lid
        for (int y = 0; y < top_y && y < H; y++) {
            PxClear(x, y);
        }
        // Clear below bottom lid
        for (int y = bot_y + 1; y < H; y++) {
            PxClear(x, y);
        }
        // Draw lid edge outlines
        if (shape > 0.0f) {
            if (top_y >= 0 && top_y < H) PxSet(x, top_y);
            if (bot_y >= 0 && bot_y < H) PxSet(x, bot_y);
        }
    }
}

// ── Eyelashes (white zigzag lines above top lid) ──────────────────────────

void EyeRenderer::DrawLashes(float open_top, float drive) {
    int count = 5 + (int)(drive * 3.0f);
    float max_len = 3.0f + drive * 6.0f;
    float span = 20.0f;

    for (int i = 0; i < count; i++) {
        // Distribute evenly across lid span
        float t = (count > 1)
            ? -1.0f + 2.0f * (float)i / (float)(count - 1)
            : 0.0f;

        // Attachment point on top lid
        float dx_norm = t * span / EYE_HALF_W;
        float shape = AlmondShape(dx_norm);
        if (shape <= 0.0f) continue;
        float attach_x = (float)EYE_CX + t * span;
        float attach_y = (float)EYE_CY - open_top * shape;

        // Fan angle: center straight up, sides angle outward ±40°
        float angle = -1.5708f + t * 0.7f;
        // Edge falloff: shorter at sides
        float falloff = 1.0f - 0.4f * t * t;
        float len = max_len * falloff;
        if (len < 1.0f) continue;

        float ca = std::cos(angle);
        float sa = std::sin(angle);
        int steps = (int)len;

        int thickness = 1 + (int)(drive * 2.0f);  // 1-3px wide

        for (int s = 0; s <= steps; s++) {
            float px = attach_x + ca * (float)s;
            float py = attach_y + sa * (float)s;

            // Hash-based zigzag perpendicular offset, constant moderate squiggle
            int h = (int)(Hash(i, s, 0x1A5E) & 7);
            float offset = ((float)h - 3.5f) * 0.35f;
            px += -sa * offset;
            py += ca * offset;

            // Draw with thickness perpendicular to lash direction
            for (int tw = 0; tw < thickness; tw++) {
                float perp = (float)tw - (float)(thickness - 1) * 0.5f;
                PxSet((int)(px - sa * perp), (int)(py + ca * perp));
            }
        }
    }
}

// ── Rays (dashed white lines from almond edge outward) ────────────────────

void EyeRenderer::DrawRays(float intensity) {
    if (intensity < 0.01f) return;

    float max_len = 12.0f * intensity;

    for (int i = 0; i < 10; i++) {
        float angle = (float)i * 6.2832f / 10.0f;
        float ca = std::cos(angle);
        float sa = std::sin(angle);

        // Find where the ray exits the almond shape
        // Step outward from center until outside lid curves
        float edge_r = 0.0f;
        for (float r = 2.0f; r < 60.0f; r += 1.0f) {
            float px = (float)EYE_CX + r * ca;
            float py = (float)EYE_CY + r * sa;
            float dx_norm = (px - (float)EYE_CX) / EYE_HALF_W;
            float shape = AlmondShape(dx_norm);
            // Use generous lid opening for ray start (always use max open)
            float top_y = (float)EYE_CY - 24.0f * shape;
            float bot_y = (float)EYE_CY + 24.0f * shape;
            if (shape <= 0.0f || py < top_y || py > bot_y) {
                edge_r = r;
                break;
            }
        }
        if (edge_r < 1.0f) continue;

        // Start ray from edge + 2px gap
        float start_r = edge_r + 2.0f;
        int total_steps = (int)max_len;

        for (int s = 0; s < total_steps; s++) {
            // Dashed pattern: 2px on, 2px off
            if ((s / 2) % 2 != 0) continue;

            float r = start_r + (float)s;
            int px = (int)((float)EYE_CX + r * ca);
            int py = (int)((float)EYE_CY + r * sa);
            PxSet(px, py);
        }
    }
}

// ── 3×5 bitmap font for digits 0-9 ────────────────────────────────────────
// Each digit is 3 columns × 5 rows, stored as column bytes (bit 0 = top row).

static constexpr uint8_t FONT_3X5[10][3] = {
    {0x1F, 0x11, 0x1F},  // 0
    {0x12, 0x1F, 0x10},  // 1
    {0x1D, 0x15, 0x17},  // 2
    {0x15, 0x15, 0x1F},  // 3
    {0x07, 0x04, 0x1F},  // 4
    {0x17, 0x15, 0x1D},  // 5
    {0x1F, 0x15, 0x1D},  // 6
    {0x01, 0x01, 0x1F},  // 7
    {0x1F, 0x15, 0x1F},  // 8
    {0x17, 0x15, 0x1F},  // 9
};

void EyeRenderer::DrawGlyph(int gx, int gy, int digit) {
    if (digit < 0 || digit > 9) return;
    int page = gy / 8;
    int bit_off = gy & 7;

    for (int c = 0; c < 3; c++) {
        int x = gx + c;
        if (x < 0 || x >= W) continue;

        uint16_t col_bits = (uint16_t)FONT_3X5[digit][c] << bit_off;

        // First page: clear 5-bit region then set glyph bits
        if (page >= 0 && page < 8) {
            uint8_t mask = (uint8_t)((uint16_t)0x1F << bit_off);
            buffer_[x + page * W] &= ~mask;
            buffer_[x + page * W] |= (uint8_t)(col_bits & 0xFF);
        }
        // Second page if glyph crosses boundary
        if (bit_off > 3 && page + 1 < 8) {
            uint8_t mask = (uint8_t)(0x1F >> (8 - bit_off));
            buffer_[x + (page + 1) * W] &= ~mask;
            buffer_[x + (page + 1) * W] |= (uint8_t)(col_bits >> 8);
        }
    }
}

void EyeRenderer::DrawNumber(int x, int y, int value) {
    if (value < 0) value = 0;
    if (value > 127) value = 127;

    if (value >= 100) {
        DrawGlyph(x,     y, value / 100);
        DrawGlyph(x + 4, y, (value / 10) % 10);
        DrawGlyph(x + 8, y, value % 10);
    } else if (value >= 10) {
        DrawGlyph(x,     y, value / 10);
        DrawGlyph(x + 4, y, value % 10);
    } else {
        DrawGlyph(x, y, value);
    }
}

void EyeRenderer::DrawCCValues(const Params& p) {
    // Top row: CCs 1-4 (cutoff, drive, sub, fold)
    // Bottom row: CCs 5-8 (decay, amp_env, filt_env, fx)
    const float top[4] = {p.cc_cutoff, p.cc_drive, p.cc_sub, p.cc_fold};
    const float bot[4] = {p.cc_decay, p.cc_amp_env, p.cc_filt_env, p.cc_fx};

    for (int i = 0; i < 4; i++) {
        int x = 2 + i * 32;
        DrawNumber(x,  0, (int)(top[i] * 127.0f));
        DrawNumber(x, 59, (int)(bot[i] * 127.0f));
    }
}

// ── Main render pipeline ───────────────────────────────────────────────────

void EyeRenderer::Render(const Params& p) {
    frame_count_++;

    // ── Ripple wave distortion ──
    ripple_phase_ += 0.12f;
    if (ripple_phase_ > 6.2832f) ripple_phase_ -= 6.2832f;

    float ripple_amp = p.cc_fx * 5.0f;
    if (ripple_amp < 0.01f) {
        std::memset(ripple_offsets_, 0, sizeof(ripple_offsets_));
    } else {
        for (int y = 0; y < H; y++) {
            // Two sine waves at different frequencies and opposite directions
            // for an organic, water-like shimmer
            float wave = std::sin((float)y * 0.18f + ripple_phase_)
                       + 0.5f * std::sin((float)y * 0.31f - ripple_phase_ * 0.7f);
            ripple_offsets_[y] = (int)(ripple_amp * wave * 0.67f);
        }
    }

    // ── Pupil wander (slow Lissajous drift) ──
    float t = (float)frame_count_;
    pupil_cx_ = EYE_CX + (int)(6.0f * std::sin(t * 0.03f));
    pupil_cy_ = EYE_CY + (int)(4.0f * std::sin(t * 0.019f));

    // ── Advance envelopes ──
    float decay_time = 0.1f + p.cc_decay * p.cc_decay * p.cc_decay * 4.9f;

    // Ray envelope: grow while gate on, decay on gate off
    if (gate_) {
        float growth = 1.0f / (decay_time * 20.0f);
        ray_env_ += growth;
        if (ray_env_ > 1.0f) ray_env_ = 1.0f;
    } else {
        ray_env_ *= 0.85f;
        if (ray_env_ < 0.005f) ray_env_ = 0.0f;
    }

    // Lid twitch: decays naturally regardless of gate
    float lid_decay = std::exp(-0.05f / (decay_time * 0.5f));
    lid_env_ *= lid_decay;
    if (lid_env_ < 0.005f) lid_env_ = 0.0f;

    // ── Derive visual parameters ──
    float effective_cut = p.cc_cutoff + lid_env_ * p.cc_filt_env * (1.0f - p.cc_cutoff);
    if (effective_cut > 1.0f) effective_cut = 1.0f;

    float open_top = 2.0f + effective_cut * 22.0f;
    float open_bot = open_top;

    int pupil_r = 7 + (int)(p.cc_sub * 6.0f);
    float ray_intensity = ray_env_ * p.cc_amp_env;

    // ── Render ──
    std::memset(buffer_, 0, BUF_SIZE);

    FillSclera(open_top, open_bot);
    DrawLimbalRing(pupil_r);
    DrawIrisTexture(pupil_r);
    DrawVessels(p.cc_fold, open_top, open_bot);
    ClearPupil(pupil_r);
    DrawCatchlight(pupil_r);
    ClipToLids(open_top, open_bot);
    DrawLashes(open_top, p.cc_drive);
    DrawRays(ray_intensity);
    DrawCCValues(p);
}
