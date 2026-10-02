# Rampant: ground truths for the guide

Facts from the repository and the bench runs as of 2 October 2026 (branch
`durandal/round-13-rampant`), for updating `docs/GUIDE.md` and
`docs/GUIDE.html`. Everything here was checked against the code or measured;
where a number is approximate it says so. Keep the two editions' facts in
step.

## 1. Where Rampant stands

- Rampant is the fifth tier, above Flagship. The Quality menu reads Stock,
  Classic, Enhanced, Flagship, Rampant, Custom. It is stored as
  `quality_tier` 5; Custom stays 4, so existing preferences files are
  unaffected.
- Every Rampant feature is **still in QA**. Until one passes, the menu offers
  Rampant only in a test build (Debug, or launched with `DURANDAL_QA=1`,
  which the Xcode Run action sets). Once any of its features passes, the
  menu offers it to every launch.
- Its switches are on a new **RAMPANT** tab in Preferences > DURANDAL. The
  tabs are now Feel, Look, Art, Light, Rampant, Cheats.
- The tab's own note reads: "More than this machine was built for. Metal
  renderer only."
- Choosing Rampant also sets Redistribution Strength to **Strong**. The other
  named tiers set Medium.
- Like everything else it is switchable, Stock still restores upstream
  exactly, and the film test still passes: 43 films, 86 checks, run three
  ways (stock, default, everything in QA on).

## 2. The tier ladder, as the guide's part X should now read

| Tier | What it adds |
|---|---|
| Stock | Aleph One exactly: OpenGL, 30 fps, nothing added |
| Classic | The Metal renderer, Marathon's own shading, crisp art and text, wide screens, every frame the display can show |
| Enhanced | Classic + glow, cast light and its shadows, liquids, reverb, the new camera |
| Flagship | Enhanced + the air (fog), travelling light, relief, depth (ambient and character shadows), the HD art |
| **Rampant** | Flagship + bounced light, traced shadows from every figure, reflecting water, traced ambient shadows, dust and embers, heat shimmer |

## 3. Rampant's six switches

| Switch | Stored as | Needs | What it does |
|---|---|---|---|
| Bounced Light | `light_bounce` | Light Redistribution | Light that reaches a surface passes on to the next, frame after frame, so a lit room fills in its corners and colour carries from wall to wall. The sky gives off the landscape's own average colour. Figures (monsters, BOBs, the HD sprites) take the light of the floor and ceiling around them. When a room's light changes for good (a switch thrown, a light failing), its surfaces settle again rather than keeping the old light. |
| Traced Shadows | `traced_shadows` | Dynamic Lights, Light Shadows | Monsters, items, scenery and corpses cast shadows from cast light (bolts, explosions, muzzle flashes), from their own silhouettes, not a blob. Edges are sharp at the feet and soften with distance, by the size of the light. Grates throw patterned shadows. Figures also cut shafts out of lit fog. Up to 128 figures near lights, in view or not. |
| Reflecting Liquids | `reflections` | Real Liquids | Water, sewage and goo reflect the room above them, the sky, and figures standing by them, broken by ripples. Waders and splashes make ripple rings. What lies below bends (refraction, approximated). |
| Traced Ambient Shadows | `traced_ambient` | Ambient Shadows | Ambient shadows found by tracing short rays through the level and the figures, not read from the picture: they know what is off screen and behind things. Within 10 world units of the viewer (8 rays a pixel to 4 units, 4 beyond); further off, the screen-space estimate as before. |
| Dust and Embers | `dust_embers` | (none) | Dust drifting in every room in view, lit by the room and glinting where cast light passes through it; rooms open to the sky carry more. Over real lava, embers rise off the surface, glowing, and fade as they climb. Made from the world time, so a film shows the same motes at the same tick. |
| Heat Shimmer | `heat_shimmer` | Bloom | The air over lava wavers. Only over lava surfaces and the air above them. |

All six are Metal renderer only and read-only on the game world: films,
saves and the rules are untouched.

