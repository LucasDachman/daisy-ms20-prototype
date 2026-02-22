#pragma once
// =============================================================================
// adc_pots.h — Read 9 potentiometers via ADC with smoothing + dead-zone
// =============================================================================
// Header-only. Call AdcPotsInit() once at startup, then AdcPotsRead() each
// iteration of the main loop. Pots write to the same cc_* fields that MIDI
// uses — last write wins, with a dead-zone so a stationary pot doesn't
// overwrite incoming MIDI CCs.
// =============================================================================

#include "daisy_seed.h"
#include "params.h"
#include <cmath>

inline constexpr int NUM_POTS = 9;
inline constexpr float POT_DEAD_ZONE = 0.02f;   // 2% — ~3 steps on 0-127 display
inline constexpr float POT_SMOOTH_ALPHA = 0.3f;  // IIR: 30% new (~150ms settle at 20fps)
inline constexpr float POT_RAW_MIN = 0.01f;      // ADC floor (allow for wiper offset)
inline constexpr float POT_RAW_MAX = 0.95f;      // ADC ceiling (pots read ~955-972 at full CW)

// Persistent state for median filter, smoothing, and dead-zone
static float pot_history[NUM_POTS][3];  // last 3 raw readings for median
static int   pot_hist_idx = 0;          // ring buffer write position
static float pot_smoothed[NUM_POTS];
static float pot_last_sent[NUM_POTS];
static bool  pot_initialized = false;

// Median of three — kills impulse spikes that IIR can't
static inline float Median3(float a, float b, float c) {
    if (a > b) { float t = a; a = b; b = t; }
    if (b > c) { b = c; }
    if (a > b) { a = b; }
    return a > b ? a : b;  // middle value
}

// Map pot index to the Params cc_* field it controls.
// Pots are arranged counter-clockwise starting top-right, matching the
// OLED label positions:
//
//   Top (L→R): CT  DR  SB  FL      Pots (CCW): 3  2  1  0
//               ~~~eye~~~
//   Bot (L→R): DC  AE  FE  FX      Pots (CCW): 4  5  6  7
//
static inline float& PotTarget(Params& p, int pot) {
    switch (pot) {
        case 0: return p.cc_fold;      // A0/D15 — top-right
        case 1: return p.cc_sub;       // A1/D16
        case 2: return p.cc_drive;     // A2/D17
        case 3: return p.cc_cutoff;    // A3/D18 — top-left
        case 4: return p.cc_decay;     // A4/D19 — bottom-left
        case 5: return p.cc_amp_env;   // A5/D20
        case 6: return p.cc_filt_env;  // A6/D21
        case 7: return p.cc_fx;        // A7/D22 — bottom-right
        default: return p.cc_gain;     // A8/D23 — output gain
    }
}

// Configure 9 ADC channels on pins A0–A8 and start background conversion
static inline void AdcPotsInit(daisy::DaisySeed& hw) {
    using namespace daisy;
    AdcChannelConfig cfg[NUM_POTS];
    cfg[0].InitSingle(seed::A0);
    cfg[1].InitSingle(seed::A1);
    cfg[2].InitSingle(seed::A2);
    cfg[3].InitSingle(seed::A3);
    cfg[4].InitSingle(seed::A4);
    cfg[5].InitSingle(seed::A5);
    cfg[6].InitSingle(seed::A6);
    cfg[7].InitSingle(seed::A7);
    cfg[8].InitSingle(seed::A8);
    hw.adc.Init(cfg, NUM_POTS);
    hw.adc.Start();
}

// Read all pots, apply IIR smoothing, update params if pot moved past dead-zone
static inline void AdcPotsRead(daisy::DaisySeed& hw, Params& params) {
    bool changed = false;

    for (int i = 0; i < NUM_POTS; i++) {
        float raw = (hw.adc.GetFloat(i) - POT_RAW_MIN) / (POT_RAW_MAX - POT_RAW_MIN);
        if (raw < 0.0f) raw = 0.0f;
        if (raw > 1.0f) raw = 1.0f;
        pot_history[i][pot_hist_idx] = raw;

        if (!pot_initialized) {
            // First 3 calls: fill history, then snap to physical position
            pot_history[i][0] = pot_history[i][1] = pot_history[i][2] = raw;
            pot_smoothed[i]  = raw;
            pot_last_sent[i] = raw;
            PotTarget(params, i) = raw;
            changed = true;
        } else {
            // Median of last 3 readings — rejects I2C impulse noise
            float med = Median3(pot_history[i][0],
                                pot_history[i][1],
                                pot_history[i][2]);

            // IIR one-pole low-pass on the median value
            pot_smoothed[i] += POT_SMOOTH_ALPHA * (med - pot_smoothed[i]);

            // Only update param if pot moved past dead-zone
            if (std::fabs(pot_smoothed[i] - pot_last_sent[i]) > POT_DEAD_ZONE) {
                pot_last_sent[i] = pot_smoothed[i];
                PotTarget(params, i) = pot_smoothed[i];
                changed = true;
            }
        }
    }

    pot_hist_idx = (pot_hist_idx + 1) % 3;
    if (!pot_initialized) pot_initialized = true;
    if (changed) params.Update();
}
