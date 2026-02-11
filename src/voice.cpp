// =============================================================================
// voice.cpp — Monophonic voice implementation
// =============================================================================

#include "voice.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -------------------------------------------------------------------------
// MIDI note to frequency
// -------------------------------------------------------------------------
static float MidiToFreq(int note) {
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

// -------------------------------------------------------------------------
// Init
// -------------------------------------------------------------------------
void Voice::Init(float sample_rate) {
    sr_ = sample_rate;
    inv_sr_ = 1.0f / sample_rate;

    saw_phase_ = 0.0f;
    sub_phase_ = 0.0f;
    note_freq_ = 440.0f;
    midi_note_ = 69;

    gate_ = false;
    env_stage_ = kRelease;
    env_value_ = 0.0f;

    filter_.Init(sample_rate);
}

// -------------------------------------------------------------------------
// Note handling
// -------------------------------------------------------------------------
void Voice::NoteOn(int midi_note) {
    midi_note_ = midi_note;
    note_freq_ = MidiToFreq(midi_note);
    gate_ = true;
    env_stage_ = kAttack;

    // Free-running oscillators + envelope retrigger from current level
    // — no phase/state resets, so retriggering is click-free
}

void Voice::NoteOff(int midi_note) {
    if (midi_note == midi_note_) {
        gate_ = false;
    }
}

// -------------------------------------------------------------------------
// PolyBLEP — antialiasing residual for the saw discontinuity
// -------------------------------------------------------------------------
float Voice::PolyBlep(float t, float dt) {
    // t: phase position (0-1), dt: phase increment per sample
    if (t < dt) {
        float x = t / dt;
        return x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

// -------------------------------------------------------------------------
// Wavefolder — symmetric, stateless
// -------------------------------------------------------------------------
float Voice::Wavefold(float in, float amount) {
    if (amount < 0.001f) return in;

    // Gain stage: 1x at 0%, 6x at 100%
    float gained = in * (1.0f + amount * 5.0f);

    // Closed-form triangle-wave fold: O(1), NaN/inf-safe
    // Maps any value into [-1, +1] by "bouncing" off boundaries.
    // Equivalent to a triangle wave with period 4 and amplitude 1.
    gained = gained + 1.0f;                     // shift to [0, 4) range center
    gained = gained - 4.0f * std::floor(gained * 0.25f);  // wrap to [0, 4)
    gained = std::fabs(gained - 2.0f) - 1.0f;  // triangle: [0,4) → [-1,+1]
    return gained;
}

// -------------------------------------------------------------------------
// Simple AD envelope with one-pole smoothing
// -------------------------------------------------------------------------
float Voice::ProcessEnvelope(float attack_s, float decay_s, float sustain, float release_s) {
    float target;
    float time_s;

    if (gate_) {
        if (env_stage_ == kAttack) {
            target = 1.0f;
            time_s = attack_s;
            if (env_value_ >= 0.999f) env_stage_ = kDecay;
        } else {
            // Decay/sustain: ramp to sustain level and hold
            target = sustain;
            time_s = decay_s;
        }
    } else {
        env_stage_ = kRelease;
        target = 0.0f;
        time_s = release_s;
    }

    // One-pole coefficient: 1 - e^(-1 / (time * sr))
    // Clamped to avoid div-by-zero for very short times
    float coeff;
    if (time_s < 0.001f) {
        coeff = 1.0f;
    } else {
        coeff = 1.0f - std::exp(-inv_sr_ / time_s);
    }

    env_value_ += coeff * (target - env_value_);
    return env_value_;
}

// -------------------------------------------------------------------------
// Process one sample
// -------------------------------------------------------------------------
float Voice::Process(const Params& p) {
    if (!IsActive()) return 0.0f;

    // --- Pitch with pitch bend ---
    float freq = note_freq_ * std::pow(2.0f, p.pitch_bend * PITCH_BEND_RANGE / 12.0f);
    float dt = freq * inv_sr_;  // phase increment for main osc

    // --- Saw oscillator (PolyBLEP antialiased) ---
    saw_phase_ += dt;
    if (saw_phase_ >= 1.0f) saw_phase_ -= 1.0f;
    float saw = 2.0f * saw_phase_ - 1.0f;       // naive saw: -1 to +1
    saw -= PolyBlep(saw_phase_, dt);              // apply antialiasing

    // --- Sub oscillator (sine, -1 octave) ---
    float sub_dt = dt * 0.5f;
    sub_phase_ += sub_dt;
    if (sub_phase_ >= 1.0f) sub_phase_ -= 1.0f;
    float sub = std::sin(2.0f * static_cast<float>(M_PI) * sub_phase_);

    // --- Mix ---
    float mix = saw + sub * p.sub_level;

    // --- Wavefolder ---
    float folded = Wavefold(mix, p.fold_amount);

    // --- Envelope ---
    // depth=0: gate (sustain=1, instant release). depth=1: full AD envelope.
    float sustain = 1.0f - p.amp_env_depth;
    float release = std::max(0.002f, p.amp_env_depth * p.decay_time);
    float env = ProcessEnvelope(ENV_ATTACK_S, p.decay_time, sustain, release);

    // --- MS-20 Filter ---
    // Cutoff with key tracking and envelope modulation
    float base_cutoff = p.cutoff_hz;

    // Key tracking: 50% means cutoff shifts by half the interval from middle C
    float semitones_from_c4 = static_cast<float>(midi_note_ - 60);
    float tracking_mult = std::pow(2.0f, KEY_TRACKING * semitones_from_c4 / 12.0f);
    float mod_cutoff = base_cutoff * tracking_mult;

    // Envelope → filter (sweeps UP from cutoff knob toward 10 kHz)
    // depth=0: no effect, depth=1: envelope opens filter fully
    float headroom = std::max(0.0f, 10000.0f - mod_cutoff);
    float filt_env = env * env;  // squared: filter closes faster than amp
    mod_cutoff += filt_env * p.filt_env_depth * headroom;

    // Clamp to valid range
    mod_cutoff = std::clamp(mod_cutoff, 5.0f, sr_ * 0.49f);

    filter_.SetCutoff(mod_cutoff);
    filter_.SetResonance(p.resonance);
    filter_.SetDrive(p.drive);

    float filtered = filter_.Process(folded);

    // --- Amp ---
    float amp = env;

    return filtered * amp;
}
