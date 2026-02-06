# Daisy Synth — Prototype Specification

-----

## 1. Overview

A **monophonic dark industrial synthesizer** on the Daisy Seed. One voice, one oscillator, one filter, two effects, eight knobs. The purpose is to prototype the MS-20 filter emulation — the hardest DSP component in the full design — inside a minimal playable instrument.

**Signal flow:**

```
SYNTAKT MIDI OUT ──→ 5-pin DIN ──→ MIDI Input Circuit ──→ Daisy UART RX (D14)

SAW OSC ──┐
           ├──→ Wavefolder → MS-20 Filter (LP) → Amp Env → Chorus → Reverb → Mono Out
SUB OSC ──┘                                                                      │
                                                                          1/4" jack
                                                                              │
                                                                     SYNTAKT AUDIO IN
```

**8 MIDI CC controls — nothing else:**

|CC|Knob    |Parameter                         |Range                          |
|--|--------|----------------------------------|-------------------------------|
|1 |Cutoff  |MS-20 filter cutoff frequency     |20 Hz – 18 kHz                 |
|2 |Drive   |Input drive + linked resonance    |0–100% (see §4)                |
|3 |Sub     |Sub oscillator level              |0–100%                         |
|4 |Fold    |Wavefolder amount                 |0–100%                         |
|5 |Decay   |Shared envelope decay/release time|30 ms – 5 s                    |
|6 |Amp Env |Envelope → amp depth              |0–100%                         |
|7 |Filt Env|Envelope → filter depth           |0–100%                         |
|8 |FX      |Chorus ↔ Reverb crossfade         |0%=dry, 50%=chorus, 100%=reverb|

Everything else is hardcoded to sensible defaults. No patch storage, no mod matrix, no macros, no waveform selection. The goal is: receive MIDI from the Syntakt over a 5-pin DIN cable, hear sound through the Syntakt's audio input, tweak 8 CCs, evaluate the MS-20 filter character.

-----

## 2. Oscillator

### 2.1 Main Oscillator

**Hardcoded saw wave.** PolyBLEP antialiased. Pitch tracks MIDI note number. No octave select, no detune, no waveform switching — just a saw at concert pitch.

Why saw: full harmonic series gives the MS-20 filter maximum material to work with, and the wavefolder knob provides the route from clean to aggressive. Saw through the MS-20 is the thickest, most aggressive combination.

### 2.2 Sub Oscillator

Sine wave, one octave below the main oscillator. Mixed in parallel before the wavefolder. Level controlled by **CC 3** (0–100%).

At 0% the sub is silent. At 100% it adds a full-amplitude sine one octave down, giving the sound low-end weight without adding harmonic complexity above the fundamental.

### 2.3 Mixing

The saw oscillator output is fixed at unity gain. The sub oscillator level is variable via CC. The two signals are summed and fed into the wavefolder.

-----

## 3. Wavefolder

|Parameter  |Control                  |Notes                                      |
|-----------|-------------------------|-------------------------------------------|
|Fold Amount|**CC 4** (0–100%)        |Pre-gain into folding stages               |
|Symmetry   |Hardcoded: 0% (symmetric)|Symmetric folding for odd-harmonic emphasis|

At 0% fold the signal passes through unchanged. As fold increases, the waveform is progressively folded back on itself, adding upper harmonics. With a saw input this produces increasingly buzzy, aggressive overtones. At extreme settings the output is dense and metallic.

The wavefolder sits before the filter, so the MS-20 can then shape (or pass through) the added harmonics.

-----

## 4. MS-20 Filter

The core of the prototype. An oversampled Korg 35 model (two cascaded one-pole filters with saturated feedback via TPT/ZDF method), emulating the Korg MS-20's characteristic aggressive, screaming filter behavior.

### 4.1 Parameters

