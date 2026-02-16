# OLED Wiring

SSD1309 128x64 I2C OLED on Daisy Seed I2C1.

## Connections

```
Daisy Pin 13 (D12 / PB9) ----> OLED SDA
Daisy Pin 12 (D11 / PB8) ----> OLED SCL
3V3 -------------------------> OLED VDD
GND -------------------------> OLED GND
```

## Notes

- I2C address: **0x3C** (7-bit). libDaisy left-shifts internally before HAL call.
- Bus speed: **400 kHz**. This module does not respond at 1 MHz.
- Most modules have **4.7k pull-ups** on board. No external resistors needed.
- Pin 12 is D11, pin 13 is D12. Count header pins carefully — off-by-one is easy to miss.
- Init audio **before** display. I2C init blocks ~30s if the device is not connected.
