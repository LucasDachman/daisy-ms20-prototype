// =============================================================================
// fx_chain.cpp — Amp-like overdrive with pre/post filtering
// =============================================================================

#include "fx_chain.h"
#include <cmath>

void FxChain::Init(float sample_rate) {
    hp_pre_.Init();
    hp_pre_.SetFilterMode(daisysp::OnePole::FILTER_MODE_HIGH_PASS);
    hp_pre_.SetFrequency(80.f / sample_rate);

    lp_post_.Init();
    lp_post_.SetFilterMode(daisysp::OnePole::FILTER_MODE_LOW_PASS);
    lp_post_.SetFrequency(5000.f / sample_rate);

    dc_state_ = 0.f;
}

float FxChain::AsymClip(float x) {
    // Positive: gentle saturation; Negative: harder clip at half amplitude
    // Both branches pass through origin → continuous at x=0
    return (x >= 0.f) ? tanhf(x) : tanhf(2.f * x) * 0.5f;
}

float FxChain::FlushDenormal(float x) {
    // Force subnormal floats to zero — prevents CPU stalls on ARM
    union { float f; uint32_t i; } u = {x};
    return (u.i & 0x7F800000) ? x : 0.f;
}

float FxChain::Process(float in, float drive) {
    if (drive < 0.001f) {
        return in;
    }

    // Pre HPF — coupling cap removes sub-bass before clipping
    float sig = hp_pre_.Process(in);

    // Gain stage — quadratic curve for fine control at low drive
    float pre_gain = 1.f + drive * drive * 39.f;
    sig *= pre_gain;

    // Asymmetric soft clip — even harmonics for tube warmth
    sig = AsymClip(sig);

    // Output gain compensation — keep loudness consistent across drive range
    float post_gain = 0.5f / tanhf(0.5f * pre_gain);
    sig *= post_gain;

    // DC blocker (~10 Hz) — removes offset from asymmetric clipping
    constexpr float dc_alpha = 2.f * 3.14159265f * 10.f / 48000.f;
    dc_state_ += dc_alpha * (sig - dc_state_);
    sig -= dc_state_;
    sig = FlushDenormal(sig);

    // Post LPF — cabinet sim softens harsh upper harmonics
    sig = lp_post_.Process(sig);

    return sig;
}
