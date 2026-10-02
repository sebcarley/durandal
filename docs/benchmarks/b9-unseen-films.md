# Benchmark: baseline-9 on seven films not used before

- Date: 2026-10-02 23:33 to 2026-10-03 00:00, a few minutes after a reboot
- Build: `baseline-9` (relaxed shader maths, the exact light fixes,
  per-surface light culling, the corpse fix, the skies)
- Machine: Mac17,3, Apple M5, 16 GB, mains power
- Display: MSI MAG 272U X24, full screen, 1920 x 1080 at 240 Hz
- Settings: uncapped, vsync off, 3 drawables, sound off, GPU stage timing
  on, `DURANDAL_SET=quality_tier=3` (Flagship) or `=5` (Rampant), the HD
  packs installed
- Each film's first 3600 ticks (or the whole film where shorter), once per
  tier, alternating which goes first, after an untimed warm-up. The thermal
  state read "fair" throughout.
- Post-boot noise: the Neural Engine compiler held one core at the start,
  and `dasd` and the App Store agent were busy at the end, so absolute
  numbers may be a touch low.

| Film | Liquid | Flagship avg / 1% low / worst second | Rampant avg / 1% low / worst second |
|---|---|---|---|
| L05 Come and Take Your Medicine (47 s) | water | 288 / 131 / 201 | 198 / 102 / 123 |
| L11 The Hard Stuff Rules | lava, 5D (43 pairs) | 251 / 127 / 161 | 220 / 107 / 112 |
| L14 IIHARL | goo | 239 / 122 / 167 | 213 / 106 / 119 |
| L22 Kill Your Television | water | 263 / 172 / 197 | 276 / 149 / 148 |
| L24 Beware of Abandoned Rental Trucks | sewage, 5D (44 pairs) | 235 / 111 / 113 | 202 / 78 / **35** |
| L24, after the fix below | | 243 / 121 / 130 | 236 / 98 / 93 |
| L27 Feel the Noise | lava | 240 / 167 / 148 | 205 / 109 / 107 |
| Net: House of Pain | | 262 / 134 / 194 | 237 / 158 / 164 |

## Reading

- Rampant held 98 or more at the 1% low on every film once L24 was fixed;
  Flagship 111 or more. Averages: Rampant 198 to 276, Flagship 235 to 288.
- Against the bench of 2 October on other films with baseline-8, Rampant
  then averaged 111 to 134 with a 1% low of 62 to 91.
- Run-to-run drift on a warm, busy machine is visible: on L22 Rampant's
  average came out above Flagship's.

## L24's spike, and its fix

Rampant's worst second on L24 was 35 frames, at about 22 s (ticks
675-700), with world-pass frames up to 140 ms; Flagship was untroubled
there. One feature off at a time over that stretch pointed at Traced
Shadows, and fewer figure-casting lights shrank the spike in proportion
(none: longest frame 23 ms; two: 38; four, the default: 75). At that
moment an explosion fills the view, layer on layer of large blended
sprites, each lighting the room, while about ninety figures stand nearby,
up to 52 listed in one polygon (`DURANDAL_OCCLUDER_LOG=1`). Every pixel of
every fire layer walked each figure-casting light through that crowd.

A wall or sprite already at full light (a self-lit frame such as an
explosion) cannot be lit further: the shaders take min(colour + light, 1).
Such draws now get no lights to walk (`RenderRasterize_Metal::draw`; not
liquids, whose glints add on top). Frame shots of the stretch are
pixel-identical to the build before. L24 afterwards: worst second 93,
longest frame 28 ms.

Per-frame CSVs, summaries and stage timings: `.deps/bench/b9-unseen/`,
`.deps/bench/b9-unseen-fixed/`, `.deps/bench/b9-l24-*`.
