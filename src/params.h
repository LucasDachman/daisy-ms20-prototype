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

// Hardcoded defaults
constexpr float DEFAULT_CUTOFF       = 1000.0f;  // Hz
constexpr float DEFAULT_DRIVE        = 0.0f;     // 0-1
constexpr float DEFAULT_SUB_LEVEL    = 0.0f;     // 0-1
constexpr float DEFAULT_FOLD_AMOUNT  = 0.0f;     // 0-1
constexpr float DEFAULT_DECAY_TIME   = 0.3f;     // seconds
constexpr float DEFAULT_AMP_ENV      = 1.0f;     // full envelope
constexpr float DEFAULT_FILT_ENV     = 0.0f;     // no filter envelope
constexpr float DEFAULT_FX_MIX       = 0.0f;     // dry

constexpr float ENV_ATTACK_S       = 0.005f;  // 5 ms, always
constexpr float ENV_SUSTAIN        = 0.0f;    // pure AD envelope
constexpr float KEY_TRACKING       = 0.5f;    // 50% key tracking

constexpr float CHORUS_RATE_HZ    = 0.8f;
constexpr float CHORUS_DEPTH       = 0.4f;
constexpr float CHORUS_FEEDBACK    = 0.2f;
constexpr float REVERB_FEEDBACK    = 0.70f;   // "size"
constexpr float REVERB_LP_FREQ     = 4000.0f; // dark tail

constexpr float PITCH_BEND_RANGE   = 2.0f;    // semitones

// -------------------------------------------------------------------------
// CC Scaling Functions
// -------------------------------------------------------------------------

// CC 1 → Filter cutoff (20 Hz – 18 kHz, exponential)
inline float ScaleCutoff(float cc_norm) {
    // Attempt exponential mapping: 20 * (18000/20)^cc = 20 * 900^cc
    return 20.0f * std::pow(900.0f, cc_norm);
}

// CC 2 → Drive (fast ramp: x^0.6)
inline float ScaleDrive(float cc_norm) {
    return std::pow(cc_norm, 0.6f);
}

// CC 2 → Resonance (slow ramp: x^1.8 * 0.85, capped)
inline float ScaleResonance(float cc_norm) {
    return std::pow(cc_norm, 1.8f) * 0.85f;
}

// CC 5 → Decay time (30 ms – 5 s, exponential)
inline float ScaleDecay(float cc_norm) {
    return 0.03f * std::pow(5.0f / 0.03f, cc_norm);
}

// CC 7 → Filter envelope depth in Hz (maps 0-1 to 0-18000 Hz additive range)
inline float ScaleFilterEnvDepth(float cc_norm) {
    return cc_norm * 18000.0f;
}

// -------------------------------------------------------------------------
// Synth Parameters — the runtime state updated by MIDI CCs
// -------------------------------------------------------------------------

struct Params {
    // Raw 0-1 normalized CC values
    float cc_cutoff   = 0.5f;   // CC 1  → ~424 Hz default
    float cc_drive    = 0.0f;   // CC 2
    float cc_sub      = 0.0f;   // CC 3
    float cc_fold     = 0.0f;   // CC 4
    float cc_decay    = 0.3f;   // CC 5
    float cc_amp_env  = 1.0f;   // CC 6
    float cc_filt_env = 0.0f;   // CC 7
    float cc_fx       = 0.0f;   // CC 8

    // Pitch bend: -1 to +1
    float pitch_bend = 0.0f;

    // Derived parameters (call Update() after changing CCs)
    float cutoff_hz      = DEFAULT_CUTOFF;
    float drive          = DEFAULT_DRIVE;
    float resonance      = 0.0f;
    float sub_level      = DEFAULT_SUB_LEVEL;
    float fold_amount    = DEFAULT_FOLD_AMOUNT;
    float decay_time     = DEFAULT_DECAY_TIME;
    float amp_env_depth  = DEFAULT_AMP_ENV;
    float filt_env_depth = DEFAULT_FILT_ENV;
    float fx_mix         = DEFAULT_FX_MIX;

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
        fx_mix         = cc_fx;
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
