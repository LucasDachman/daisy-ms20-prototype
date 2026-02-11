#pragma once
// =============================================================================
// fx_chain.h — Post-filter overdrive
// =============================================================================

#include "daisysp.h"

class FxChain {
public:
    void Init(float sample_rate);

    // Process one sample with overdrive amount (0–1)
    float Process(float in, float drive);

private:
    daisysp::Overdrive od_;
};
