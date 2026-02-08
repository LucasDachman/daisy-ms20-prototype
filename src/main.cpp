// =============================================================================
// main.cpp — DaisyMS20 Prototype
// =============================================================================
// Monophonic MS-20 filter synth. MIDI in via USB and UART (D14).
// Audio out on pin 18. 8 MIDI CCs control everything. See README.md.
// =============================================================================

#include <cmath>
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
static MidiUartHandler midi_uart;
static MidiUsbHandler  midi_usb;
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

        // Output gain (no soft clip — let resonance peaks clip naturally)
        sig *= OUTPUT_GAIN;

        // Mono out (same signal on both channels)
        out[0][i] = sig;
        out[1][i] = sig;
    }
}

// ---------------------------------------------------------------------------
// MIDI message handling
// ---------------------------------------------------------------------------
static void HandleMidiMessage(MidiEvent event) {
    // Filter to channel 1 only (0-indexed)
    if (event.channel != MIDI_CHANNEL) return;

    // Blink LED on any MIDI message
    hw.SetLed(true);

    switch (event.type) {
        case NoteOn: {
            auto note = event.AsNoteOn();
            if (note.velocity > 0) {
                voice.NoteOn(note.note);
                hw.SetLed(true);
            } else {
                voice.NoteOff(note.note);
                hw.SetLed(false);
            }
            break;
        }

        case NoteOff: {
            auto note = event.AsNoteOn();
            voice.NoteOff(note.note);
            hw.SetLed(false);
            break;
        }

        case ControlChange: {
            auto cc = event.AsControlChange();
            params.HandleCC(cc.control_number, cc.value);
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
    MidiUartHandler::Config uart_cfg;
    uart_cfg.transport_config.rx = DaisySeed::GetPin(14);
    uart_cfg.transport_config.periph =
        UartHandler::Config::Peripheral::USART_1;
    midi_uart.Init(uart_cfg);
    midi_uart.StartReceive();

    // MIDI: USB (class-compliant, no driver needed)
    MidiUsbHandler::Config usb_cfg;
    midi_usb.Init(usb_cfg);
    midi_usb.StartReceive();

    // Start audio
    hw.StartAudio(AudioCallback);

    // Main loop: poll both MIDI inputs
    while (1) {
        midi_uart.Listen();
        while (midi_uart.HasEvents()) {
            HandleMidiMessage(midi_uart.PopEvent());
        }

        midi_usb.Listen();
        while (midi_usb.HasEvents()) {
            HandleMidiMessage(midi_usb.PopEvent());
        }
    }
}