Tuning after the owner's first looks:
- the embers were halved in brightness and glow (1 Oct);
- heat shimmer was confined to lava, after it showed on every wall (1 Oct);
- bounced light was steadied, after it made walls ripple like a pool's ceiling (1 Oct);
- the water reflection is a quarter weaker than full Fresnel, with the ripples kept as they were (2 Oct).

## 4. Part XII ("The edge of the known"): what is no longer true

Each claim in the current text, against what Rampant does:

| The guide says | Now |
|---|---|
| "Nothing here is ray traced." | Rampant traces rays: shadows toward each light, reflections and refraction off liquids, ambient shadows, and the light bounce. It traces them on the GPU through Marathon's own map (the same polygon walk the engine's line of sight uses), not with the M5's ray-tracing hardware (see section 7). Below Rampant, nothing is traced. |
| "Cast light ... is exact for walls and blind to sprites." | Still true below Rampant. In Rampant, figures cast shadows from cast light (Traced Shadows). |
| "Ambient shadows are read from the picture ... they know only what is on screen." | Still true below Rampant and beyond 10 world units. In Rampant, nearby ambient shadows are traced through the world. |
| "The water does not refract and does not reflect." | In Rampant it reflects (the room, the sky, figures) and refracts (approximated). Not lava, and not seen from under the surface. |
| "Light travels once across a room and stops." | In Rampant it keeps travelling (Bounced Light). |
| "The Jjaro wall set has not had its lamps marked." | **Correction:** Marathon 2 has no Jjaro walls at all. Its collections 20 and 25 are empty, so there is nothing to mark. The line can go. |

Still true, unchanged by Rampant:
- Models do not animate.
- The Pfhor are sprites.
- The view pitches to about 65 degrees and no further. Straight up and down would need a second, backward visibility walk, which is planned for a later round.
- Film export is not available on the Metal display. It is low priority: screen recording does the job.
- The OpenGL renderer gets none of this.

New limits worth stating:
- Reflections use the wall art at 256 pixels, from the HD pack where installed, else the 8-bit art. Their figures are flat cut-outs lit by the floor's light, not fully lit.
- Shadows from figures come only from the four lights nearest the viewer that are bright enough. Weak and distant lights still light, but cast no figure shadows.
- Dust is drawn within 16 world units.

## 5. Performance (measured, full screen)

M5 MacBook Air (Mac17,3), 16 GB, on mains power, macOS 27. MSI display at
1920 x 1080, 240 Hz, uncapped, vsync off, the four HD packs installed.

**Six films never used before (2 Oct 2026)**: each film's first two minutes,
once per tier, in alternating order, after a warm-up. The Mac's thermal
state read "fair" throughout. Another session ran CPU-heavy jobs on and off,
so absolute numbers may be a touch low; the comparison is fair.

| Film | Liquid | Flagship avg / 1% low | Rampant avg / 1% low |
|---|---|---|---|
| L08 Nuke and Pave | lava | 189 / 133 | 134 / 91 |
| L16 For Carnage, Apply Within (50 s) | goo | 148 / 98 | 119 / 64 |
| L21 My Own Private Thermopylae | water | 158 / 102 | 122 / 76 |
| L23 Where the Twist Flops | lava | 149 / 110 | 116 / 77 |
| L28 All Roads Lead to Sol | all four | 142 / 82 | 111 / 62 |
| Net game: Giant Flaming Pit | lava | 164 / 112 | 133 / 81 |

- **Target:** 60 fps at the 1% low, reaching for 120.
  - Rampant held 60 at the 1% low on all six (62–91).
  - Its worst single second was 58 fps, once, on L28 at 65 s, the same moment as Flagship's worst (77).
  - Its average was 111–134.
- **Cost against Flagship:** 19–29% of the frame rate.
- **Where the extra time goes, in the slowest frames:**
  - the world pass, +4 to +7 ms (traced shadows, reflections);
  - traced ambient shadows, +2.5 to +3.5 ms;
  - the fog, +0.5 to +2 ms.

  Rampant's slow frames wait on the GPU, not the CPU.