|Parameter   |Control                       |Notes                                                     |
|------------|------------------------------|----------------------------------------------------------|
|Mode        |Hardcoded: **LP**             |Low-pass, 12 dB/oct                                       |
|Cutoff      |**CC 1** (20 Hz – 18 kHz)     |Exponential mapping                                       |
|Resonance   |**Linked to CC 2** (see below)|0–100%, self-oscillates near max                          |
|Input Drive |**CC 2** (0–100%)             |MS-20 input clipper saturation                            |
|Key Tracking|Hardcoded: **50%**            |Cutoff partially follows pitch                            |
|Env Depth   |**CC 7** (0–100%)             |Envelope → filter amount (unipolar positive for prototype)|

### 4.2 Drive + Resonance Link

CC 2 controls both input drive and resonance simultaneously, with **drive leading**:

```
CC value:    0%  ──────────── 50% ──────────── 100%
Drive:       0%  ──────────── 80% ──────────── 100%
Resonance:   0%  ──────────── 20% ──────────── 85%
```

At low CC values, drive ramps up quickly while resonance stays low — you get grit and saturation without screaming. Past the halfway point, resonance catches up and the filter starts to sing and scream. At maximum, both are near full: heavily saturated input with aggressive self-oscillation. This single knob gives you the full journey from clean to industrial chaos.

The curves are:

- **Drive:** fast ramp — `drive = cc_value ^ 0.6` (concave, front-loaded)
- **Resonance:** slow ramp — `resonance = cc_value ^ 1.8 * 0.85` (convex, back-loaded, capped at 85%)

### 4.3 Implementation

Oversampled 2× (processing at 96 kHz internally). Nonlinear elements: tanh-approximation soft clipping in feedback path, asymmetric input clipper. With only one voice, CPU cost is negligible — roughly 3–4% of the total budget. This is the component to get right; everything else in the prototype exists to support evaluating it.

-----

## 5. Envelope

A **single shared envelope** controls both amp and filter, with independent depth knobs for each destination. This gives full envelope expression with just 3 CC knobs instead of the 8+ that separate ADSRs would require.

### 5.1 Shape

|Stage  |Control            |Range                                     |
|-------|-------------------|------------------------------------------|
|Attack |Hardcoded: **5 ms**|Fast attack — always snappy               |
|Decay  |**CC 5**           |30 ms – 5 s                               |
|Sustain|Hardcoded: **0%**  |Fully decaying envelope (plucky character)|
|Release|**Linked to CC 5** |Same value as decay                       |

This is a **percussive AD envelope** (attack-decay with no sustain). The decay and release share the same time value from CC 5. Short values give plucky, percussive hits; long values give slow, evolving swells. Sustain is hardcoded at 0% to keep the envelope simple and punchy — this is a prototype, and the full ADSR lives in v1.

### 5.2 Routing

|Destination  |Control          |Behavior                                                                                                                         |
|-------------|-----------------|---------------------------------------------------------------------------------------------------------------------------------|
|Amp          |**CC 6** (0–100%)|At 100% the note fully decays to silence. At 0% the amp is constant (organ-like sustain, note only ends on Note Off).            |
|Filter Cutoff|**CC 7** (0–100%)|Adds the envelope on top of the CC 1 cutoff value. At 0% the filter is static. At 100% the envelope sweeps the full cutoff range.|

The amp envelope depth at 0% means the amp stays fully open and the note sustains indefinitely until Note Off — effectively turning off the envelope for the amp while still using it for filter sweeps. This gives the single envelope dual personality: at CC 6 = 100%, CC 7 = 0% it's a pure amp envelope (plucky); at CC 6 = 0%, CC 7 = 100% it's a pure filter envelope (sustained note with filter sweep); anywhere in between, it's both.

-----

## 6. Effects

Two effects in series: **chorus → reverb**, controlled by a single crossfade knob.

### 6.1 Chorus

|Parameter|Value |Notes    |
|---------|------|---------|
|Rate     |0.8 Hz|Hardcoded|
|Depth    |40%   |Hardcoded|
|Feedback |20%   |Hardcoded|

Uses DaisySP `Chorus`. Adds stereo width and gentle movement.

### 6.2 Reverb

