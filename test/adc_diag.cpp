// test/adc_diag.cpp — ADC pot diagnostic: shows raw 0-1000 values on OLED
// Build:  make adc-diag
// Flash:  make adc-diag-program-dfu

#include "daisy_seed.h"
#include <cstring>

using namespace daisy;

static DaisySeed hw;
static I2CHandle oled_i2c;
static constexpr uint8_t OLED_ADDR = 0x3C;
static constexpr int NUM_POTS = 9;

// --- SSD1309 driver (same as main.cpp) ---

static void OledCmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    oled_i2c.TransmitBlocking(OLED_ADDR, buf, 2, 10);
}

static void OledInit() {
    OledCmd(0xAE);
    OledCmd(0xD5); OledCmd(0x80);
    OledCmd(0xA8); OledCmd(0x3F);
    OledCmd(0xD3); OledCmd(0x00);
    OledCmd(0x40);
    OledCmd(0x8D); OledCmd(0x14);
    OledCmd(0xA1);
    OledCmd(0xC8);
    OledCmd(0xDA); OledCmd(0x12);
    OledCmd(0x81); OledCmd(0x8F);
    OledCmd(0xD9); OledCmd(0x25);
    OledCmd(0xDB); OledCmd(0x34);
    OledCmd(0xA4);
    OledCmd(0xA6);
    OledCmd(0xAF);
}

static uint8_t page_buf[129];

static void OledSendPage(uint8_t page, const uint8_t* data) {
    OledCmd(0xB0 + page);
    OledCmd(0x00);
    OledCmd(0x10);
    page_buf[0] = 0x40;
    std::memcpy(&page_buf[1], data, 128);
    oled_i2c.TransmitBlocking(OLED_ADDR, page_buf, 129, 50);
}

// --- Framebuffer ---

static uint8_t fb[1024];

static void FbClear() { std::memset(fb, 0, 1024); }

static void FbSet(int x, int y) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    fb[(y / 8) * 128 + x] |= (1 << (y & 7));
}

// --- 3x5 font (same glyphs as eye_renderer.cpp) ---

static constexpr uint8_t FONT[10][3] = {
    {0x1F, 0x11, 0x1F},  // 0
    {0x12, 0x1F, 0x10},  // 1
    {0x1D, 0x15, 0x17},  // 2
    {0x15, 0x15, 0x1F},  // 3
    {0x07, 0x04, 0x1F},  // 4
    {0x17, 0x15, 0x1D},  // 5
    {0x1F, 0x15, 0x1D},  // 6
    {0x01, 0x01, 0x1F},  // 7
    {0x1F, 0x15, 0x1F},  // 8
    {0x17, 0x15, 0x1F},  // 9
};

static void DrawDigit(int x, int y, int d) {
    if (d < 0 || d > 9) return;
    for (int c = 0; c < 3; c++)
        for (int r = 0; r < 5; r++)
            if (FONT[d][c] & (1 << r))
                FbSet(x + c, y + r);
}

static void DrawNum4(int x, int y, int val) {
    DrawDigit(x,      y, (val / 1000) % 10);
    DrawDigit(x + 4,  y, (val / 100) % 10);
    DrawDigit(x + 8,  y, (val / 10) % 10);
    DrawDigit(x + 12, y, val % 10);
}

static void DrawBar(int x0, int x1, int y, int h) {
    for (int x = x0; x <= x1; x++)
        for (int dy = 0; dy < h; dy++)
            FbSet(x, y + dy);
}

// --- Main ---

int main(void) {
    hw.Init();

    // Blink 3x to confirm firmware running
    for (int i = 0; i < 3; i++) {
        hw.SetLed(true);  System::Delay(100);
        hw.SetLed(false); System::Delay(100);
    }

    // ADC: 9 pots on A0-A8
    AdcChannelConfig adc_cfg[NUM_POTS];
    adc_cfg[0].InitSingle(seed::A0);
    adc_cfg[1].InitSingle(seed::A1);
    adc_cfg[2].InitSingle(seed::A2);
    adc_cfg[3].InitSingle(seed::A3);
    adc_cfg[4].InitSingle(seed::A4);
    adc_cfg[5].InitSingle(seed::A5);
    adc_cfg[6].InitSingle(seed::A6);
    adc_cfg[7].InitSingle(seed::A7);
    adc_cfg[8].InitSingle(seed::A8);
    hw.adc.Init(adc_cfg, NUM_POTS);
    hw.adc.Start();

    // OLED: I2C1 at 400 kHz (D11=SCL, D12=SDA)
    I2CHandle::Config i2c_cfg;
    i2c_cfg.periph         = I2CHandle::Config::Peripheral::I2C_1;
    i2c_cfg.speed          = I2CHandle::Config::Speed::I2C_400KHZ;
    i2c_cfg.mode           = I2CHandle::Config::Mode::I2C_MASTER;
    i2c_cfg.pin_config.scl = {DSY_GPIOB, 8};
    i2c_cfg.pin_config.sda = {DSY_GPIOB, 9};
    oled_i2c.Init(i2c_cfg);
    OledInit();

    while (1) {
        FbClear();

        for (int i = 0; i < NUM_POTS; i++) {
            float raw = hw.adc.GetFloat(i);
            int y = i * 7;
            int val = static_cast<int>(raw * 1000.0f + 0.5f);
            if (val > 1000) val = 1000;

            // Pot index (single digit 0-8)
            DrawDigit(0, y, i);

            // Raw value as 0-1000
            DrawNum4(6, y, val);

            // Bar graph: x=22..127 (106px), proportional to raw
            int bar_end = 22 + static_cast<int>(raw * 105.0f);
            if (bar_end > 22) DrawBar(22, bar_end, y + 1, 3);
        }

        for (uint8_t page = 0; page < 8; page++)
            OledSendPage(page, &fb[page * 128]);

        System::Delay(100);
    }
}
