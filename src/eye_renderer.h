#pragma once
// =============================================================================
// eye_renderer.h — Animated almond-eye display for 128×64 monochrome OLED
// =============================================================================
// Renders a parameterized eye into a 1024-byte framebuffer (SSD130x vertical
// byte packing). No libDaisy dependencies — portable and self-contained.
// =============================================================================

#include <cstdint>
#include <cmath>

struct Params;  // forward declaration (defined in params.h)

class EyeRenderer {
public:
    void Init();
    void NoteOn();
    void NoteOff();
    void Render(const Params& p);
    const uint8_t* Buffer() const { return buffer_; }

private:
    // --- Display constants ---
    static constexpr int W = 128;
    static constexpr int H = 64;
    static constexpr int BUF_SIZE = W * H / 8;  // 1024
    static constexpr int EYE_CX = 64;
    static constexpr int EYE_CY = 32;
    static constexpr int IRIS_R = 14;
    static constexpr float EYE_HALF_W = 24.0f;
    static constexpr float ALMOND_POW = 0.5f;

    // --- Framebuffer ---
    uint8_t buffer_[BUF_SIZE];

    // --- Envelopes and state ---
    float ray_env_;       // 0→1 on note-on (grows), decays on note-off
    float lid_env_;       // 1.0 on note-on, decays naturally
    bool  gate_;
    int   shake_x_, shake_y_;
    uint32_t frame_count_;

    // --- Pixel operations (apply shake offset, bounds-checked) ---
    void PxSet(int x, int y);
    void PxClear(int x, int y);

    // --- Almond shape: returns 0..1 for normalized x distance ---
    float AlmondShape(float dx_norm) const;

    // --- Eye component renderers ---
    void FillSclera(float open_top, float open_bot);
    void DrawVessels(int count, float open_top, float open_bot);
    void DrawIris();
    void ClearPupil(int pupil_r);
    void DrawGlare(int pupil_r);
    void ClipToLids(float open_top, float open_bot);
    void DrawLashes(float open_top, float drive);
    void DrawRays(float intensity);

    // --- Drawing primitives ---
    void DrawLine(int x0, int y0, int x1, int y1);

    // --- Deterministic hash for textures ---
    static uint32_t Hash(int x, int y, uint32_t seed);
};