|Parameter|Value|Notes                         |
|---------|-----|------------------------------|
|Size     |70%  |Hardcoded — medium-large space|
|LP Freq  |4 kHz|Hardcoded — dark, warm tail   |

Uses DaisySP `ReverbSc`.

### 6.3 Serial Crossfade (CC 8)

The FX knob doesn't control a simple wet/dry — it crossfades through three zones:

```
CC 8 value:  0% ──────── 25% ──────── 50% ──────── 75% ──────── 100%
Output:      DRY         DRY→CHORUS   CHORUS       CHORUS→REVERB REVERB
```

|CC Range|Behavior                                                                    |
|--------|----------------------------------------------------------------------------|
|0%      |Fully dry — no effects                                                      |
|0–50%   |Chorus fades in. At 50% it's fully wet chorus, no reverb.                   |
|50–100% |Chorus fades out, reverb fades in. At 100% it's fully wet reverb, no chorus.|

This gives a single knob three distinct zones: clean, modulated, and spacious. The crossfade is linear within each half.

-----

## 7. MIDI

### 7.1 Transport

**Hardware UART MIDI** via 5-pin DIN input circuit on Daisy pin **D14** (USART1 RX). Uses libDaisy's `MidiUartHandler` class. Standard MIDI baud rate (31250). See §13 for hardware wiring.

### 7.2 Notes

|Feature   |Behavior                    |
|----------|----------------------------|
|Channel   |1 only                      |
|Priority  |Latest note (mono)          |
|Velocity  |Ignored (constant amplitude)|
|Pitch Bend|±2 semitones                |
|Note Off  |Triggers envelope release   |

Latest-note priority means new notes always take over immediately. When the new note is released, the synth does **not** return to a previously held note — it simply enters the release phase. This is the simplest mono behavior to implement.

### 7.3 CC Map

|CC#|Parameter             |Default           |Range                          |
|---|----------------------|------------------|-------------------------------|
|1  |Filter Cutoff         |64 (mid)          |0–127 → 20 Hz – 18 kHz (expo)  |
|2  |Drive + Resonance     |32 (moderate grit)|0–127 → see §4.2 curves        |
|3  |Sub Level             |76 (~60%)         |0–127 → 0–100%                 |
|4  |Wavefolder Amount     |25 (~20%)         |0–127 → 0–100%                 |
|5  |Envelope Decay/Release|40 (~400ms)       |0–127 → 30 ms – 5 s (expo)     |
|6  |Envelope → Amp        |100 (~80%)        |0–127 → 0–100%                 |
|7  |Envelope → Filter     |76 (~60%)         |0–127 → 0–100%                 |
|8  |FX Crossfade          |0 (dry)           |0–127 → serial crossfade (§6.3)|

Defaults are chosen so that the synth sounds good on first power-up with no CC input: a saw with moderate sub, light wavefold, punchy decaying envelope sweeping both amp and filter, gritty MS-20 character, no effects.

-----

## 8. Hardcoded Defaults Summary

Everything not controlled by the 8 CCs:

|Parameter        |Hardcoded Value|Reasoning                                                    |
|-----------------|---------------|-------------------------------------------------------------|
|Waveform         |Saw            |Full harmonic series, thickest MS-20 interaction             |
|Filter Mode      |LP             |The classic subtractive starting point                       |
|Key Tracking     |50%            |Cutoff follows pitch partially — keeps bass dark, treble open|
|Envelope Attack  |5 ms           |Always snappy — plucky character                             |
|Envelope Sustain |0%             |Pure AD shape — percussive and simple                        |
|Wavefold Symmetry|0% (symmetric) |Odd harmonics, pairs well with saw                           |
|Chorus Rate      |0.8 Hz         |Gentle movement                                              |
|Chorus Depth     |40%            |Noticeable but not seasick                                   |
|Chorus Feedback  |20%            |Subtle                                                       |
|Reverb Size      |70%            |Medium-large dark space                                      |
|Reverb LP        |4 kHz          |Warm tail                                                    |

