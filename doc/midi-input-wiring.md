# MIDI Input Wiring

Daisy Seed + 5-pin DIN jack + PC817 optocoupler.

## LED side (MIDI in)

```
DIN Pin 4 --[220R]---> PC817 Pin 1 (Anode)
DIN Pin 5 -----------> PC817 Pin 2 (Cathode)
```

## Phototransistor side (Daisy)

```
3V3 --[4.7k]--+-- PC817 Pin 4 (Collector)
              |
              +-- D14 (UART RX)

GND ------------ PC817 Pin 3 (Emitter)
```

## Notes

- **Optocoupler** isolates MIDI sender ground from Daisy ground (prevents ground loops).
- **220R** limits LED current. Sender has another 220R, so ~11 mA total at 5V.
- **4.7k pull-up** holds D14 HIGH when idle. Phototransistor pulls LOW for data bits.
- **Common-emitter** config: emitter to GND, collector to output. No inversion needed for UART.
- MIDI baud rate: **31,250**. Configured on Daisy UART1 (pin D14).
- Optional **1N4148** diode across pins 1-2 (reverse-biased) protects against miswired cables.
