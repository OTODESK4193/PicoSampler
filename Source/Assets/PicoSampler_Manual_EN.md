# PICO SAMPLER — Quick Manual (English)

**English** | [日本語](PicoSampler_Manual_JP.md)

---

## Contents

1. [Getting Started](#1-getting-started)
2. [Layout](#2-layout)
3. [MAIN Tab](#3-main-tab)
4. [ARP / FILTER Tab](#4-arp--filter-tab)
5. [MOD Tab](#5-mod-tab)
6. [FX Tab](#6-fx-tab)
7. [CONFIG Tab](#7-config-tab)
8. [Presets](#8-presets)
9. [Starting Points](#9-starting-points)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Getting Started

### Installation

1. Copy `PicoSampler.vst3` to:
   ```
   C:\Program Files\Common Files\VST3\
   ```
2. Rescan plugins in your DAW.

### First Steps

1. **Drag and drop** a WAV / AIFF / MP3 / FLAC / OGG file onto the waveform area.
2. Analysis runs, the waveform appears, and the root key is detected automatically.
3. Play your keyboard.

That's the whole workflow. Everything below is about refining the sound.

---

## 2. Layout

From top to bottom:

| Area | Contents |
|---|---|
| **Header** | Logo, loaded file name, Root Key, Key Range, AUTOSLICE, REANALYZE, PRESETS |
| **Waveform display** | Waveform and markers. Drag to edit, scroll to zoom |
| **Key range map** | Key range of all 8 slots. Drag the coloured bars to change them |
| **Tabs** | MAIN / ARP・FILTER / MOD / FX / CONFIG |

### Waveform Display

| Action | Result |
|---|---|
| Drag and drop | Load a sample |
| Drag a marker | Move Start / End / Loop points |
| Scroll wheel | Zoom (up to 100,000×) |
| Drag the bottom bar | Scroll while zoomed |
| Right-click | Zoomed in → reset zoom / At 1× → confirm sample deletion |

### Knob Controls

| Action | Result |
|---|---|
| Drag | Change value |
| **Ctrl + drag** | **Coarse** — cover the full range quickly |
| **Shift + drag** | **Fine** — sample-level precision |
| Double-click | Reset to default |
| Right-click | Type a value directly |

> 💡 Press Ctrl / Shift **before** grabbing the knob. The resolution is fixed at the moment you click.

---

## 3. MAIN Tab

### ACTIVE SLOT (S1–S8)

Selects which slot you're editing. The waveform display and every knob below reflect the selected slot.

### PLAYBACK MODE

| Mode | Behaviour |
|---|---|
| **SINGLE** | Only the selected slot sounds |
| **LAYER** | Every slot whose key range matches sounds at once — stack them for thickness |
| **RANDOM** | One matching slot is picked at random per note — adds human variation |

### ROUTING

**FLT BYPASS** and **FX BYPASS** are set **per slot**. A dry drum slot and a fully processed pad slot can coexist in one instance.

### SAMPLE / LOOP

| Knob | Description |
|---|---|
| **Start / End** | Playback range. Fully continuous — usable at any zoom level |
| **L-Start / L-End** | Loop range |
| **X-Fade** | Loop crossfade length, as a fraction of the loop |
| **Pan** | Slot position: `L100` – `C` – `R100` |
| **Volume** | Slot level (-36 to +12 dB) |

| Button | Description |
|---|---|
| **LOOP** | Loop playback |
| **STRETCH** | Time-stretched playback — pitch changes without speed changes |
| **REVERSE** | Reverse playback. The waveform display flips to match |
| **SNAP** | **Forces markers onto zero crossings** |

> 💡 **About SNAP**
> With SNAP off, markers move completely freely. With it on they move in steps, but always land where the waveform crosses zero — which is what prevents clicks. Near transients the marker is also magnetically attracted to the attack.

### ENVELOPE

Attack / Decay / Sustain / Release shape the slot's level over time.

**LINK** immediately copies the current slot's ADSR to **all 8 slots**, and keeps them synchronised as you turn the knobs. Handy for matching release times across a drum kit.

> ⚠️ The LINK on/off state is not stored in presets (the sound is fully reproduced). Re-enable it after loading if you want the linked behaviour back.

### PITCH

| Knob | Description |
|---|---|
| **Root** | The sample's original pitch. `Auto` uses the detected value |
| **Octave** | ±3 octaves |
| **Semi** | ±24 semitones |
| **Fine** | ±100 cents |

### MASTER

The shared output stage: Pitch (master transposition), HPF, LPF, Gain, and Ceiling (limiter threshold).

---

## 4. ARP / FILTER Tab

### ARPEGGIATOR SETTINGS

Enable with **ARP ON**. Held chords are played back as an arpeggio.

| Control | Description |
|---|---|
| **LATCH** | Keeps playing after you release the keys |
| **SYNC** | On = tempo-synced (1/1–1/32), Off = free-running Rate in Hz |
| **Pattern** | 13 types: Up, Down, Up-Down, Down-Up, Up-Down (Incl), Converge, Diverge, Pedal Low, Pedal High, As Played, Random, Random Walk, Chord |
| **Octaves** | How many octaves the pattern climbs (1–4) |
| **Gate** | Note length (0.1–1.0) |
| **Offset** | Transposes the whole pattern (±12 semitones) |
| **Swing** | Shuffle amount (0–75 %) |
| **Repeat** | Retriggers per step (1–4) |
| **Accent** | Bipolar velocity accent |

### SCALE QUANTIZER

Forces output notes into a chosen scale. **17 scales**: Free, Chromatic, Major, Natural Minor, Major/Minor Pentatonic, Dorian, Lydian, Mixolydian, Phrygian, Harmonic Minor, Melodic Minor, Whole Tone, Octaves & Fifths, Quartal, Sus2/4 Cloud, Diminished.

Root Key can be `Auto` (uses the detected value) or fixed manually.

### MAIN FILTER

Three models:

| Model | Description |
|---|---|
| **Clean SVF** | Standard filter. LowPass / BandPass / HighPass / Notch at 12 or 24 dB/oct |
| **Vowel Formant** | A-I-U-E-O vowel morphing, swept with the Formant knob |
| **Comb Filter** | Metallic resonance, blended with Comb Mix |

The **response curve moves in real time with modulation**. Assign an LFO to Cutoff and the graph sweeps exactly as the sound does — the display uses the same maths as the DSP.

### FILTER ENVELOPE

A dedicated ADSR for the filter. **Amt** is bipolar and spans ±4 octaves of cutoff travel; negative values produce a closing envelope.

---

## 5. MOD Tab

### SOURCES

**LFO tab** — LFO 1–4. Waveforms: Sine, Triangle, Saw, Square, S&H, Chaos. With SYNC off they run 0.05–30 Hz; with SYNC on they follow host tempo.

**ENV tab** — ENV 1–3. Standard ADSR plus a **Loop** switch that turns the envelope into a repeating (LFO-like) shape.

### MATRIX

16 slots across two pages. Each row is:

```
[Source] → [Destination] [UNI] [Amount slider]
```

| Field | Description |
|---|---|
| **Source** | 12 options: LFO 1-4, ENV 1-3, Velocity, Note, Mod Wheel, Random |
| **Destination** | 81 options: per-slot Start/End/Loop/X-Fade, S1–S8 Pan, Master Pitch, every arpeggiator and filter control, all FX amounts and detail parameters |
| **UNI** | On = unipolar (0 → +), Off = bipolar (− → +) |
| **Amount** | Modulation depth. Double-click to return to 0 |

> 💡 **Visualised modulation**
> Modulated knobs draw a coloured band showing the reachable range, plus a bright dot for the current value. This runs through the same function the DSP uses, so the display never disagrees with what you hear.

> 💡 **Pan destinations** sit at the end of the list (`S1/Pan`–`S8/Pan`) to keep existing projects' assignments stable.

---

## 6. FX Tab

Five slots in **series**.

- **Drag a card header** to reorder the chain.
- **Click a card** to show its detail parameters in the DETAILS area below.
- Each slot's **AMOUNT** sets how much of that effect is applied.

### Effect Types

| FX | Parameters | Use |
|---|---|---|
| **Saturation (ADAA)** | ALGO (10 types), DRIVE, PRE-HPF, TRIM | Distortion with anti-aliasing |
| **Ensemble Chorus** | RATE, DEPTH, WIDTH | Width and thickness |
| **Tape Delay** | TIME (synced), FEEDBACK, DUCK, DAMP | Delay. DUCK pulls back while input is present |
| **Freeze** | SIZE, FEEDBACK, DAMP | Sustains and stretches the sound indefinitely |
| **Shimmer Reverb** | DECAY, SHIMMER, DAMP, MOD | Reverb with octave-up sparkle |

**Saturation algorithms:** Soft Tanh, Hard Clip, Triode, Tape, Transformer, JFET, BJT, Wavefold, Exciter, Cubic

---

## 7. CONFIG Tab

| Area | Control | Description |
|---|---|---|
| **ANALYSIS** | Material Mode | Analysis character (Auto / Crisp / Smooth / Formant) |
| | Stretch Mode | Stretch character (Beat / Tone / Texture / Complex) |
| **SLICING** | Slice Sens | Auto-slice density. Higher = more slices |
| | Filter Slope | Default filter slope |
| **SAMPLE EDGE** | Fade In / Fade Out | Marker de-click (0–200 ms) |
| **PERFORMANCE** | Polyphony | Voice count (1–32) |
| | Portamento / Glide Time | Pitch glide |
| **OUTPUT** | Lim Release | Limiter recovery time |
| **APPEARANCE** | Color Theme | Midnight / Sakura / Ocean / Forest / Sunset / Mono |

> 💡 **About Fade In / Out**
> When a Start or End marker sits mid-waveform (where amplitude isn't zero), playback clicks at that edge. This short fade removes it. The defaults of 2 ms / 3 ms are inaudible as a level change but eliminate the click. Set to `0` for a completely hard cut.
> In loop mode the End fade is skipped, because the loop crossfade already handles that boundary.

---

## 8. Presets

Open with the **PRESETS** button in the header.

### Saving

**SAVE** opens a dialog for a sub-category and a preset name. The sub-category can be picked from existing ones or typed fresh.

**What is stored:**
Every knob, every combo box, every toggle, the full 16-slot modulation matrix, all LFO and ENV settings, the FX chain order and all its parameters, the sample file paths for all 8 slots, key ranges, and root key settings.

> ⚠️ **The audio data itself is not stored** — only the file paths. If you move or delete a source sample, that slot loads empty and the missing file names are listed for you.

### Loading

**Double-click** a preset name, or select it and press **Enter**. Use the category list on the left to filter, and the search box at the top to find by name.

### Deleting

**Right-click** a preset name → `Delete...` → confirm with **Yes**.

Only the preset file is removed; the samples it referenced are untouched. The file goes to the Recycle Bin, so an accidental delete is recoverable.

### INIT

**Resets everything.** Clears the samples from all 8 slots and returns every parameter to its default. This is destructive, so a confirmation dialog appears. It cannot be undone.

### Storage Location

```
C:\Users\<username>\AppData\Roaming\PicoSampler\Presets\<Category>\<Name>.picopreset
```

Type `%APPDATA%\PicoSampler` into Explorer's address bar to open it directly.

---

## 9. Starting Points

### Tight One-Shot Drum

1. Load the sample
2. **SNAP** on
3. Drag Start to just before the attack
4. CONFIG → Fade In `0 ms`, Fade Out `3 ms`

The attack stays intact while the release-side click disappears.

### Seamless Sustained Loop

1. **LOOP** on
2. Place L-Start and L-End with **SNAP** still on
3. Raise **X-Fade** to around `0.1`

Both points sit on zero crossings, so there's no step, and the crossfade covers the join.

### Chopping a Breakbeat

1. Enable **AUTOSLICE** in the header
2. Drop the loop onto the waveform area

Transients are detected and up to 8 slices are spread across the slots, mapped chromatically from C1. Each slice ends 1 ms before the next attack so nothing bleeds through.

### Pad From a Short Sample

1. **STRETCH** on
2. CONFIG → Stretch Mode = `Tone` or `Complex`
3. **LOOP** on with a long **X-Fade**

The 49 anchor buffers keep formants stable across four octaves of transposition.

### Auto-Pan

In the MOD tab, set Source to `LFO 1` and Destination to `S1/Pan`, then raise Amount.

### Evolving Filter

1. MOD → ENV tab, enable **Loop** on `ENV 1`
2. In MATRIX, route `ENV 1` → `Filter Cutoff`
3. Watch the curve on the ARP / FILTER tab

The graph animates in real time with the modulation.

---

## 10. Troubleshooting

### No sound

- Is **FILTER ON** with Cutoff turned right down?
- Does the slot's **Key Range** cover the notes you're playing? Check the key range map
- Is **PLAYBACK MODE** set to SINGLE with a different slot selected?
- Is **Volume** turned all the way down to `-inf`?

### Clicks at the marker positions

- Turn **SNAP** on to land on zero crossings
- Raise **Fade In / Fade Out** in CONFIG slightly (3–5 ms)

### The marker won't stop where I want

- With **SNAP** on it can only land on zero crossings. Turn it off for completely free placement

### Knobs feel too fine / too coarse

- **Ctrl + drag** for coarse, **Shift + drag** for fine
- Press the modifier **before** grabbing the knob

### A preset says samples are missing

- The source files have been moved or deleted. Presets store paths, so either restore the files to their original location, or reload them and save the preset again

### REANALYZE takes a moment

- It re-renders all 49 stretch anchor buffers. This runs in the background, so you can keep working while it finishes

### Audio dropouts / high CPU

- Lower **Polyphony** in CONFIG (e.g. 16 → 8)
- Set unused FX slots to `None`

---

## License

This software is distributed under the **AGPLv3**.

Time-stretching is powered by **Signalsmith Stretch**, used under the MIT License.

- Signalsmith Stretch — Copyright © 2022 Geraint Luff / Signalsmith Audio Ltd.
- Signalsmith Linear — Copyright © 2025 Signalsmith Audio

---

**Developer:** @kijyoumusic (OTODESK)
[X (Twitter)](https://x.com/kijyoumusic) | [Official Website](https://otodesk4193.github.io/OTODESK_SITE/)