-----

## 9. Resource Budget

|Resource    |Available        |Estimated Usage                          |
|------------|-----------------|-----------------------------------------|
|CPU         |480 MHz          |~10–15%                                  |
|Internal RAM|512 KB           |~15 KB                                   |
|SDRAM       |64 MB            |~200 KB (reverb)                         |
|Audio       |48 kHz / 24-bit  |Block size 48                            |
|MIDI        |UART (31250 baud)|Pin D14 (USART1 RX) via 6N138 optocoupler|

With one voice and two effects, the Daisy Seed is barely working. This is ideal for a prototype — zero risk of CPU issues, and you can focus entirely on getting the MS-20 filter to sound right.

-----

## 10. Software Architecture

```
src/
├── main.cpp               ← Daisy init, audio callback, MIDI CC handling
├── voice.h/.cpp           ← Saw osc + sub + wavefolder + MS-20 filter + envelope
├── ms20_filter.h/.cpp     ← Oversampled Korg 35 emulation
├── fx_chain.h/.cpp        ← Chorus → Reverb with serial crossfade
└── params.h               ← 8 CC values, hardcoded defaults, scaling curves
```

Five files. The `voice` module contains the complete signal path from oscillator to amp output. The `ms20_filter` module is self-contained and portable — when you scale to v1, you drop it into the larger architecture unchanged. The `fx_chain` is a thin wrapper around two DaisySP modules with the crossfade logic. The `params` file holds the 8 CC values, the drive/resonance linking curves, and all hardcoded constants in one place.

-----

## 11. Hardware

### 11.1 Parts List

|Part                    |Qty     |Notes                                             |
|------------------------|--------|--------------------------------------------------|
|Electrosmith Daisy Seed |1       |Pre-soldered headers recommended (~$30)           |
|Breadboard (830-point)  |1       |Standard full-size                                |
|USB-C cable             |1       |For flashing firmware and power during development|
|5-pin DIN socket        |1       |Panel-mount or PCB-pin style. For MIDI input.     |
|6N138 optocoupler       |1       |Standard MIDI-spec optoisolator (H11L1 also works)|
|1N4148 diode            |1       |Reverse polarity protection on MIDI input         |
|220Ω resistor           |1       |MIDI input current limiting                       |
|470Ω resistor           |1       |6N138 output bias (pins 5–6)                      |
|10kΩ resistor           |1       |6N138 collector pull-up to 3.3V                   |
|1/4" mono jack socket   |1       |Audio output to Syntakt                           |
|5-pin DIN MIDI cable    |1       |Syntakt MIDI Out → Daisy MIDI In                  |
|1/4" TS instrument cable|1       |Daisy audio out → Syntakt audio in                |
|Hookup wire             |Assorted|For breadboard connections                        |

**Estimated total cost:** $35–45 (assuming no existing parts).

### 11.2 MIDI Input Circuit

Standard optoisolated MIDI input per MIDI spec. The optocoupler electrically isolates the Syntakt from the Daisy, preventing ground loops and protecting both devices.

```
5-PIN DIN SOCKET                    BREADBOARD                      DAISY SEED
(rear view, solder side)

    ┌───────────┐
    │  4     5  │
    │     2     │               6N138 (top view)
    │  1     3  │          ┌────────────────────┐
    └───────────┘          │ 1 Anode    Vcc  8  │── 3.3V
                           │ 2 Cathode  Out  7  │──┬── 10kΩ ── 3.3V
Pin 5 ─────────────────────│→ Cathode          │  │
                           │                    │  └──────────── D14 (USART1 RX)
Pin 4 ── 220Ω ──┬─────────│→ Anode            │
                 │         │ 3 NC     GND  5  │──┬── GND
              1N4148       │ 4 NC     Vb   6  │──┘
              (band        └────────────────────┘
              toward        470Ω between pins 5 and 6
              pin 5)
                 │
Pin 5 ◄──────────┘ (diode cathode, band this end)

Pin 2 ── GND (optional shield ground)
```

