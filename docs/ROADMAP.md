# Durandal — roadmap and options

A menu to choose from, not a plan of record. Each item is rated, then there
is a recommended order and the decisions I need from you. Evidence is in
`docs/AUDIT.md`.

## Principles

These come out of the research. The ports people keep playing restore;
the ones they drift away from reinvent.

1. **The default look is Marathon at native resolution**, not a
   reinterpretation. GZDoom rebuilt its light modes in 2023 because the
   "modern" ones made levels look nothing like their designers intended.
   The path-traced Doom ports were widely called curiosities because they
   threw away sector lighting. Nightdive's KEX remasters stress
   restoration: native resolution, high frame rate, quirks kept, even Doom's
   fuzz effect evaluated per texel.
2. **Modern light is anchored to Marathon's light.** Every surface keeps
   its authored brightness on average; new techniques only redistribute
   light within it (AUDIT §3.6). The dial runs from 0 (stock) upwards.
3. **Faithful to the texel.** 128×128 art stays crisp: lighting is
   quantised per source texel, and the shading-table banding is available
   in HDR. The Force Engine offers the same idea as its
   8-bit / 8-bit-interpolated / true-colour choice for Dark Forces.
4. **Everything switchable, one control for tiers,** and a Stock tier that
   is exactly today's game.
5. **Game state is untouchable.** Film tests stay green; saves and films
   are unaffected.

## Rating key

| Rating | Meaning |
|---|---|
| **Impact** 1–5 | How much you'd notice: look, sound or feel. |
| **Effort** S / M / L / XL | S = one working session. M = 2–4. L = 5–10. XL = more than 10. |
| **Risk** Low / Med / High | Chance of regressions, missing 120 fps, or a result that needs rework. |
| **Faithful** 1–5 | 5 = indistinguishable in spirit from the original. 1 = a reinterpretation. |

Performance is a separate concern. The flagship budget at 1080p/120 is
**8.3 ms per frame** on a fanless base M5 with an 8-core GPU. Marathon's
geometry is tiny (about 3,000 triangles per level), so rasterising is
cheap. The budget goes on ray tracing, volumetrics and post-processing. The
estimates below are mine and will be checked against the benchmark as each
item lands.

---

## The menu

### Foundation

| # | Item | Impact | Effort | Risk | Faithful | Notes |
|---|---|---|---|---|---|---|
| F0 | **Housekeeping.** Give the app its own name and folders, so it never shares preferences or saves with a stock install. Add a `<durandal>` preferences section, feature flags (on in Debug, off in Release) and the quality-tier scaffold. Record the benchmark baseline. | 1 | S | Low | 5 | Prerequisite for everything. |
| F1 | **Per-frame mouse look and input polling.** Look is currently sampled at 30 Hz. Apply the not-yet-simulated mouse movement to the view every frame, and poll input every frame. | 4 (feel) | S–M | Low–Med | 5 | Render side only: aim still enters the simulation per tick, so films and net play are unaffected. The biggest "feels like 120 Hz" win. |
| F2 | **Finish interpolation.** Liquid heights, light intensities, fades and texture slides are currently 30 Hz steps; interpolate them per frame. | 3 | S–M | Low | 5 | Interpolates values the tick has already computed. Never re-evaluates light functions. |
| F3 | **Native Metal world renderer.** Option A: `RenderRasterize_Metal` behind the existing interface, reusing the portal visibility and sort code. Plus a level-geometry mirror for later ray-tracing, liquid and audio work. | 2 on its own (parity) | XL | Med–High | 5 | Must match OpenGL side by side before anything is built on it. Keep GL as a fallback switch until round 3. |
| F4 | **Metal 2D layer.** HUD, automap, terminals, menus, fonts and fades through one small `Renderer2D`. | 2 | L | Med | 5 | Removes the last GL. Required before the whole frame is Metal. |
| F5 | **Presentation.** CAMetalLayer in the SDL window, `CAMetalDisplayLink` pacing, 120 Hz and above, and an EDR-capable half-float swap chain. SDR output is identical when EDR is off. | 3 | M | Med | 5 | 120 Hz can only be *seen* on a display running above 60 Hz (AUDIT §8). |
| F6 | **MetalFX upscaling.** Spatial for native 5K full screen; temporal only for the ray-traced tiers above 1080p. | 2 | M | Med | 3 | **Not needed at 1080p**, where native rendering is cheap. Temporal accumulation softens texels and ghosts sprites, which works against principle 3. Recommend deferring until the ray-tracing tier needs it. |
| F7 | **One quality control.** Tiers Stock → Classic → Enhanced → Flagship, plus Custom (definitions below). | 3 | S, then grows | Low | 5 | Scaffold in F0; each feature registers itself as it lands. |

