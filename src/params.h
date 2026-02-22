#pragma once
// =============================================================================
// params.h — 8 CC values, scaling curves, and hardcoded defaults
// =============================================================================

#include <cmath>
#include <algorithm>

// MIDI CC assignments
constexpr int CC_CUTOFF   = 1;
constexpr int CC_DRIVE    = 2;
constexpr int CC_SUB      = 3;
constexpr int CC_FOLD     = 4;
constexpr int CC_DECAY    = 5;
constexpr int CC_AMP_ENV  = 6;
constexpr int CC_FILT_ENV = 7;
constexpr int CC_FX       = 8;

// MIDI channel (0-indexed, so channel 1 = 0)
constexpr int MIDI_CHANNEL = 0;

constexpr float ENV_ATTACK_S       = 0.002f;  // 2 ms, always
constexpr float ENV_SUSTAIN        = 0.0f;    // pure AD envelope
constexpr float KEY_TRACKING       = 0.5f;    // 50% key tracking


constexpr float PITCH_BEND_RANGE   = 2.0f;    // semitones
constexpr float MAX_OUTPUT_GAIN     = 2.0f;    // full-CW pot ceiling

// -------------------------------------------------------------------------
// CC Scaling Functions
// -------------------------------------------------------------------------

// CC 1 → Filter cutoff (5 Hz – 18 kHz, exponential)
inline float ScaleCutoff(float cc_norm) {
    return 5.0f * std::pow(18000.0f / 5.0f, cc_norm);
}

// CC 2 → Drive (fast ramp: x^0.6)
inline float ScaleDrive(float cc_norm) {
    return std::pow(cc_norm, 0.6f);
}

// CC 2 → Resonance (exponential — gentle low end, screaming top end)
inline float ScaleResonance(float cc_norm) {
    // exp curve: most of the range is subtle, top 25% is aggressive
    return (std::exp(3.0f * cc_norm) - 1.0f) / (std::exp(3.0f) - 1.0f);
}

// CC 5 → Decay time (5 ms – 5 s, exponential with x^2 skew for short-decay detail)
inline float ScaleDecay(float cc_norm) {
    float curved = cc_norm * cc_norm;
    return 0.005f * std::pow(5.0f / 0.005f, curved);
}

// CC 7 → Filter envelope depth (0 = no effect, 1 = full envelope sweep)
// x^4 curve: fine resolution at low depths for subtle filter movement
inline float ScaleFilterEnvDepth(float cc_norm) {
    float x2 = cc_norm * cc_norm;
    return x2 * x2;
}

// -------------------------------------------------------------------------
// Synth Parameters — the runtime state updated by MIDI CCs
// -------------------------------------------------------------------------

struct Params {
    // Raw 0-1 normalized CC values
    float cc_cutoff   = 1.0f;            // CC 1  (127/127) fully open
    float cc_drive    = 0.0f;            // CC 2  (0/127)   no drive
    float cc_sub      = 40.0f / 127.0f;  // CC 3  (40/127)  light sub
    float cc_fold     = 0.0f;            // CC 4  (0/127)   no fold
    float cc_decay    = 40.0f / 127.0f;  // CC 5  (40/127)
    float cc_amp_env  = 1.0f;            // CC 6  (127/127) full envelope
    float cc_filt_env = 0.0f;            // CC 7  (0/127)   no filter env
    float cc_fx       = 0.0f;            // CC 8  (0/127)   dry
    float cc_gain     = 0.775f;          // pot 8 — 0.775² × 2.0 ≈ 1.2 (audio taper)

    // Pitch bend: -1 to +1
    float pitch_bend = 0.0f;

    // Derived parameters — set by Update() before audio starts
    float cutoff_hz      = 0.0f;
    float drive          = 0.0f;
    float resonance      = 0.0f;
    float sub_level      = 0.0f;
    float fold_amount    = 0.0f;
    float decay_time     = 0.0f;
    float amp_env_depth  = 0.0f;
    float filt_env_depth = 0.0f;
    float overdrive      = 0.0f;
    float output_gain    = 0.0f;

    // Recalculate derived values from raw CCs
    void Update() {
        cutoff_hz      = ScaleCutoff(cc_cutoff);
        drive          = ScaleDrive(cc_drive);
        resonance      = ScaleResonance(cc_drive);
        sub_level      = cc_sub;
        fold_amount    = cc_fold;
        decay_time     = ScaleDecay(cc_decay);
        amp_env_depth  = cc_amp_env;
        filt_env_depth = ScaleFilterEnvDepth(cc_filt_env);
        overdrive      = cc_fx;
        output_gain    = cc_gain * cc_gain * MAX_OUTPUT_GAIN;
    }

    // Handle a MIDI CC message. Returns true if it was one of ours.
    bool HandleCC(int cc_num, int cc_val) {
        float norm = static_cast<float>(cc_val) / 127.0f;
        switch (cc_num) {
            case CC_CUTOFF:   cc_cutoff   = norm; break;
            case CC_DRIVE:    cc_drive    = norm; break;
            case CC_SUB:      cc_sub      = norm; break;
            case CC_FOLD:     cc_fold     = norm; break;
            case CC_DECAY:    cc_decay    = norm; break;
            case CC_AMP_ENV:  cc_amp_env  = norm; break;
            case CC_FILT_ENV: cc_filt_env = norm; break;
            case CC_FX:       cc_fx       = norm; break;
            default: return false;
        }
        Update();
        return true;
    }

    // Handle pitch bend (14-bit, 0-16383, center 8192)
    void HandlePitchBend(int bend_val) {
        pitch_bend = (static_cast<float>(bend_val) - 8192.0f) / 8192.0f;
    }
};