**Wiring step by step:**

1. **DIN Pin 4** → 220Ω resistor → 6N138 pin 1 (Anode)
1. **DIN Pin 5** → 6N138 pin 2 (Cathode)
1. **1N4148 diode** across 6N138 pins 1 and 2, with cathode (band) on pin 2 — this protects against reversed MIDI cables
1. **6N138 pin 8** (Vcc) → Daisy **3V3** pin
1. **6N138 pin 5** (GND) → Daisy **GND**
1. **470Ω resistor** between 6N138 pin 5 (GND) and pin 6 (Vb) — biases the output transistor
1. **10kΩ resistor** from 6N138 pin 7 (Out) → Daisy **3V3** — pull-up resistor
1. **6N138 pin 7** (Out) → Daisy **pin D14** — this is the UART RX data line

### 11.3 Audio Output

Mono output from the Daisy's onboard WM8731 codec. The output is AC-coupled and line-level (~1V peak-to-peak), which matches the Syntakt's external audio input.

```
DAISY SEED                          1/4" MONO JACK
                                    (rear view)

Pin 18 (Audio Out L) ──────────────── Tip
GND ────────────────────────────────── Sleeve
```

That's it — two wires. Software sums to mono internally (same signal written to both codec channels). No amplifier, no coupling capacitor, no level matching needed.

### 11.4 Power

During development: USB-C cable to your computer provides 5V power and firmware flashing.

Standalone use (no computer): any USB 5V power source (phone charger, USB battery pack). The Daisy draws roughly 200–300 mA.

### 11.5 Daisy Seed Pin Usage Summary

```
DAISY SEED (pin side, top view)
         ┌──────────┐
    D0  ─┤1      40├─ D15
    D1  ─┤2      39├─ D14 ◄── MIDI RX (USART1)
    D2  ─┤3      38├─ D13
    D3  ─┤4      37├─ D12
    D4  ─┤5      36├─ D11
    D5  ─┤6      35├─ D10
    D6  ─┤7      34├─ D9
    D7  ─┤8      33├─ D8
   GND ──┤9      32├─ D31
   3V3 ──┤10     31├─ D30
  3V3A ──┤11     30├─ D29
  VREF ──┤12     29├─ D28
   D16 ──┤13     28├─ D27
   D17 ──┤14     27├─ D26
 AUD L ──┤15     26├─ D25      Only 3 pins used:
 AUD R ──┤16     25├─ D24      • Pin 15 (Audio Out L) → 1/4" jack tip
  AGND ──┤17     24├─ D23      • Pin 39 (D14) ← MIDI input
 AUD L ──┤18     23├─ D22      • GND → jack sleeve + MIDI circuit
   OUT    │  ◄────── Audio     • 3V3 → MIDI circuit pull-ups
 AUD R ──┤19     22├─ D21
   OUT    │       21├─ D20
  DGND ──┤20     20├─ D19
         └──────────┘
```

-----

## 12. Signal Path Summary

```
MIDI IN (5-pin DIN → 6N138 → UART RX D14)
    │
    ▼
MIDI Note On
    │
    ▼
SAW OSC (PolyBLEP, concert pitch)
    │
    + SUB OSC (sine, -1 oct, level = CC 3)
    │
    ▼
WAVEFOLDER (amount = CC 4, symmetry = 0%)
    │
    ▼
MS-20 FILTER (LP, cutoff = CC 1, drive+reso = CC 2, env = CC 7)
    │
    ▼
AMP ENVELOPE (attack 5ms, decay = CC 5, sustain 0%, depth = CC 6)
    │
    ▼
FX CROSSFADE (CC 8: dry → chorus → reverb)
    │
    ▼
MONO OUTPUT (Audio Out L, pin 18 → 1/4" jack → Syntakt)
```

-----

## 13. Complexity

**2 out of 10** for the system. **7 out of 10** for the MS-20 filter alone. That's exactly the right ratio for a prototype — trivial scaffolding around the one component that needs the most attention and iteration.
