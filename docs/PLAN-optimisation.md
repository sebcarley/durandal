# Plan: a faster Flagship and Rampant

2 October 2026, branch `durandal/optimisation` (from `baseline-8`). Measured
hidden and off-screen while the Mac was busy with other work, as the owner
asked; numbers are for comparison between runs, not the official on-screen
baseline.

## In short

1. **One compile option is worth more than everything else put together.**
   - The world shaders are compiled in Metal's *safe* (strict IEEE) maths mode. That was chosen in Round 2 to match OpenGL exactly for the parity checks.
   - In *relaxed* mode the GPU's time per frame roughly halves:

     | | Safe | Relaxed |
     |---|---|---|
     | Rampant, level 6 | 175 fps | **311 fps** |
     | Flagship, level 6 | 211 fps | **358 fps** |
     | Rampant, L28 | 125 fps | **216 fps** |
     | Flagship, L28 | 155 fps | **270 fps** |

   - Frame shots on four films (level 6, 5-D Space, L21 water, L28 all liquids) show no visible change: at most 0.6 levels in 255 on average, and no artefacts.
   - 5-D Space, where the polygon walk folds through itself, changes least of all (0.01).
2. **Three exact fixes**, built and checked pixel-identical on two films:
   - glow passes no longer walk lights whose result is clamped away;
   - transparent texels are discarded before they are lit;
   - the weapon in hand no longer walks lights it then overwrites.

   On L28 (safe maths) the world pass drops from 9.75 to 9.07 ms, and its worst frames from 22.3 to 20.7 ms.
3. **After those two, the average is far past the targets.** What limits now is the **1% low**: Rampant 71–105, Flagship 110–135.
   - The slow frames are firefights. Every one of them waits on the GPU, and each has twice the usual number of cast lights (12–14 against 6.7).
   - The rest of the plan is about those frames.
4. **Two ideas were measured and dropped:**
   - Compiling Rampant's paths out of the lower tiers (a per-tier shader build) made Flagship no faster.
   - A glow image at half the bytes (RG11B10) saved nothing.
5. **Found on the way:** Bounced Light makes the picture differ slightly from one run to the next (about 0.5 in 255 on L28; exactly repeatable with it off). That points to a timing dependence or a read/write race in the light atlas. It harms nothing in play, but it blurs every exact comparison.

## How it was measured

- **The runs:** `scripts/feature-costs.sh` plays a stretch of a film per variant, in a hidden window, off-screen at 1920x1080, uncapped, with GPU stage timing on. `scripts/feature-costs.py` tabulates the runs.
- **Variants:** each is a tier, or a tier with one switch off (through `DURANDAL_SET`), or an environment override such as `DURANDAL_MATH=relaxed`, a size, or another build kept for A/B runs.
- **Order:** each list ran two to four times, in alternating order, so steady drift evens out.
- **Stretches:**
  - level 6 to tick 1200;
  - L28 to tick 2100, which holds the heaviest second of the earlier benchmark;
  - level 6's opening firefight (ticks 0–300), four times per variant.
- **Noise:** the Mac was busy, and switches that cannot cost anything moved the GPU frame total by up to ±0.7 ms. Smaller effects can't be separated, and are marked as such below.
- **Reading the GPU times:** stage times run from each pass's start to its end with frames in flight, so they overlap. Compare them with each other, not with the frame interval.
- **Pictures:** compared with frame shots at a fixed 30 fps, scored per pixel.

## Where the time goes (relaxed maths)

### Tiers, frames a second (average / 1% low)

| | Level 6 to tick 1200 | L28 to tick 2100 | Level 6 firefight |
|---|---|---|---|
| Classic | 644 / 342 | | |
| Enhanced | 401 / 178 | | |
| Flagship | 358 / 135 | 270 / 110 | 259 / 118 |
| Rampant | 311 / 105 | 216 / 71 | 194 / 85 |

Rampant at other sizes on level 6: 527 / 170 at 1280x720, and 159 / 68 at 2560x1440. That is strongly bound by the cost per pixel.

### What a switch costs when on

GPU ms per frame, measured by switching it off. Values within the noise are left out.

| Switch | Average | In the worst frames | Where |
|---|---|---|---|
| Dynamic lights (Flagship, firefight) | 1.07 | 2.2 (world p99) | the per-pixel light loop, not the shadow walks (those cost 0.17); 0.22 of it is the lit fog |
| Volumetric fog | 0.7–0.9 | Flagship's 1% low 135 → 179 without it | kernel 0.66 ms, plus applying it per pixel; it walks lights per slice |
| Ambient shadows (screen-space) | 0.55–0.66 | | |
| Traced ambient shadows (Rampant, over screen-space) | 0.5–0.75 | 0.9 (AO p99) | the largest Rampant cost |
| Traced shadows (Rampant) | 0.3–0.4 | 1.75–2.0 (world p99) | figure tests in the walks; figure lights 4 → 0 saves 1.2 at p99 |
| Bounced light | 0.3 | | bake 0.2 + averages 0.3; also copies the whole atlas each frame |
| Smooth Edges (4x MSAA) in Rampant | 0.26 | | liquids run per sample |
| Bloom | 0.8–1.5 (stage) | | 9 small passes; high for what it does |
| Light redistribution | about 0.3 | Flagship's 1% low 135 → 147 | |
| Reflections, dust and embers, heat shimmer | within the noise | | |
| No HD art (the 8-bit ramp shading instead) | +2.8 to +3 more | Flagship 358 → 223 fps | |

