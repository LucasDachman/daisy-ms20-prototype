// =============================================================================
// main.cpp — DaisyMS20 Prototype
// =============================================================================
// Monophonic MS-20 filter synth. MIDI in via USB and UART (D14).
// Audio out on pin 18. 8 MIDI CCs control everything. See README.md.
// =============================================================================

#include <cmath>
#include <cstring>
#include "daisy_seed.h"
#include "daisysp.h"
#include "voice.h"
#include "fx_chain.h"
#include "params.h"
#include "eye_renderer.h"
#include "adc_pots.h"

using namespace daisy;

// ---------------------------------------------------------------------------
// Global objects
// ---------------------------------------------------------------------------
static DaisySeed hw;
static MidiUartHandler midi_uart;
static MidiUsbHandler  midi_usb;
static Voice voice;
static Params params;

static FxChain fx;

// Eye display
static EyeRenderer eye;
static I2CHandle   oled_i2c;
static constexpr uint8_t OLED_ADDR = 0x3C;

// ---------------------------------------------------------------------------
// Minimal SSD1309 driver — batched page writes for fast, non-starving I2C
// ---------------------------------------------------------------------------
// libDaisy's SSD130x driver sends 1 byte per I2C transaction (74ms per
// frame at 400 kHz). This driver sends 128 bytes per transaction (~3ms per
// page), and the main loop polls MIDI between pages.
// ---------------------------------------------------------------------------

static void OledCmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    oled_i2c.TransmitBlocking(OLED_ADDR, buf, 2, 10);
}

static void OledInit() {
    // Standard SSD1306/SSD1309 128×64 init sequence
    OledCmd(0xAE);        // display off
    OledCmd(0xD5); OledCmd(0x80);  // clock divide
    OledCmd(0xA8); OledCmd(0x3F);  // multiplex 64
    OledCmd(0xD3); OledCmd(0x00);  // display offset 0
    OledCmd(0x40);        // start line 0
    OledCmd(0x8D); OledCmd(0x14);  // charge pump on
    OledCmd(0xA1);        // segment remap
    OledCmd(0xC8);        // COM scan descending
    OledCmd(0xDA); OledCmd(0x12);  // COM pins
    OledCmd(0x81); OledCmd(0x8F);  // contrast
    OledCmd(0xD9); OledCmd(0x25);  // pre-charge
    OledCmd(0xDB); OledCmd(0x34);  // VCOMH deselect
    OledCmd(0xA4);        // resume from RAM
    OledCmd(0xA6);        // normal display (not inverted)
    OledCmd(0xAF);        // display on
}

// Send one page (128 bytes) in a single I2C transaction.
// Returns in ~3ms at 400 kHz — short enough to not starve MIDI.
static uint8_t page_buf[129];  // 0x40 prefix + 128 data bytes

static void OledSendPage(uint8_t page, const uint8_t* data) {
    OledCmd(0xB0 + page);  // set page
    OledCmd(0x00);         // low column = 0
    OledCmd(0x10);         // high column = 0
    page_buf[0] = 0x40;   // I2C data mode prefix
    std::memcpy(&page_buf[1], data, 128);
    oled_i2c.TransmitBlocking(OLED_ADDR, page_buf, 129, 50);
}

// ---------------------------------------------------------------------------
// Audio callback — runs at 48 kHz, block size 48
// ---------------------------------------------------------------------------
static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size) {
    for (size_t i = 0; i < size; i++) {
        float sig = voice.Process(params);
        sig = fx.Process(sig, params.overdrive);
        sig *= params.output_gain;
        out[0][i] = sig;
        out[1][i] = sig;
    }
}

// ---------------------------------------------------------------------------
// MIDI polling helper — call frequently to avoid buffer overflow
// ---------------------------------------------------------------------------
static void PollMidi() {
    midi_uart.Listen();
    while (midi_uart.HasEvents()) {
        MidiEvent event = midi_uart.PopEvent();
        if (event.channel != MIDI_CHANNEL) continue;
        hw.SetLed(true);
        switch (event.type) {
            case NoteOn: {
                auto note = event.AsNoteOn();
                if (note.velocity > 0) {
                    voice.NoteOn(note.note, note.velocity);
                    eye.NoteOn();
                } else {
                    voice.NoteOff(note.note);
                    eye.NoteOff();
                    hw.SetLed(false);
                }
                break;
            }
            case NoteOff: {
                auto note = event.AsNoteOn();
                voice.NoteOff(note.note);
                eye.NoteOff();
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
            default: break;
        }
    }

    midi_usb.Listen();
    while (midi_usb.HasEvents()) {
        MidiEvent event = midi_usb.PopEvent();
        if (event.channel != MIDI_CHANNEL) continue;
        hw.SetLed(true);
        switch (event.type) {
            case NoteOn: {
                auto note = event.AsNoteOn();
                if (note.velocity > 0) {
                    voice.NoteOn(note.note, note.velocity);
                    eye.NoteOn();
                } else {
                    voice.NoteOff(note.note);
                    eye.NoteOff();
                    hw.SetLed(false);
                }
                break;
            }
            case NoteOff: {
                auto note = event.AsNoteOn();
                voice.NoteOff(note.note);
                eye.NoteOff();
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
            default: break;
        }
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    float sample_rate = hw.AudioSampleRate();

    voice.Init(sample_rate);
    fx.Init(sample_rate);
    params.Update();

    // MIDI: UART on pin D14 (USART1 RX)
    MidiUartHandler::Config uart_cfg;
    uart_cfg.transport_config.rx = DaisySeed::GetPin(14);
    uart_cfg.transport_config.periph =
        UartHandler::Config::Peripheral::USART_1;
    midi_uart.Init(uart_cfg);
    // Optocoupler inverts MIDI signal; flip RX polarity in hardware
    USART1->CR2 |= USART_CR2_RXINV;
    midi_uart.StartReceive();

    // MIDI: USB
    MidiUsbHandler::Config usb_cfg;
    midi_usb.Init(usb_cfg);
    midi_usb.StartReceive();

    eye.Init();

    // Start audio first — runs at interrupt priority
    hw.StartAudio(AudioCallback);

    // ADC: 9 pots on A0–A8 (init after audio to avoid DMA conflict)
    AdcPotsInit(hw);

    // OLED display: I2C1 at 400 kHz (D11=SCL, D12=SDA)
    if (EyeRenderer::ENABLED) {
        I2CHandle::Config i2c_cfg;
        i2c_cfg.periph         = I2CHandle::Config::Peripheral::I2C_1;
        i2c_cfg.speed          = I2CHandle::Config::Speed::I2C_400KHZ;
        i2c_cfg.mode           = I2CHandle::Config::Mode::I2C_MASTER;
        i2c_cfg.pin_config.scl = {DSY_GPIOB, 8};
        i2c_cfg.pin_config.sda = {DSY_GPIOB, 9};
        oled_i2c.Init(i2c_cfg);
        OledInit();
    }

    // Main loop
    uint32_t last_frame = 0;

    while (1) {
        PollMidi();

        uint32_t now = System::GetNow();
        if (now - last_frame >= 50) {  // ~20 fps target
            last_frame = now;

            AdcPotsRead(hw, params);

            if (EyeRenderer::ENABLED) {

                eye.Render(params);
                const uint8_t* buf = eye.Buffer();

                // Send 8 pages, polling MIDI between each (~3ms per page)
                for (uint8_t page = 0; page < 8; page++) {
                    OledSendPage(page, &buf[page * 128]);
                    PollMidi();
                }
            }
        }
    }
}
