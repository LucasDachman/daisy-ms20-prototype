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
    shake_x_ = 0;
    shake_y_ = 0;
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
    x += shake_x_;
    y += shake_y_;
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    buffer_[x + (y / 8) * W] |= (1 << (y & 7));
}

void EyeRenderer::PxClear(int x, int y) {
    x += shake_x_;
    y += shake_y_;
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

void EyeRenderer::DrawVessels(int count, float open_top, float open_bot) {
    if (count <= 0) return;

    for (int v = 0; v < count; v++) {
        float angle = (float)v * 6.2832f / (float)count;
        float ca = std::cos(angle);
        float sa = std::sin(angle);

        float r = 10.0f;
        for (int step = 0; step < 20; step++) {
            float px = (float)EYE_CX + r * ca;
            float py = (float)EYE_CY + r * sa;

            // Squiggle: hash-based perpendicular offset ±2-3px
            int h = (int)(Hash(v, step, 0xB100D) & 7);
            float offset = ((float)h - 3.5f) * 0.8f;
            px += -sa * offset;
            py += ca * offset;

            int ix = (int)px;
            int iy = (int)py;

            // 3x3 block for bold rendering
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    PxClear(ix + dx, iy + dy);

            r += 1.0f;
        }
    }
}

// ── Iris ring (2px black circle outline) ──────────────────────────────────

void EyeRenderer::DrawIris() {
    int r_outer = IRIS_R;
    int r_inner = IRIS_R - 2;
    int r2_outer = r_outer * r_outer;
    int r2_inner = r_inner * r_inner;

    for (int y = EYE_CY - r_outer; y <= EYE_CY + r_outer; y++) {
        for (int x = EYE_CX - r_outer; x <= EYE_CX + r_outer; x++) {
            int dx = x - EYE_CX;
            int dy = y - EYE_CY;
            int d2 = dx * dx + dy * dy;
            if (d2 <= r2_outer && d2 >= r2_inner) {
                PxClear(x, y);
            }
        }
    }
}

// ── Pupil (filled black circle) ───────────────────────────────────────────

void EyeRenderer::ClearPupil(int pupil_r) {
    int r2 = pupil_r * pupil_r;
    for (int y = EYE_CY - pupil_r; y <= EYE_CY + pupil_r; y++) {
        for (int x = EYE_CX - pupil_r; x <= EYE_CX + pupil_r; x++) {
            int dx = x - EYE_CX;
            int dy = y - EYE_CY;
            if (dx * dx + dy * dy <= r2) {
                PxClear(x, y);
            }
        }
    }
}

// ── Glare (two white dots on pupil) ───────────────────────────────────────

void EyeRenderer::DrawGlare(int pupil_r) {
    // Primary: 2×2 block at upper-right of pupil
    int hx = EYE_CX + pupil_r / 3;
    int hy = EYE_CY - pupil_r / 3;
    PxSet(hx, hy);
    PxSet(hx + 1, hy);
    PxSet(hx, hy - 1);
    PxSet(hx + 1, hy - 1);
    // Secondary: single pixel at lower-left
    PxSet(hx - 3, hy + 3);
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
    int count = 5 + (int)(drive * 8.0f);
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

        for (int s = 0; s <= steps; s++) {
            float px = attach_x + ca * (float)s;
            float py = attach_y + sa * (float)s;

            // Hash-based zigzag perpendicular offset, scaled by drive
            if (drive > 0.01f) {
                int h = (int)(Hash(i, s, 0x1A5E) & 7);
                float offset = ((float)h - 3.5f) * drive * 0.86f;
                px += -sa * offset;
                py += ca * offset;
            }

            PxSet((int)px, (int)py);
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

// ── Main render pipeline ───────────────────────────────────────────────────

void EyeRenderer::Render(const Params& p) {
    frame_count_++;

    // ── Screen shake ──
    if (p.cc_fx > 0.01f) {
        int mag = (int)(p.cc_fx * 4.0f);
        uint32_t hx = Hash((int)frame_count_, 0, 0x5EA4);
        uint32_t hy = Hash((int)frame_count_, 1, 0x5EA5);
        shake_x_ = (int)(hx % (2 * mag + 1)) - mag;
        shake_y_ = (int)(hy % (2 * mag + 1)) - mag;
    } else {
        shake_x_ = 0;
        shake_y_ = 0;
    }

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
    int vessel_count = (int)(p.cc_fold * 10.0f);
    float ray_intensity = ray_env_ * p.cc_amp_env;

    // ── Render ──
    std::memset(buffer_, 0, BUF_SIZE);

    FillSclera(open_top, open_bot);
    DrawVessels(vessel_count, open_top, open_bot);
    ClearPupil(pupil_r);
    DrawGlare(pupil_r);
    ClipToLids(open_top, open_bot);
    DrawLashes(open_top, p.cc_drive);
    DrawRays(ray_intensity);
}
