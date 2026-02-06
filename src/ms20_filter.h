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

class Korg35LPF {
public:
    void Init(float sample_rate) {
        sr_ = sample_rate;
        s1_ = 0.0f;
        s2_ = 0.0f;
        s3_ = 0.0f;

        SetCutoff(1000.0f);
        SetResonance(0.0f);
        SetDrive(0.0f);
        saturation_ = 1.0f;
    }

    // Cutoff frequency in Hz. Range: 20 – sr/2.
    void SetCutoff(float freq) {
        freq = std::clamp(freq, 20.0f, sr_ * 0.5f);
        g_ = std::tan(static_cast<float>(M_PI) * freq / sr_);
    }

    // Resonance: 0.0 = none, 1.0 = self-oscillation.
    void SetResonance(float res) {
        res = std::clamp(res, 0.0f, 1.0f);
        K_ = res * 2.0f;
    }

    // Drive: 0.0 = clean, 1.0 = heavy saturation in feedback path.
    void SetDrive(float drive) {
        drive = std::clamp(drive, 0.0f, 1.0f);
        saturation_ = 1.0f + drive * 7.0f;
    }

    // Process one sample
    float Process(float in) {
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

        return lp2;
    }

    // Clear state on note-on to prevent clicks
    void Reset() {
        s1_ = 0.0f;
        s2_ = 0.0f;
        s3_ = 0.0f;
    }

private:
    float Saturate(float x) {
        return std::tanh(saturation_ * x);
    }

    float sr_;
    float g_;
    float K_;
    float saturation_;

    float s1_;  // LPF1 state
    float s2_;  // LPF2 state
    float s3_;  // feedback path state
};
