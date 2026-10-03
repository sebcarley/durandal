# Plan: the trilogy — Marathon and Marathon Infinity

Written 3 Oct 2026, at `baseline-11` / release 0.1.0. The ask: "do the same
with Marathon 1 and Infinity, just to complete the trilogy." Branch:
`durandal/trilogy`. This is an outline: scope, order, decisions and risks.
Each round gets its own detailed plan when it starts.

## In short

- **The engine already plays all three.** Aleph One reads Marathon 1's old
  formats and converts them on load, and the repository already holds all
  three games' data (submodules under `data/Scenarios`). It also holds
  upstream's test films for each: 27 for Marathon 1, 42 for Marathon 2 and
  32 for Infinity. The Xcode project has a target per game ("Marathon 1",
  "Marathon 2", "Marathon 3"), and all three compile Durandal's code. A
  Marathon 1 or Infinity build already gets the Metal display and the
  Flagship tier on its first run. Nobody has looked at one yet.
- **What is Marathon 2-specific is content, not machinery.** That means:
  - tables keyed to M2's collection numbers and environments;
  - the hand-reviewed list of glowing wall lamps;
  - the HD art packs;
  - the app's name and folders;
  - the film gate, which replays only M2;
  - the tuning done by eye on M2 levels.

  A sweep of the code found these and nothing keyed to M2 level numbers or
  names (inventory below).
- **Infinity first, then Marathon 1.**
  - Infinity is M2's own format and numbering, with more in it: the Jjaro
    wall set, Jjaro liquid, VacBobs, and an environment M2 never uses. The
    community's HD art (the Community/Freeverse packs) covers it.
  - Marathon 1 differs more:
    - **No liquids at all:** the loader sets every polygon's liquid to none,
      so all the water work is inert there.
    - **Different numbering:** collections and environment codes mean
      different things.
    - **No built-in classic HUD:** only a Lua HUD.
    - **Skies:** they repeat vertically.
    - **HD art:** thinner, and from different authors.
- **The gate grows to 101 films**, each replayed three ways, for each game.
  Upstream's own CI already replays all three games, so the harness needs
  no change, only the script around it.

## Decisions for the owner

Decided 3 Oct 2026: **three apps** (decision 1) named **Durandal Marathon**
and **Durandal Infinity** (decision 2); Marathon 2 stays **Durandal**.
Decisions 3 to 5 are still open.

1. **Three apps or one.**
   - **Recommended: three apps sharing one engine**, as Aleph One ships.
     Each game keeps its own saves, films, settings and tier.
   - **One app with a game chooser** is possible, but it touches upstream's
     start-up and folder logic, which every save and film path depends on.
