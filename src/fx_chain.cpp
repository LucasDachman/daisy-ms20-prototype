// =============================================================================
// fx_chain.cpp — Chorus → Reverb with serial crossfade
// =============================================================================

#include "fx_chain.h"
#include "params.h"

void FxChain::Init(float sample_rate) {
    sr_ = sample_rate;

    // Chorus: gentle movement
    chorus_.Init(sample_rate);
    chorus_.SetLfoFreq(CHORUS_RATE_HZ);
    chorus_.SetLfoDepth(CHORUS_DEPTH);
    chorus_.SetFeedback(CHORUS_FEEDBACK);

    // Reverb: dark, medium-large space
    reverb_.Init(sample_rate);
    reverb_.SetFeedback(REVERB_FEEDBACK);
    reverb_.SetLpFreq(REVERB_LP_FREQ);
}

float FxChain::Process(float in, float fx_mix) {
    // Serial crossfade:
    //   0.0       = fully dry
    //   0.0–0.5   = dry → chorus
    //   0.5       = fully chorus
    //   0.5–1.0   = chorus → reverb
    //   1.0       = fully reverb

    if (fx_mix < 0.001f) {
        return in;  // fully dry, skip processing
    }

    // Always process chorus (needed for both halves of the crossfade)
    float chorus_out = chorus_.Process(in);

    if (fx_mix <= 0.5f) {
        // First half: crossfade dry → chorus
        float wet = fx_mix * 2.0f;  // 0-1 over the 0-0.5 range
        return in * (1.0f - wet) + chorus_out * wet;
    }

    // Second half: crossfade chorus → reverb
    float rev_outL, rev_outR;
    reverb_.Process(chorus_out, chorus_out, &rev_outL, &rev_outR);
    float rev_mono = (rev_outL + rev_outR) * 0.5f;

    float wet = (fx_mix - 0.5f) * 2.0f;  // 0-1 over the 0.5-1.0 range
    return chorus_out * (1.0f - wet) + rev_mono * wet;
}
