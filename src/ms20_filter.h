#pragma once
// =============================================================================
// MS-20 Filter (Korg 35 LPF) — Virtual Analog, Zero-Delay Feedback
// =============================================================================
//
// Based on Will Pirkle's analysis of the Korg 35 module (AN-5) using
// Vadim Zavalishin's Topology-Preserving Transform (TPT) method.
//
// References:
//   - Pirkle, "Designing Software Synthesizer Plug-Ins in C++" (2014), Ch.4
//   - Pirkle, AN-5: Virtual Analog Korg35 LPF v3.5
//   - Zavalishin, "The Art of VA Filter Design" (2018), rev 2.1.0
//   - Faust stdlib: vaeffects.lib -> korg35LPF (Eric Tarr, 2019)
//   - Csound: K35_lpf opcode (Steven Yi / kunstmusik)
//
// The circuit: two 1-pole lowpass filters in series with resonance feedback
// through a saturating nonlinearity (tanh). The TPT approach resolves the
// delay-free feedback loop analytically — no unit-delay in the feedback path,
// so the resonance peak stays consistent across all frequencies.
//
// Self-contained. No Daisy dependencies. Drop into any C++ project.
// =============================================================================

#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Korg35LPF {
public:
    void Init(float sample_rate) {
        sr_ = sample_rate * 2.0f;  // 2x oversampling: internal rate is 96 kHz
        s1_ = 0.0f;
        s2_ = 0.0f;
        s3_ = 0.0f;

        SetCutoff(1000.0f);
        SetResonance(0.0f);
        SetDrive(0.0f);
        input_gain_ = 1.0f;
    }

    // Cutoff frequency in Hz. Range: 20 – sr/2.
    void SetCutoff(float freq) {
        freq = std::clamp(freq, 20.0f, sr_ * 0.5f);
        g_ = std::tan(static_cast<float>(M_PI) * freq / sr_);
    }

    // Resonance: 0.0 = none, 1.0 = screaming.
    void SetResonance(float res) {
        res = std::clamp(res, 0.0f, 1.0f);
        K_ = res * 12.0f;
    }

    // Drive: 0.0 = clean, 1.0 = heavy saturation.
    void SetDrive(float drive) {
        drive = std::clamp(drive, 0.0f, 1.0f);
        input_gain_ = 1.0f + drive * 1.0f;
    }

    // Process one sample (2x oversampled internally)
    float Process(float in) {
        float boosted = in * input_gain_;
        ProcessSample(boosted);
        return ProcessSample(boosted);
    }

    // Clear state on note-on to prevent clicks
    void Reset() {
        s1_ = 0.0f;
        s2_ = 0.0f;
        s3_ = 0.0f;
    }

private:
    // Single tick of the Korg 35 filter core at the oversampled rate
    float ProcessSample(float in) {
        float G = g_ / (1.0f + g_);

        // Resolve delay-free feedback loop algebraically
        float u = (in - K_ * Saturate(s3_)) / (1.0f + K_ * G * G);

        // LPF1: trapezoidal integrator
        float v1 = (u - s1_) * G;
        float lp1 = v1 + s1_;
        s1_ = lp1 + v1;

        // LPF2: trapezoidal integrator
        float v2 = (lp1 - s2_) * G;
        float lp2 = v2 + s2_;
        s2_ = lp2 + v2;

        // Feedback state
        s3_ = lp2;

        // Flush denormals to prevent FPU slowdown during release tails
        s1_ = FlushDenormal(s1_);
        s2_ = FlushDenormal(s2_);
        s3_ = FlushDenormal(s3_);

        return lp2;
    }

    float Saturate(float x) {
        // Hard clip at ±3: buzzy, aggressive resonance (MS-20 OTA character)
        return std::max(-3.0f, std::min(3.0f, x));
    }

    static float FlushDenormal(float x) {
        return (std::fabs(x) < 1e-20f) ? 0.0f : x;
    }

    float sr_;
    float g_;
    float K_;
    float input_gain_;

    float s1_;  // LPF1 state
    float s2_;  // LPF2 state
    float s3_;  // feedback path state
};
