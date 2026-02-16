// uart_test.cpp — Raw UART byte counter for MIDI debugging
// Configures UART on D14 at 31250 baud and counts received bytes.
// After each 2-second window: blinks N times (capped at 20).
// No blinks = UART not receiving. 20 blinks = data flowing.

#include "daisy_seed.h"

using namespace daisy;

static DaisySeed hw;
static UartHandler uart;

int main(void) {
    hw.Init();

    UartHandler::Config cfg;
    cfg.periph    = UartHandler::Config::Peripheral::USART_1;
    cfg.baudrate  = 31250;
    cfg.stopbits  = UartHandler::Config::StopBits::BITS_1;
    cfg.parity    = UartHandler::Config::Parity::NONE;
    cfg.wordlength = UartHandler::Config::WordLength::BITS_8;
    cfg.pin_config.rx = DaisySeed::GetPin(14);
    cfg.pin_config.tx = Pin();  // RX only
    cfg.mode      = UartHandler::Config::Mode::RX;
    uart.Init(cfg);

    // Blink 3x to confirm firmware is running
    for (int i = 0; i < 3; i++) {
        hw.SetLed(true);  System::Delay(150);
        hw.SetLed(false); System::Delay(150);
    }

    while (1) {
        uint32_t count = 0;
        uint32_t start = System::GetNow();
        while (System::GetNow() - start < 2000) {
            uint8_t byte;
            if (uart.PollReceive(&byte, 1, 1) == 0) {
                count++;
            }
        }

        uint32_t blinks = count > 20 ? 20 : count;
        for (uint32_t i = 0; i < blinks; i++) {
            hw.SetLed(true);  System::Delay(80);
            hw.SetLed(false); System::Delay(80);
        }
        System::Delay(500);
    }
}
