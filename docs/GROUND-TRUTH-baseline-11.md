# Ground truths for the guide: baseline-8 to baseline-11

Facts from the repository and the bench runs as of 3 October 2026 (tag
`baseline-11`), for updating `docs/GUIDE.html` and `docs/GUIDE.md`. The guide
was last placed at `baseline-8` (2 October). Everything here was checked
against the code or measured; where a number is approximate it says so. Keep
the two editions' facts in step. Neither edition has been touched since
`baseline-8`.

## 1. What changed since the guide was placed

| Tag | Date | What |
|---|---|---|
| `baseline-9` | 2 Oct | Relaxed shader maths (about twice as fast), three exact light fixes, per-surface light culling, the corpse-bar fix in traced ambient shadows, skies that stay with the world |
| `baseline-10` | 3 Oct | Surfaces already at full light walk no lights (explosion spikes in crowds); rebench on seven films not used before |
| `baseline-11` | 3 Oct | **Get HD Art**: the community's HD packs fetched and installed from inside the game, one button. QA passed |

Nothing is in QA now: every feature is released to every launch.

## 2. Get HD Art (new; part VI)

**Where:** Preferences > DURANDAL > **ART** tab, a button labelled
**GET HD ART...**, below the soundtrack and the note "Packs go in Application
Support/Durandal/Plugins."

**What it opens:** a dialog titled GET HD ART. Its opening lines read:

> The community's HD art for Marathon 2: walls and sky, monsters,
> scenery, weapons and 3D pickups, from its authors' own pages on
> Simplici7y. It is theirs: Durandal fetches it but never ships it.

Then one line per pack, then a summary line, then **DOWNLOAD** and **CLOSE**.

**The five packs** (the set the guide was written with, as
`scripts/get-hd-art.sh` fetches):

| Line in the dialog | What it is | Download |
|---|---|---|
| CFP - Walls M2 | walls and skies (Community/Freeverse) | about 373 MB |
| CFP Monsters | every monster and figure | about 516 MB |
| CFP Scenery | scenery | about 24 MB |
| CFP Weapons M2 | weapons in hand, items | about 66 MB |
| 3D Items | the 3D pickups (thedoctor45) | about 3 MB |
| **All five** | | **about 1 GB** (982 MB) |

**What each line says, in order as it works:** "waiting (about N MB)" or
"already installed, left alone"; "finding the authors' download";
"downloading, X of Y MB"; "checking the archive"; "unpacking"; "installed".
If a pack fails: "not installed: " and the reason. If CLOSE stops it:
"cancelled".

**The summary line:**
- Before: "N to fetch, about N MB. They go in Application
  Support/Durandal/Plugins."
- With all five present: "All five packs are installed." (DOWNLOAD is
  greyed out.)
- Short of disk space: "Not enough free space: about N GB is needed."
  (DOWNLOAD greyed out.)
- While fetching: "Fetching. Close cancels; packs already installed stay."
- At the end: "N installed. The HD art applies from the next level." With a
  failure: "N installed, N not (see above; scripts/get-hd-art.sh or by
  hand)."

**How it behaves:**
- **The source:** each pack comes from its authors' own Simplici7y page, by
  the download link they publish (the Google Drive files through Drive's
  direct download). Nothing is re-hosted. The guide's "it is theirs, so it
  is not kept in this repository" and the credits' "none of it is
  distributed here" both stay true.
- **One pack at a time**, in the background: the game stays responsive and
  the dialog updates as it goes.
- **Never overwrites:** a pack whose folder is already in the Plugins
  folder is left alone. That includes one installed by hand or by the
  script.
- **Checks:** each download must be a zip archive. It is compared (SHA-256)
  with the version tested. If the authors have since updated it, it
  installs anyway and its line says "installed (a newer version than
  tested)".
- **CLOSE while fetching** cancels the current download and closes once it
  has stopped. Packs already installed stay installed.
- **Disk space:** it needs about three times the download free (the
  archive, the unpacked copy and a margin). For all five that is about
  3.4 GB; the dialog rounds it up and asks for "about 4 GB".
- **No restart:** new packs join the game's plugin list at once. Their art
  loads from the next level.
- **Which tiers use it:** the packs follow the ART tab's HD switches (HD
  Walls and Sky, HD Monsters, HD Weapons and Items, HD Scenery, 3D
  Pickups), so the art shows in **Flagship and Rampant**, or wherever those
  switches are on in Custom. Like all HD art, it needs the Metal renderer.
- **Nothing new is added to the Mac:** it uses only what macOS already
  provides (its own download system, checksum library, `ditto` and
  `unzip`).