2. **Names.**
   - **Why the names matter:** an app's display name sets its settings and
     Application Support folders. Stock Aleph One's "Marathon" and
     "Marathon Infinity" would share folders with a stock install of the
     same game, so they cannot be reused.
   - **Recommended:** **Durandal Marathon** and **Durandal Infinity**,
     with Marathon 2 staying **Durandal** (renaming it would move existing
     players' saves).
   - **The lore alternative:** **Leela** for Marathon 1 (she is the AI
     who guides you there).
3. **Releases:** one release per version carrying three zips (about 35 MB
   each), signed and notarised as 0.1.0 was. The field guide site gains a
   page per game.
4. **Marathon 1's HD art** (catalogue it first; pick after a look). On
   Simplici7y today:
   - the **Marathon Texture Renewal Project** (Rock, 0.5, Aug 2024: 4x
     upscales, in Surfaces, Monsters and Weapons modules);
   - **M1A1 Animated Textures** (President People, Feb 2025);
   - older packs: TTEP v7, xBR Monsters, M1 Weapons Redux, and **3D scenery
     for M1**, the only scenery models in the series.

   Same rule as M2: fetched from the authors by the game's GET HD ART...
   button, never shipped.
5. **The guides.** The M2 guide is "Durandal, by Durandal". A guide per game
   in the same form would suit the series:
   - **Marathon 1:** narrated by Leela.
   - **Infinity:** contested, which the M2 guide's Tycho epilogue already
     sets up.

   For Claude chat to write, from ground truths as before.

## Rounds

Sizes are relative: **S** is about a session, **M** two or three, **L**
four or more, each plus the owner's QA.

### T0 — Baseline (S)

- **Build and name the two other targets:**
  - local signing, as the M2 target already has;
  - the decided names, bundle ids and minimum macOS;
  - their Info.plists, which hard-code "Classic Marathon" and macOS 10.13.
- **Scripts that take a game:**
  - `build.sh`, `test-films.sh`, `soak.sh`, `make-release.sh` (M1 has
    different data file names);
  - `setup.sh`, which fetches only the M2 submodule today;
  - the benchmark scripts.
- **The film gate:** replay the 27 M1 and 32 Infinity films three ways
  (stock, default, QA). Upstream passes them, so any failure is ours and
  comes first.
- **First look on screen:** the owner plays each game at Flagship and at
  Rampant. I take frame shots of the upstream demo films. Together they
  give the list of what is wrong.

### T0 results (3 Oct 2026)

- **Built and named:** Durandal Infinity.app and Durandal Marathon.app,
  signed to run locally; scripts take `GAME` (`scripts/game.sh`).
- **Film gate:** green first time, three ways, for all three games
  (Marathon 2 86 assertions, Infinity 66, Marathon 56).
- **Get HD Art** shows only in Marathon 2 until each game has its list.
- **`DURANDAL_DISPLAY=builtin`** puts on-screen runs on the MacBook's own
  screen (the owner's ask: not the MSI).
- **First look, frame shots** (`.deps/looks/t0-first-look`: Infinity's four
  demos and Marathon's L1, L8 and L16, first 90 s each, stock against
  Rampant, full screen on the laptop at 2940x1846):
  - **Infinity:** nothing wrong seen. Widescreen, fog, ambient shadows,
    lamps, skies, sewage and water liquids all behave as in Marathon 2.
  - **Marathon, the HUD:** its Lua HUD draws correctly on the Metal
    display; the view stays 4:3 with black bars either side (expected: no
    widescreen with a Lua HUD).
  - **Marathon, too dark:** in Rampant, walls that are dim in stock fall
    to near black, with blotchy dark patches in dim rooms and corridors
    (L1 tick 2401, L8 throughout, L16 tick 1501). Not seen in Infinity.
    Suspects: the haze table (Marathon's environment codes pick Marathon
    2's lava/sewage/Jjaro haze), light redistribution or bounced light
    with Marathon's palettes, traced ambient shadows. To isolate by
    switching features one at a time.
  - **Marathon, green flood:** in Rampant, the rooms around Hunters on L16
    wash green (ticks 601, 2401) where stock shows grey: sewage-green air
    and/or the Hunters' self-lit armour as strong dynamic lights. Same
    isolation.
  - **Marathon, hitches:** 13 frames of the L1 run took ~350 ms to render
    (longest 646 ms), at 30 fps capped; Infinity showed none. To look at
    with the cache log (texture builds on first sight?).
- **Still to do:** the owner's own play of each game at Flagship and
  Rampant (on the laptop screen).

### T1 — Infinity (M)

- **Which game is this:** a small `DurandalScenario` module (from the
  scenario name the data declares), so tables can be per game instead of
  assumed.
- **Lamps that glow:**
  - Infinity's wall collections 17–21 differ byte for byte from M2's, so
    every wall-lamp fingerprint is re-checked (`DURANDAL_DUMP_BITMAPS`);
  - the Jjaro set (20) is reviewed for the first time.

  Light redistribution's lamp boost uses the same list.
- **Never seen before:** the Jjaro liquid's style and murk, environment 3's
  haze, and reverb in Jjaro liquid.
- **New collections:** the VacBob (13) and Jjaro scenery (25) join the HD
  art classing and the character shadows' list of figures.
- **HD art:**
  - **The set:** CFP Walls MInf (about 425 MB) and CFP Weapons MInf (about
    73 MB), with the CFP Monsters and Scenery packs that already cover
    Infinity, plus 3D Items, which was made for Infinity.
  - **Get HD Art takes a pack list per game.** Its text names the game and
    its own Plugins folder.
  - **The catalogue** gets an Infinity section.
- **Checks:**
  - Summon BOBs' monster type in Infinity's physics;
  - the dream levels and 5D space;
  - the largest levels against the radiance atlas and the level grid.
- **Close:**
  - the owner records three benchmark films and I bench them;
  - film tests three ways;
  - the owner's QA, then a tag and an Infinity zip.

### T2 — Marathon 1 (L)

- **Numbering:** M1's collections mean different things.
  - Walls are 2, 8, 17, 18, 19 and 24.
  - The Juggernaut is 21, the Wasp 27 and the Alien Leader 29.
  - 10 is a second interface collection.

  Today M1's walls would be classed as monsters, and three monsters would
  cast no character shadow. HD art classing and the figure list go per
  game.
- **Air:** M1's environment codes (1, 2, 3, 5) pick haze meant for lava,
  sewage and Jjaro arbitrarily. The level you arrive on, Arrival, would get
  sewage-green air. M1 needs its own haze table, set by eye per environment.
- **Lamps:** a glow list for M1's walls, reviewed as M2's was.
- **The HUD:**
  - **Unchecked on the Metal display:** M1 has only Lua HUDs.
  - **No widescreen:** Durandal's widescreen code only works without a Lua
    HUD, and M1's default HUD frames the view at 4:3. M1 needs a
    widescreen view that works with a Lua HUD.
  - **The crosshair:** Free Look's crosshair needs checking with M1's HUD.
- **Skies:** M1's landscapes repeat vertically, so the sky wrapping that
  True Look exposed in M2 would come back. The fix needs extending to
  repeating skies.
- **No liquids:**
  - Inert in M1: Real Liquids, Reflecting Liquids, ripples, caustics,
    embers and heat shimmer.
  - The tiers stay as they are; the guide says so.
- **Summon BOBs:** M1's index 14 is a hostile S'pht. The cheat must take
  M1's Bob Security (8), or be left out of M1 if M1's BOBs are unarmed (to
  check in the physics).
- **Music:** M1 plays a track per level from `Music/NN.ogg`. The soundtrack
  switch does not recognise a pack that only supplies those files.
- **Shading:** the ramp-finding rules were tuned on M2's palettes, so M1's
  are checked in the shade-table dump.
- **HD art:**
  - catalogue the M1 packs and their licences;
  - choose a set with the owner (decision 4);
  - give Get HD Art its M1 list.
- **Close:** as for Infinity (benchmark films, film tests, QA, tag, zip).

### T3 — The trilogy together (S–M)

- One release carrying the three notarised apps.
- `setup.sh` builds all three.
- The README and the Pages site choose a game.
- Ground truths for the three guides.
- Then a `baseline` tag for the trilogy.

## What carries over unchanged

These need nothing beyond a look, because they read only what all three
games share:
- the polygon map, light intensities, sides and objects;
- the renderer;
- Marathon's own shading (subject to the palette check);
- texel lighting, crisp filtering, smooth edges, crisp text and terminals;
- dynamic lights and their shadows, contact shadows, bloom and HDR output;
- relief, ambient shadows (screen-space and traced), distance shade, the
  scene grade;
- the camera (True Look, Free Look, Sidestep Sway, per-frame look);
- reverb, apart from its liquid part in M1;
- the texture cache;
- bounced light, traced shadows, dust;
- Weapon Takes the Light;
- God Mode, Noclip, All Weapons (already allows for M1's weapon limits);
- Level Select.

Marathon 1's exploration missions use their own visibility path, which
True Look already leaves alone.

## Inventory: what assumes Marathon 2

From the sweep. File references are as of `baseline-11`.

| Area | Where | Infinity | Marathon 1 |
|---|---|---|---|
| HD art classing by collection | `DurandalArt.cpp:43-62` | same numbering; add 13, 20, 25 | wrong: walls read as monsters and scenery |
| Character shadows' figure list | `RenderRasterize_Metal.cpp:1178` | add 13, 25 | 21, 27, 29 cast none |
| Wall lamps (glow, redistribution boost) | `DurandalGlow.cpp:35-60` | re-fingerprint 17–21, review 20 | new list for 2, 8, 17–19, 24 |
| Haze by environment | `RenderRasterize_Metal.cpp:95-105` | Jjaro (3) unseen | codes mean other things |
| Liquid styles and murk | `RenderRasterize_Metal.cpp:78-126` | Jjaro unseen | none (no liquids) |
| Summon BOBs monster | `DurandalCheats.cpp:65` | to check | would summon S'pht |
| HD pack list, dialog text | `DurandalFetch.mm:91-102`, `DurandalPreferences.cpp:485-508, 779`, `get-hd-art.sh` | CFP MInf set | M1 packs |
| Soundtrack detection | `DurandalArt.cpp:108-135` | fine | misses `Music/NN.ogg` packs |
| Sky fade beyond the image | `RenderRasterize_Metal.cpp:757-758` | as M2 | skies repeat vertically |
| Widescreen | `screen.cpp:376-399`, `ViewControl.cpp:195-202` | fine (built-in HUD) | never applies (Lua HUD) |
| Shade-ramp rules | `DurandalShading.cpp:31-89` | to check | to check |
| App name, folders, signing | Xcode targets "Marathon 1" / "Marathon 3", `App_Resources/Marathon{1,3}` | upstream's names and team | upstream's names and team |
| Scripts | `build.sh`, `test-films.sh`, `soak.sh`, `make-release.sh`, `setup.sh`, the benchmark scripts | M2 only | M2 only, and M1's file names differ |
| Crash log, texture cache folder | `DurandalCrash.cpp:202`, `DurandalTextureCache.mm:117` | shared "Durandal" paths: fine (cache keyed by file) | as Infinity |

## Risks

- **Determinism in M1:** M1 films replay through upstream's own conversion
  and film profile, and Durandal's upstream edits have never been run
  against them. T0's film run settles it before anything else.
- **M1's Lua HUD on the Metal display** is unverified, and widescreen for it
  is new work, not a port.
- **Thinner HD art for M1:** 4x upscales at version 0.5, not hand-redrawn
  sets. M1 may look less transformed than M2 and Infinity, and should be
  judged on the display before it is promised.
- **Folder collisions:** without new names, the apps would share settings
  and saves with a stock Aleph One install of the same game.
- **Tuning by eye:** fog, lamps and liquids were judged on M2 levels. Each
  game needs its own looks on the owner's display, which is the long pole of
  each round.
