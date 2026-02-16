# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
make              # build firmware → build/daisy-ms20.bin
make clean        # remove build artifacts
make program-dfu  # flash via USB (hold BOOT, press RESET, release BOOT first)
```

Requires `arm-none-eabi-gcc` cross-compiler and `DaisyExamples` (with libDaisy + DaisySP built) as a sibling directory at `../DaisyExamples`. No tests — this is bare-metal embedded firmware.

## Architecture

Monophonic synth for the Electrosmith Daisy Seed (STM32H750, ARM Cortex-M7). Audio runs at 48 kHz with block size 48. All control is via 8 MIDI CCs on channel 1 over hardware UART (pin D14).

**Signal chain:** Saw OSC (+Sub) → Wavefolder → MS-20 Filter → Amp Envelope → Overdrive → Mono Out

### Source Files

- **`main.cpp`** — Hardware init, audio callback (calls `voice.Process()` then `fx.Process()`), MIDI polling loop with `HandleMidiMessage` dispatcher.
- **`voice.h/.cpp`** — Complete voice: PolyBLEP saw oscillator, sine sub (-1 oct), symmetric wavefolder, Korg35LPF filter, one-pole AD envelope. `Process()` takes a `const Params&` and returns one sample.
- **`ms20_filter.h`** — Header-only, zero-dependency Korg 35 LPF using TPT/ZDF method. Two cascaded one-pole filters with `tanh` saturated feedback. Portable — no Daisy includes. Based on Pirkle AN-5 and Zavalishin's "Art of VA Filter Design."
- **`fx_chain.h/.cpp`** — Wraps DaisySP `Overdrive` (post-filter saturation). CC 8 controls drive amount (0=clean, 127=full saturation).
- **`params.h`** — All 8 CC mappings, scaling curves (exponential cutoff/decay, linked drive/resonance from CC 2), hardcoded constants (envelope shape, key tracking), and the `Params` struct with `HandleCC()`/`Update()`.

### Key Design Patterns

- **No dynamic allocation.** Everything is statically allocated.
- **Params struct as interface.** Voice and FxChain don't handle MIDI directly — `main.cpp` updates `Params` via `HandleCC()`, and DSP modules read derived values.
- **Drive + Resonance linked on CC 2.** Drive uses `x^0.6` (front-loaded), resonance uses `x^1.8 * 0.85` (back-loaded, capped). Both derived from the same raw CC value.
- **Filter envelope is additive Hz**, not multiplicative. `mod_cutoff += env * filt_env_depth` where depth maps 0–18 kHz.
- **ms20_filter.h is intentionally standalone.** It has no Daisy dependencies and can be unit-tested offline or reused in other projects.

## ASCII Art / Diagrams

- Use plain ASCII only (`-`, `|`, `+`, `<`, `>`, `v`, `^`) — no Unicode box-drawing characters
- Keep diagrams short (3-4 lines max). Break complex diagrams into smaller pieces.
- Prefer horizontal single-line signal-chain style when possible (no vertical alignment needed)
- Pad all lines within a diagram to the same length to catch column drift
- Write the full file rather than using Edit for diagram sections
- Always Read the file back after writing and verify all lines are equal length
- For complex diagrams, ask the user to compose in ASCIIFlow (asciiflow.com) and paste the result