### The faithful look core

| # | Item | Impact | Effort | Risk | Faithful | Notes |
|---|---|---|---|---|---|---|
| L1 | **Shading-table-faithful shading.** Use Marathon's real shading tables in the shader, in three modes: *banded* (exact 8-bit look), *interpolated* (smooth between bands) and *true colour*. The HDR option keeps the banding on intensity before tone mapping. | 4 | M | Low | 5 | The current GL path approximates with a continuous formula and never uses the tables. GZDoom's "Software" light mode and The Force Engine's colour modes are the precedents. |
| L2 | **Texel-faithful lighting.** Snap the lighting position to the source-texel centre (screen-space derivatives) before the depth term and all dynamic light, so each texel gets exactly one shade. | 4 | S–M (after L1) | Low | 5 | Stops modern light from smearing across 128×128 art. Every later lighting item inherits it. |
| L3 | **Per-texel transfer modes.** Evaluate static, fade-out and wobble at texel scale, as KEX does for Doom's fuzz. Implement "smear" and re-enable "fade to black", both missing from the GL path. | 2 | S | Low | 5 | Restores missing effects. |
| L4 | **Crisp filtering.** Sharp-bilinear: pixel-honest texels with anti-aliased texel edges. Plus anisotropic filtering and geometry-edge anti-aliasing. | 3 | S | Low | 4 | Removes shimmer without blurring. Nearest stays available. |

### Glow and light

| # | Item | Impact | Effort | Risk | Faithful | Notes |
|---|---|---|---|---|---|---|
| E1 | **Emissive HDR, bloom and EDR.** Three sources: (a) monster self-luminous palette entries (eyes, armour lights); (b) per-frame `minimum_light_intensity` on projectiles, explosions, muzzle flashes and lava/Pfhor scenery; (c) a **curated emissive mask for the 120 M2 wall bitmaps** (screens, lamps, panels), generated by luminance and saturation, then reviewed by eye and scaled by the surface's live light. Bloom is fed only by emissive texels; EDR headroom goes on emissives only. | 5 | M | Med | 4 | M2's walls have **no** self-luminous palette entries (AUDIT §3.4), so (c) is needed for screens to glow. The masks ship as a plugin. Judged on the display. |
| E2 | **Dynamic lights from projectiles, explosions and muzzle flashes.** A few point lights per frame, portal-limited, added to intensity *before* the shading-table lookup so they band like the original. | 4 | M | Low–Med | 3 | New behaviour: in the original, muzzle flash only brightens the shooter's own view. Behind a toggle; on from Enhanced. |
| E3 | **Ray-traced shadows** for the E2 lights. Hardware ray tracing (M3 and later), sprites as alpha-tested cards, walking the portal graph to respect 5D space. | 3 | M–L | Med | 4 | Tight shadow budget of 1–2 ms. |
| E4 | **Ray-traced redistribution and one-bounce GI.** Fixtures (E1c) and bright ceilings become area lights. Shade each surface as its authored brightness × a clamped ratio of traced irradiance (AUDIT §3.6), plus a capped colour-bleed bounce. Sky light enters through landscape surfaces. Computed in a **surface cache**: world-space, at reduced texel density, updated incrementally. That keeps it stable and texel-aligned without screen-space temporal smearing. | 5 | XL | High | 3–4 | The flagship feature, and the one most likely to strain 8.3 ms on an 8-core GPU; estimate 3–4 ms at 1080p. Needs E1 and E3 first. Degrades to lower update rates or density on lower tiers. |
| E5 | **Headlight as a soft spotlight with shadows**, instead of the flat distance fade. | 2 | S | Med | 2 | Changes Marathon's feel. Listed for completeness; not recommended as a default. |

