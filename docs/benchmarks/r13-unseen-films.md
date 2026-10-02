# Benchmark: Rampant against Flagship on six films not used before

- Date: 2026-10-02 08:12-08:36
- Commit: a40d24c0 (durandal/round-13-rampant)
- Machine: Mac17,3, Apple M5, 16 GB, on mains power
- Display: MSI MAG 272U X24, full screen, 1920 x 1080 at 240 Hz (the
  built-in screen also attached)
- Settings: uncapped, vsync off, 3 drawables, sound off, `DURANDAL_QA=1`,
  `DURANDAL_GPU_TIMING=1`, `DURANDAL_SET=quality_tier=3` (Flagship) or
  `quality_tier=5` (Rampant), the HD art packs from the owner's settings
- Each film's first 3600 ticks (or the whole film where shorter), once
  per tier, order alternating film by film (Flagship first on odd rows);
  one untimed Rampant warm-up on L08 first. Thermal state "fair" from the
  second run on.
- Another Claude session ran CPU-heavy jobs on and off that morning; the
  alternating order keeps the comparison fair, and Rampant's slow frames
  are GPU-bound (the CPU waits on the drawable), but absolute numbers may
  be a little low.

| Film | Liquid | Tier | Avg fps | 1% low | Worst second | GPU p99 ms: frame | world | ambient | fog |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| L08 Nuke and Pave (116 s) | lava | Flagship | 189.0 | 132.8 | 136 | 13.3 | 9.8 | 1.8 | 2.6 |
| | | Rampant | 134.2 | 90.7 | 95 | 21.4 | 15.1 | 4.6 | 4.0 |
| L16 For Carnage, Apply Within (50 s) | goo | Flagship | 147.8 | 98.4 | 105 | 19.6 | 15.7 | 2.0 | 2.0 |
| | | Rampant | 119.2 | 63.7 | 65 | 30.4 | 22.6 | 5.6 | 3.8 |
| L21 My Own Private Thermopylae | water | Flagship | 158.1 | 101.7 | 105 | 17.8 | 13.6 | 2.1 | 3.1 |
| | | Rampant | 122.1 | 76.1 | 71 | 25.5 | 17.9 | 5.1 | 4.2 |
| L23 Where the Twist Flops | lava | Flagship | 148.8 | 110.2 | 114 | 17.2 | 12.9 | 2.1 | 3.2 |
| | | Rampant | 115.9 | 76.7 | 86 | 24.6 | 17.8 | 4.8 | 3.7 |
| L28 All Roads Lead to Sol | all four | Flagship | 142.4 | 81.7 | 77 | 22.8 | 18.3 | 2.0 | 3.4 |
| | | Rampant | 111.0 | 62.1 | 58 | 31.4 | 23.5 | 5.3 | 4.4 |
| Net: Giant Flaming Pit | lava | Flagship | 163.6 | 112.3 | 113 | 16.6 | 12.5 | 2.0 | 2.0 |
| | | Rampant | 133.0 | 80.6 | 79 | 22.4 | 16.3 | 4.4 | 2.6 |

GPU stage times run from each pass's start to its end with frames in
flight, so they overlap and add up to more than the frame interval; read
them against each other, not as the frame budget.

## Reading

- Rampant holds 60 at the 1% low on all six (62 to 91); the worst second
  dips to 58 once, on L28 at 65 s, where Flagship has its worst second
  too (77).
- Rampant costs 19 to 29% of the average against Flagship.
- Where it goes, at the p99: the world pass +3.8 to +6.9 ms (traced shadows,
  reflections, the figure lights), traced ambient shadows +2.4 to +3.6 ms
  (about 2.5 times the screen-space pass), the fog +0.5 to +1.8 ms. The
  bake and averages stay under 2 ms.
- Rampant's slow frames are GPU-bound: the CPU spends them waiting for a
  drawable (11 to 18.5 ms), not on the tick or the render.

Per-frame CSVs, summaries and stage timings: `.deps/bench/r13-unseen/`.