- **Hidden with `DURANDAL_STOCK=1`**, like every Durandal feature.

**Still true alongside it:**
- `scripts/get-hd-art.sh` and `scripts/setup.sh` do the same from a
  terminal, and `setup.sh --no-art` skips the art (the button can fetch it
  later).
- Any other Aleph One pack still works by putting it in the Plugins folder;
  the button fetches only these five.
- **Tested:** CFP Scenery and 3D Items were fetched for real by a test
  harness on 3 Oct, and both installed matching the tested versions. The
  three large packs come by the same Google Drive route as CFP Scenery.
  The in-game QA passed on 3 Oct.

**Suggested change to part VI:** where it says "it is one command away",
the art is now one button away (Preferences > DURANDAL > ART > GET HD
ART...), with the script still there for terminal users. The illustrated
edition's sentence is in the first paragraph under "Thirty years of other
people's work". Its diagram line, "Choose by kind: walls and sky, monsters,
weapons and items, scenery.", stays true.

## 3. Performance (parts X and XII): every figure has moved

The guide's figures were measured at `baseline-8`. Since then:
- **Relaxed shader maths.** The shaders had been compiled in strict IEEE
  maths, chosen in the early rounds so the Metal renderer matched OpenGL
  pixel for pixel. Relaxed maths roughly halves the GPU time per frame,
  with no visible change on four films (5-D Space included).
- **Per-surface light culling.** Each surface walks only the lights that
  can reach it, in the same order as before, so the picture is
  bit-identical. This helps firefights most.
- **Surfaces at full light walk no lights.** An explosion already at full
  brightness cannot be lit further. This removed the one bad spike found:
  an explosion in a crowd of about ninety figures on L24.

### The new bench (official: on screen)

- **When:** 2–3 October 2026, a few minutes after a reboot.
- **Machine:** M5 MacBook Air (Mac17,3), 16 GB, on mains power, macOS 27.
- **Display:** MSI MAG 272U X24, full screen, 1920 x 1080 at 240 Hz.
- **Settings:** uncapped, vsync off, the four HD packs and 3D Items
  installed.
- **Films:** seven not used before. Each film's first two minutes (L05's
  film is 47 s), once per tier, in alternating order, after a warm-up.
- **Conditions:** the thermal state read "fair" throughout. Some post-boot
  background work ran at the start and end, so absolute numbers may be a
  touch low.

| Film | Liquid | Flagship avg / 1% low / worst second | Rampant avg / 1% low / worst second |
|---|---|---|---|
| L05 Come and Take Your Medicine | water | 288 / 131 / 201 | 198 / 102 / 123 |
| L11 The Hard Stuff Rules | lava (5D) | 251 / 127 / 161 | 220 / 107 / 112 |
| L14 IIHARL | goo | 239 / 122 / 167 | 213 / 106 / 119 |
| L22 Kill Your Television | water | 263 / 172 / 197 | 276 / 149 / 148 |
| L24 Beware of Abandoned Rental Trucks | sewage (5D) | 243 / 121 / 130 | 236 / 98 / 93 |
| L27 Feel the Noise | lava | 240 / 167 / 148 | 205 / 109 / 107 |
| Net: House of Pain | | 262 / 134 / 194 | 237 / 158 / 164 |

L24 is shown after the full-light fix (`baseline-10`), which is the
released code. Before it, Rampant's worst second there was 35.

**In short:**
- **Flagship:** averages 239–288, 1% low 121–172. It now holds **120 at
  the 1% low on every film**.
- **Rampant:** averages 198–276, 1% low 98–158, worst single second 93.
  The target (60 at the 1% low, reaching for 120) is met everywhere, and
  120 is reached at the 1% low on two of the seven.
- **Rampant's cost against Flagship:** 3–31% of the frame rate on six of
  the films, typically a tenth to a sixth. On L22 Rampant came out *above* Flagship, which is
  run-to-run drift on a warm machine. The guide's "a fifth to a third" is
  now "up to a third, usually much less".
- **Where Rampant's extra GPU time goes, in the slowest 1% of frames:**
  - the world pass, +1.3 to +4.3 ms (traced shadows, reflections);
  - traced ambient shadows, +0.4 to +1.5 ms;
  - the fog, +0.2 to +1.0 ms;
  - the light bake, +0.3 to +0.7 ms.

  The guide's 4–7, 2.5–3.5 and up to 2 are out of date.

  The stage times overlap (frames in flight), so they are for comparing
  with each other.

