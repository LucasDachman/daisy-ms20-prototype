#pragma once
// =============================================================================
// voice.h — Monophonic voice: saw + sub + wavefolder + MS-20 filter + envelope
// =============================================================================

#include "ms20_filter.h"
#include "params.h"

class Voice {
public:
    void Init(float sample_rate);

    // Trigger a new note
    void NoteOn(int midi_note);

    // Release the current note
    void NoteOff();

    // Process one sample. Reads parameters from the provided Params struct.
    float Process(const Params& p);

    bool IsActive() const { return gate_ || env_value_ > 0.001f; }

private:
    // PolyBLEP residual for antialiased saw
    float PolyBlep(float t, float dt);

    // Wavefolder: symmetric, stateless
    float Wavefold(float in, float amount);

    // Simple AD envelope (one-pole smoothed)
    float ProcessEnvelope(float attack_s, float decay_s);

    float sr_;
    float inv_sr_;

    // Oscillator state
    float saw_phase_;      // 0–1 phasor
    float sub_phase_;      // 0–1 phasor (sine sub, -1 oct)
    float note_freq_;      // Hz, from MIDI note
    int   midi_note_;

    // Envelope
    bool  gate_;
    float env_value_;      // current envelope output, 0–1

    // Filter
    Korg35LPF filter_;
};
