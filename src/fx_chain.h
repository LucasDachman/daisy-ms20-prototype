#pragma once
// =============================================================================
// fx_chain.h — Amp-like overdrive with pre/post filtering
// =============================================================================

#include "daisysp.h"

class FxChain {
public:
    void Init(float sample_rate);

    // Process one sample with overdrive amount (0–1)
    float Process(float in, float drive);

private:
    static float AsymClip(float x);
    static float FlushDenormal(float x);

    daisysp::OnePole hp_pre_;   // 80 Hz coupling cap
    daisysp::OnePole lp_post_;  // 5 kHz cabinet sim
    float dc_state_;            // DC blocker accumulator
};
