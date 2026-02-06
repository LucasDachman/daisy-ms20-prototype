#pragma once
// =============================================================================
// fx_chain.h — Chorus → Reverb with serial crossfade
// =============================================================================
//
// Uses DaisySP Chorus and ReverbSc. Single crossfade knob:
//   0%   = dry
//   50%  = full chorus
//   100% = full reverb
//

#include "daisysp.h"
#include "Effects/reverbsc.h"

class FxChain {
public:
    // Must be called after hardware init (ReverbSc uses SDRAM)
    void Init(float sample_rate);

    // Process one sample with crossfade position (0–1)
    float Process(float in, float fx_mix);

private:
    daisysp::Chorus  chorus_;
    daisysp::ReverbSc reverb_;

    float sr_;
};