### Materials

| # | Item | Impact | Effort | Risk | Faithful | Notes |
|---|---|---|---|---|---|---|
| M1 | **Derived normal, height and specular maps** for walls (metal panels, alien stone), with restrained parallax. Generated from the original textures and hand-tuned per bitmap. | 3 | L | Med | 3 | Only visible under E2–E4 lights or the headlight. Otherwise it adds nothing: a flat-lit room shows no relief. So it comes late. Ships as plugin data. |

### Liquids

| # | Item | Impact | Effort | Risk | Faithful | Notes |
|---|---|---|---|---|---|---|
| W1 | **Real liquid surfaces**: water, lava, sewage and goo, each with its own look. Animated normals, refraction of what's below, **depth murk** from the true floor depth under the surface, caustics on the floor, and a lit surface on lava. Handles rising and falling liquid heights (interpolated per F2). Underwater view distortion. | 5 | L | Med | 3–4 | Marathon's liquids are drawn as flat floors today. The ordering rules are already in the portal renderer; the geometry mirror gives depth below the surface. |

### Atmosphere

| # | Item | Impact | Effort | Risk | Faithful | Notes |
|---|---|---|---|---|---|---|
| V1 | **Volumetric fog and dust** lit by scene lights, starting from the existing MML fog settings. Stronger in lava and sewage levels and underwater. | 4 | L | Med–High | 3 | Easy to overdo; must default subtle. Budget about 1 ms at quarter resolution. |
| V2 | **Landscapes as HDR sky.** Landscape art with EDR headroom on bright areas; it feeds E4's sky light in open areas. | 3 | M | Low–Med | 4 | Also fixes the sky's cylinder or sphere projection at wide field of view. |

### Sprites

| # | Item | Impact | Effort | Risk | Faithful | Notes |
|---|---|---|---|---|---|---|
| S1 | **Correct sprite lighting.** Sprites take their polygon's floor light only. Blend in the ceiling light and nearby dynamic lights, the same across all 8 angles. | 3 | M | Low–Med | 4 | |
| S2 | **Soft depth fade** where sprites meet floors, walls or liquid. | 2 | S | Low | 4 | Removes hard cut lines. |

### Terminals, HUD and menus

| # | Item | Impact | Effort | Risk | Faithful | Notes |
|---|---|---|---|---|---|---|
| T1 | **Crisp terminals at any resolution.** Render the 640×320 terminal at native pixel scale using the existing TrueType fonts, with page breaks unchanged and smooth-scaled pictures. **Optional CRT treatment**: scanlines, slight curvature and bloom. | 3 | M | Low | 5 (CRT 3) | Today the terminal is a 640×320 image stretched about 6× at 4K. |
| T2 | **Crisp HUD and map.** Glyphs rasterised at display scale; the motion sensor and HUD at native resolution. Menus stay 640×480 art, upscaled cleanly. | 2 | M | Low | 5 | |

### Sound

| # | Item | Impact | Effort | Risk | Faithful | Notes |
|---|---|---|---|---|---|---|
| A1 | **Geometry-driven reverb.** Flood-fill from the listener through open portals to estimate room volume and surfaces, feed an OpenAL EFX reverb, and use an underwater preset when submerged. | 4 (sound) | M | Low | 4 | OpenAL Soft 1.23.1 already has EFX; only reverb is missing. No new dependency. |
| A2 | **Graded occlusion**: height-aware, and following the portal path around corners. Also fixes the listener-velocity bug and updates the listener per frame. | 3 | S–M | Low | 4 | Uses a sound-only copy of the obstruction test; the AI's copy stays untouched. |

Sound is independent of the renderer, so it can run in parallel with any
graphics round.

---

## Evaluated and not recommended