### CPU

- **Rampant:** 1.4–2.1 ms per frame of render preparation, rising to 2.7–3.2 ms in the slow frames. Flagship takes 0.5–1.7.
- **Where it goes:**
  - whole-level tables rebuilt every frame (surface table, map, light grid key);
  - figure masks built on the main thread the first time a sprite frame is seen.
- **Effect today:** the CPU is not the limit, since the GPU still is in every slow frame. But it is heat on a fanless Air, and it shares the chip's power budget with the GPU.

## The plan

### Step 1: now (done 2 Oct 2026)

- **Relaxed maths for the world shaders by default.**
  - `DURANDAL_MATH=safe` stays as a development switch, to put strict IEEE back for parity work; `fast` behaves the same as `relaxed` here.
  - Effort: one line.
  - The owner judged it on the display: "pretty solid 200 fps+ at Rampant".
- **The three exact fixes** (done on this branch, pixel-identical, film tests run).

### Step 2: the firefight frames (the 1% low, all tiers)

| | What | Expected | Effort |
|---|---|---|---|
| 2a | **Cull lights per surface.** For each draw the CPU works out which of the 16 lights can reach its polygon (range against bounds, the facing side), and passes a mask. The shader loops only over those. Contact-shadow casters likewise. | Most of the 1.1 ms (2.2 at p99) that lights cost in a firefight, in every tier from Enhanced up | small |
| 2b | **The fog's light walks.** Walk each light once per column, not per slice. Use a tiling noise texture instead of 24 hashes a slice, and skip the detail noise where it has faded. | 0.3–0.6 ms; better lows (fog off lifts Flagship's 1% low by a third) | small to medium |
| 2c | **Traced ambient shadows over time.** The level's own geometry does not move. Either accumulate the rays over frames (2 a frame, reprojected with last frame's view, which is already kept), or bake the walls' occlusion into the surface cache and trace only the figures each frame. | 0.5–0.7 ms; 0.9 at p99 | medium to high |
| 2d | **Figure shadows in firefights.** Test figures only for the two nearest strong lights while more than eight lights are up (measured: 2 lights save 0.7 ms at p99, 0 save 1.2), and keep the figure test from repeating per light where the light disc is tiny. | up to 1 ms at p99 | small |

### Step 3: the steady costs

| | What | Expected | Effort |
|---|---|---|---|
| 3a | **Bloom:** fewer, merged passes (or one compute pass with shared memory), its texture views made once, the sRGB decode done once. | about 0.5 ms from Enhanced up | medium |
| 3b | **Bounced light:** write the bake's results to a small buffer instead of copying the whole atlas every frame; compute each group's average once, not once per member. Find and fix the run-to-run variance at the same time. | about 0.3 ms, and repeatable pictures | medium |
| 3c | **CPU:** cache the parts of the per-frame tables that do not change (only lights, platform heights, liquids and animated textures do). Build a level's figure masks at level entry or on a worker thread. | about 1 ms of CPU, and the CPU part of the spikes | medium |
| 3d | **The 8-bit ramp path** (players without HD art): fewer dependent lookups per tap. | some of its 3 ms | medium |
| 3e | **Liquids under 4x MSAA:** trace reflection and refraction once per pixel, not per sample. | small now (0.26 ms) | high |

### Not recommended

- **MetalFX upscaling.** It would pay (the cost scales with pixels) but softens the crisp art, and after step 1 it is not needed.
- **Per-tier shader builds** (measured: no gain).
- **A smaller glow format** (measured: no gain).

### Measuring each step

- **The tools:** the same scripts and stretches, with the base build kept beside the new one for interleaved A/B runs.
- **Pictures:** compared with frame shots at a fixed 30 fps. Exact changes must score 0; others need a look.
- **The gate:** the film tests three ways.
- **The final word:** on the display, full screen, when the screen is free.

## Decisions for the owner

1. Relaxed maths as the default (step 1), after a look on the display.
2. The order of step 2; 2a and 2d are the quickest wins for the worst frames.
3. Whether to chase Bounced Light's run-to-run variance (3b). It matters for exact comparisons, not for play.

## Data

- Runs: `.deps/fc/` (git-ignored):
  - `L06` (safe maths, every switch);
  - `L06-relaxed` and `L28-relaxed` (relaxed, the A/B builds, every switch);
  - `L06-fight` (the firefight, four repeats).
- Tables: `python3 scripts/feature-costs.py .deps/fc/<set> --base <variant>`.