- **Earlier (30 Sep), level 6 to tick 1200:** Rampant averaged 177 with a 1% low of 93; Flagship 226 and 125.
- **The guide's Flagship line still holds:** "130 to 200 frames a second" matches 142–189 here.

Full table: `docs/benchmarks/r13-unseen-films.md`.

## 6. Memory

- Rampant adds about 250 MB of GPU arrays:
  - the figure silhouettes, 512 slices of 256 x 256 RGBA with mips, about 170 MB;
  - the wall art for reflections, 256 slices of 256 x 256 RGBA with mips, about 85 MB.
- **Footprint at the end of each run today:** Flagship 390–660 MB, Rampant 655–925 MB, depending on the level.
- The guide's "about 350 MB" for Flagship was measured on one level. Today's Flagship runs, with the HD set on six different levels, ended at 390–660 MB footprint. Worth saying "a few hundred MB" rather than one number.

## 7. Why not the M5's ray-tracing hardware

- The M5 has ray-tracing hardware, usable from fragment shaders (Apple 10 family).
- A standalone test (`scripts/trace-spike.swift`) traced 1M rays per set on eight levels both ways: the engine's own walk through Marathon's polygon map, and a hardware acceleration structure of the same geometry.
- **5D space:** 14 of Marathon 2's 28 solo levels have rooms that overlap without being neighbours. A triangle structure cannot represent that, so the hardware answers wrongly there:
  - shadow segments: 2.7% wrong (13% on 5-D Space);
  - long rays: 3.5% wrong.
- **Speed:** the walk was quicker for long rays in coherent blocks, as neighbouring pixels cast them: 1.73 M rays per ms against 1.35 M. The hardware won for scattered rays.
- **So the walk is the tracer:** exact in 5D space, with no acceleration structure to build. The hardware stays in reserve.
- **A fault found on the way:** the engine's own light check never tested the height where a segment ends. Light leaked between stacked rooms: 1.6% of segments on level 4, 9.9% on level 10. Rampant's shadows fix it. Whether Flagship should too is the owner's call, still open.

## 8. Cheats: Summon BOBs (new, in QA)

- **What it does:** pressing the Summon BOBs key brings five armed security BOBs ("steve": pistol, the toughest BOB, fires every second) in beside and behind the player.
  - They teleport in with the usual effect and sound, then go for the nearest aliens.
  - They are placed where they could walk from the player in a straight line: no wall in the way, on the player's level, not over lava, goo or deep liquid, and clear of other figures.
  - With no room, the screen says "Summon BOBs: no room here".
- **Key:** C by default. It is unbound in upstream's default keys and in the tested key set, and can be changed on the Cheats tab ("Summon BOBs Key").
- **Where it works:** single-player games only, never replays or net games.
- **Films:** like switching a cheat in mid-game, a summon stops that game's film recording; a film cannot carry it. The guide's line "A film recorded with them carries them" holds for God Mode, All Weapons and Noclip begun with the game, not for summoning.
- **Shapes:** while the cheat can be used, the security BOB's shapes load with every level, including levels that have no BOBs of their own.
- **Saves:** a game saved after a summon restores with its BOBs in any build.
- **Gate:** it is in QA, so the key shows and works only in a test build until passed.

The other cheats are unchanged: Level Select on New Game, God Mode (key H in the tested key set; default G), All Weapons and Ammo, and Noclip (key N).

## 9. The illustrated edition's asides

- Part X's note, "There is no fifth tier. I would like one. I have started drawing it.", is now out of date: there is one, in QA.
- The rewrite line "Four tiers, and not one of them is enough..." can move on as well.
- The tiers grid in GUIDE.html is four columns (`.tiers`, `repeat(4, 1fr)`), so a fifth tier needs the grid widened, or a second row.
- The part X heading "Four ways to run it" becomes five.