**Before and after on the same stretches.** These runs were hidden and
off-screen, so the numbers are indicative only; the on-screen bench above
is the official one. Average fps:

| | Before (strict maths) | After (relaxed) |
|---|---|---|
| Rampant, level 6 to tick 1200 | 175 | 311 |
| Flagship, level 6 to tick 1200 | 211 | 358 |
| Rampant, L28 to tick 2100 | 125 | 216 |
| Flagship, L28 to tick 2100 | 155 | 270 |

Light culling, in level 6's opening firefight (avg / 1% low):

| | Before | After |
|---|---|---|
| Enhanced | 340 / 166 | 392 / 180 |
| Flagship | 228 / 116 | 274 / 130 |
| Rampant | 197 / 94 | 211 / 98 |

**Still true, now measured:** "With the old art it is slower, not faster."
Flagship without the HD packs (the 8-bit ramp shading) ran at 223 fps
against 358 with them (level 6, hidden).

**Memory (footprint at the end of each run):** Flagship about 400–665 MB,
Rampant 665–940 MB. Rampant adds about 265–285 MB on every film, so "about
250 MB" of GPU arrays still stands. The illustrated edition's note "About
900 MB, at the top end" still holds (940).

### The illustrated edition's chart (part XII, `#perf`)

The data array in the closing script, in its own shape (name, Flagship
avg, Flagship 1% low, Rampant avg, Rampant 1% low):

```js
var films = [['L05 Come and Take Your Medicine', 288, 131, 198, 102], ['L11 The Hard Stuff Rules', 251, 127, 220, 107], ['L14 IIHARL', 239, 122, 213, 106], ['L22 Kill Your Television', 263, 172, 276, 149], ['L24 Beware of Abandoned Rental Trucks', 243, 121, 236, 98], ['L27 Feel the Noise', 240, 167, 205, 109], ['Net: House of Pain', 262, 134, 237, 158]];
```

Three changes are needed besides the data:
- **Scale:** `fmax = 200` must become 300, or the bars run past the chart.
- **Height:** seven rows need the SVG's `viewBox` taller, from
  `0 0 600 335` to about `0 0 600 380`.
- **Wording:** the title and caption say "six films"; make it seven. The
  60 and 120 dashed lines still mark the target.

## 4. Fixes since the guide that change nothing in its text

- **Skies:** with True Look, the sky turned and leaned with your head when
  you looked up or swayed. It now stays fixed to the world.
- **Above the sky's image:** looking past the top of the sky image used to
  show a mirrored copy (a second, upside-down planet overhead). It now
  fades to the colour of the image's edge. With the HD skies the image
  covers about 45 degrees above and below the horizon. Part XIII could
  list this as a limit: "a sky is a picture, and it ends; look far enough
  up and you see its edge colour".
- **Corpse bar (Rampant):** a dead monster in a dark corner showed a pale
  bar of light, in its own plane, across the floor and walls behind it.
  That was traced ambient shadows mistaking those pixels for the corpse
  itself. Fixed. Figures away from the centre of the view no longer
  darken themselves either.
- **Explosions in crowds (Rampant)** no longer stall (L24 above).

## 5. Checked and still true

- The five tiers and what each adds (part X's table); Custom; Rampant sets
  Redistribution Strength to Strong.
- **Films:** 43 films and 86 checks, run three ways (stock, the default,
  everything in QA on). All passed on 3 October on the Get HD Art build;
  the release since then changes only whether the dialog shows the
  button.
- **Summon BOBs (part XI):** as written. C by default, solo games only,
  and a summon ends the film recording.
- **Part XII's** six switches, their stored names, and what they need.
- **Part XIII's** limits: pitch to about 65 degrees; models do not
  animate; the four nearest bright lights cast figure shadows; dust within
  16 world units; reflections use 256-pixel wall art and flat figures; no
  film export on the Metal display; the OpenGL renderer gets none of this.
- **How it traces:** the hardware ray-tracing comparison and its numbers
  are unchanged.

## 6. Sources

- Bench: `docs/benchmarks/b9-unseen-films.md`; per-frame data in
  `.deps/bench/b9-unseen/` and `.deps/bench/b9-unseen-fixed/`.
- Optimisation: `docs/PLAN-optimisation.md`.
- Get HD Art: `Source_Files/Misc/DurandalFetch.*`, `GetHDArtDialog` in
  `Source_Files/Misc/DurandalPreferences.cpp`, `docs/HD_ASSETS.md`
  section 9.
- Earlier ground truths (Rampant): `docs/RAMPANT-GROUND-TRUTH.md`.
