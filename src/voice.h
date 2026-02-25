#pragma once
// =============================================================================
// voice.h — Monophonic voice: saw + sub + wavefolder + MS-20 filter + envelope
// =============================================================================

#include "ms20_filter.h"
#include "params.h"

class Voice {
public:
    void Init(float sample_rate);

    // Trigger a new note (velocity 0–127)
    void NoteOn(int midi_note, int velocity);

    // Release the current note (only if note matches)
    void NoteOff(int midi_note);

    // Process one sample. Reads parameters from the provided Params struct.
    float Process(const Params& p);

    bool IsActive() const { return gate_ || env_value_ > 1e-6f; }

private:
    enum EnvStage { kAttack, kDecay, kRelease };

    // PolyBLEP residual for antialiased saw
    float PolyBlep(float t, float dt);

    // Wavefolder: symmetric, stateless
    float Wavefold(float in, float amount);

    // One-pole smoothed envelope
    float ProcessEnvelope(float attack_s, float decay_s, float sustain, float release_s,
                          EnvStage& stage, float& value);

    float sr_;
    float inv_sr_;

    // Oscillator state
    float saw_phase_;      // 0–1 phasor
    float sub_phase_;      // 0–1 phasor (sine sub, -1 oct)
    float note_freq_;      // Hz, from MIDI note
    int   midi_note_;
    float velocity_;       // 0–1 pregain from MIDI velocity

    // Envelope
    bool     gate_;
    EnvStage env_stage_;
    float    env_value_;          // amp envelope output, 0–1
    EnvStage filt_env_stage_;
    float    filt_env_value_;     // filter envelope output (always sustain=0)

    // Filter
    Korg35LPF filter_;
};
