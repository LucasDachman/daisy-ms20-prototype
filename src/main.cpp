// =============================================================================
// main.cpp — DaisyMS20 Prototype
// =============================================================================
// Monophonic MS-20 filter synth. UART MIDI in on D14, audio out on pin 18.
// 8 MIDI CCs control everything. See README.md for details.
// =============================================================================

#include "daisy_seed.h"
#include "daisysp.h"
#include "voice.h"
#include "fx_chain.h"
#include "params.h"

using namespace daisy;

// ---------------------------------------------------------------------------
// Global objects
// ---------------------------------------------------------------------------
static DaisySeed hw;
static MidiUartHandler midi;
static Voice voice;
static Params params;

// FxChain contains ReverbSc which has large internal buffers.
// Place it in SDRAM so it doesn't overflow internal SRAM.
static FxChain DSY_SDRAM_BSS fx;

// ---------------------------------------------------------------------------
// Audio callback — runs at 48 kHz, block size 48
// ---------------------------------------------------------------------------
static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size) {
    for (size_t i = 0; i < size; i++) {
        // Voice: osc → wavefolder → filter → amp envelope
        float sig = voice.Process(params);

        // FX: chorus → reverb crossfade
        sig = fx.Process(sig, params.fx_mix);

        // Mono out (same signal on both channels)
        out[0][i] = sig;
        out[1][i] = sig;
    }
}

// ---------------------------------------------------------------------------
// MIDI message handling
// ---------------------------------------------------------------------------
static void HandleMidiMessage(const MidiEvent& event) {
    // Filter to channel 1 only (0-indexed)
    if (event.channel != MIDI_CHANNEL) return;

    switch (event.type) {
        case NoteOn: {
            auto note = event.AsNoteOn();
            if (note.velocity > 0) {
                voice.NoteOn(note.note);
            } else {
                // Velocity 0 = note off (per MIDI spec)
                voice.NoteOff();
            }
            break;
        }

        case NoteOff:
            voice.NoteOff();
            break;

        case ControlChange: {
            auto cc = event.AsControlChange();
            params.HandleCC(cc.control, cc.value);
            break;
        }

        case PitchBend: {
            auto bend = event.AsPitchBend();
            params.HandlePitchBend(bend.value);
            break;
        }

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
    // Hardware init
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    float sample_rate = hw.AudioSampleRate();

    // Init synth modules
    voice.Init(sample_rate);
    fx.Init(sample_rate);
    params.Update();

    // MIDI: UART on pin D14 (USART1 RX)
    MidiUartHandler::Config midi_cfg;
    midi_cfg.transport_config.rx = DaisySeed::GetPin(14);
    midi_cfg.transport_config.periph =
        UartHandler::Config::Peripheral::USART_1;
    midi.Init(midi_cfg);
    midi.StartReceive();

    // Start audio
    hw.StartAudio(AudioCallback);

    // Main loop: poll MIDI
    while (1) {
        midi.Listen();
        while (midi.HasEvents()) {
            HandleMidiMessage(midi.PopEvent());
        }
    }
}
