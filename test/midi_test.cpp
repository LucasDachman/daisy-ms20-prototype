// midi_test.cpp — MIDI message test for UART on D14
// Uses MidiUartHandler to parse MIDI and shows activity on the LED:
//   Note On  → LED on (stays lit while held)
//   Note Off → LED off
//   CC / PitchBend → quick 50ms flash
// Accepts all channels. 3 startup blinks confirm firmware is running.

#include "daisy_seed.h"

using namespace daisy;

static DaisySeed hw;
static MidiUartHandler midi;

int main(void) {
    hw.Init();

    // MIDI UART on D14 (USART1 RX) — same config as main firmware
    MidiUartHandler::Config cfg;
    cfg.transport_config.rx = DaisySeed::GetPin(14);
    cfg.transport_config.periph =
        UartHandler::Config::Peripheral::USART_1;


    midi.Init(cfg);
    USART1->CR2 |= USART_CR2_RXINV;
    midi.StartReceive();

    // 3 blinks = firmware alive
    for (int i = 0; i < 3; i++) {
        hw.SetLed(true);  System::Delay(150);
        hw.SetLed(false); System::Delay(150);
    }

    while (1) {
        midi.Listen();
        while (midi.HasEvents()) {
            MidiEvent ev = midi.PopEvent();
            switch (ev.type) {
                case NoteOn: {
                    auto note = ev.AsNoteOn();
                    if (note.velocity > 0)
                        hw.SetLed(true);
                    else
                        hw.SetLed(false);
                    break;
                }
                case NoteOff:
                    hw.SetLed(false);
                    break;
                case ControlChange:
                case PitchBend:
                    hw.SetLed(true);
                    System::Delay(50);
                    hw.SetLed(false);
                    break;
                default:
                    break;
            }
        }
    }
}