| Idea | Why not |
|---|---|
| Full path tracing (as in PrBoom RT) | Replaces Marathon's authored light and loses the atmosphere. Well beyond a base M5 at 120 fps. E4's anchored approach gets most of the benefit. |
| Baked lightmaps (as in VKDoom) | Marathon's platforms move and its lights animate constantly; bakes break on both. |
| MetalFX frame interpolation | We already get real interpolated frames from the engine. Generated frames add latency, work against F1, and ghost sprites. |
| MetalFX temporal upscaling at 1080p | Not needed at that resolution and softens texels. See F6. |
| AI-upscaled textures, or 3D models replacing sprites | Changes the art. Out of spirit for the default. Third-party HD packs still load through the existing plugin path if you ever want them. |
| Steam Audio / Apple PHASE | New dependency for little gain over EFX on a 2.5D map. |

---

## Quality tiers

The one menu control. Every item is also individually switchable
("Custom").

| Tier | Contents |
|---|---|
| **Stock** | Today's game exactly: M2 first-run settings, 30 fps, nearest filtering, no enhancements. The way back. |
| **Classic** | Stock look, played modern: 120 fps interpolated with F1 and F2, Metal, exact shading tables (L1, banded or interpolated), texel-faithful (L2), per-texel transfer modes (L3), crisp filtering (L4), crisp terminals and HUD (T1, T2), graded occlusion (A2). |
| **Enhanced** | Classic, plus emissive, bloom and EDR (E1), projectile lights (E2), sprite lighting and fade (S1, S2), liquids (W1), HDR sky (V2) and reverb (A1). |
| **Flagship** | Enhanced, plus ray-traced shadows (E3), ray-traced redistribution and GI (E4), volumetrics (V1) and materials (M1). Target: 120 fps at 1080p after warm-up. |

---

## Recommended order

Each round ends with a tag, a benchmark run, the film tests and your
manual QA.

1. **Round 1 — Foundations and feel** (on the current engine): F0, F1, F2,
   plus the baseline benchmark.
   - Small, low risk, and immediately noticeable at 120 Hz.
   - Establishes the preferences, flag and tier plumbing everything else
     uses.
2. **Round 2 — Metal parity:** F3, F4, F5.
   - Acceptance: matches OpenGL side by side, film tests green, benchmark
     no worse than OpenGL.
   - The longest round, with nothing new to look at. It is the foundation
     for everything after it.
3. **Round 3 — The faithful look core:** L1–L4, T1, T2. This completes the
   **Classic** tier and retires OpenGL.
4. **Round 4 — Glow:** E1 and V2, plus the EDR display work. This is where
   the first big visual change arrives, judged on your display.
5. **Round 5 — Light:** E2, S1, S2, then E3.
6. **Round 6 — Water:** W1. Completes the **Enhanced** tier.
7. **Round 7 — Air:** V1.
8. **Round 8 — Flagship:** E4, then M1 and, if needed, F6.

**Sound (A1, A2)** can slot in alongside any round from 1 onwards. A2 is
small enough to fold into round 1.

If you'd rather see something new sooner, one alternative is worth
considering. **Swap rounds 2 and 3, and build L1–L2 on the existing GL
shader path first.** You'd see the faithful look within a round or two.
The cost is writing those shaders twice, once in GLSL and once in Metal.
I don't recommend it, but it's a legitimate trade.

---

## Decisions I need from you

1. **First round.** My recommendation is round 1, with A2 folded in.
2. **Baseline benchmark run.** The game will run full screen and windowed
   on your main display for about 28 minutes. The Mac should be plugged
   in, with nothing else running, and you shouldn't use it during the run.
   OK to run it, and when?
3. **Display.** Does the MSI offer a refresh rate above 60 Hz, and an HDR
   mode? Look in System Settings → Displays → the MSI → Refresh rate, and
   whether a "High Dynamic Range" switch appears. This decides how we judge
   120 Hz and EDR.
4. **App identity (F0).** Rename the app to **Durandal** with its own
   folders? Its preferences and saves would then live in
   `~/Library/Preferences/Durandal/` and
   `~/Library/Application Support/Durandal/`, separate from any stock
   install.
5. **Projectile and muzzle-flash lights (E2)** light the world, which the
   original never did. Are you happy for that to be on by default from the
   Enhanced tier?
