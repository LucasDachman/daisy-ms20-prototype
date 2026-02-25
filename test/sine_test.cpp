// sine_test.cpp — Continuous 440 Hz sine wave on both audio outputs
// Useful for verifying the audio DAC, output jack wiring, and amplifier.
// LED stays on while running. No MIDI, no controls.

#include <cmath>
#include "daisy_seed.h"

using namespace daisy;

static DaisySeed hw;

static float phase      = 0.0f;
static float phase_inc  = 0.0f;
static constexpr float FREQ = 440.0f;
static constexpr float AMP  = 0.5f;

static void AudioCallback(AudioHandle::InputBuffer in,
                           AudioHandle::OutputBuffer out,
                           size_t size) {
    for (size_t i = 0; i < size; i++) {
        float sample = AMP * sinf(2.0f * M_PI * phase);
        out[0][i] = sample;
        out[1][i] = sample;
        phase += phase_inc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
}

int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    phase_inc = FREQ / hw.AudioSampleRate();

    // 3 blinks = firmware alive
    for (int i = 0; i < 3; i++) {
        hw.SetLed(true);  System::Delay(150);
        hw.SetLed(false); System::Delay(150);
    }

    hw.SetLed(true);
    hw.StartAudio(AudioCallback);

    while (1) {}
}
