// =============================================================================
// fx_chain.cpp — Post-filter overdrive
// =============================================================================

#include "fx_chain.h"

void FxChain::Init(float sample_rate) {
    od_.Init();
}

float FxChain::Process(float in, float drive) {
    if (drive < 0.001f) {
        return in;
    }
    od_.SetDrive(drive);
    return od_.Process(in);
}
