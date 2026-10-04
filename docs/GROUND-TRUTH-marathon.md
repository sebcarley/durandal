# Ground truths for the guide: Durandal Marathon

Facts from the repository as of 4 October 2026 (tag `baseline-12`), for
writing the Marathon (1) guide in the form of the Marathon 2 one
(`docs/GUIDE.html`, `docs/GUIDE.md`). Everything here was checked against the
code, the data or a run; where something is approximate or unchecked it says
so. The Marathon 2 guide's facts about the engine (the five tiers, what each
switch does, the limits) hold for Marathon unless this file says otherwise.
Marathon differs from Marathon 2 and Infinity far more than they differ from
each other, so this file is longer.

## 1. The app

- **Name:** **Durandal Marathon** (`Durandal Marathon.app`, bundle id
  `local.durandal.Marathon`). One app per game, sharing one engine (the
  owner's choice, 3 Oct 2026). The plan's lore alternative, "Leela", was not
  taken.
- **Its own folders:**
  - settings: `~/Library/Preferences/Durandal Marathon/`
  - saves, films, screenshots, plugins:
    `~/Library/Application Support/Durandal Marathon/` (films in
    `Recordings`, HD packs in `Plugins`)
  - log: `~/Library/Logs/Durandal Marathon Log.txt`
  - shared with Durandal: the texture cache (`~/Library/Caches/Durandal`)
    and the crash record (`~/Library/Logs/Durandal Crash.txt`).
- **The game data** is Bungie's freely released Marathon data (Aleph One's
  repository), in its original format, which Aleph One converts on load;
  bundled inside the app. Its music plays from the bundled `Music` folder.
- **Minimum macOS 12** (tested on macOS 27 only), Apple Silicon.
- **Released** signed and notarised in v0.2.0 (4 Oct 2026),
  `Durandal-Marathon-0.2.0.zip` (32 MB), with Durandal and Durandal
  Infinity (`GAME=m1 scripts/make-release.sh <version>`).
- **Building from source:** `GAME=m1 scripts/build.sh`; Xcode scheme
  **Marathon 1**.

## 2. The game in the data

- **37 levels** in the map file. The campaign runs from **Arrival** to
  **Ingue Ferroque**: 27 levels, one upstream test film each (the map
  file's own solo flags are looser and also mark some arena maps). The
  rest are net maps ("Mars Needs Women", "Carnage Palace Deeee-Luxe",
  "5-D space", "Spiral Insanity" and others).
- **No demo films** ship with Marathon's data.
- **Upstream's 27 test films** (levels 1 to 27) replay in step through
  Durandal three ways: **56 assertions**. Marathon's films go through
  Aleph One's conversion and its own film profile; Durandal's changes leave
  them, and Marathon's gameplay and saves, exactly as upstream plays them.
- **Numbering:** Marathon's shapes collections mean other things than
  Marathon 2's. Its walls are collections 2, 8, 17, 18, 19 and 24; its
  monsters 3, 5, 6, 9, 12, 14, 15, 16, 21 (Juggernaut), 22, 26, 27 (Wasp)
  and 29 (Cyborg); it has no scenery-type collection (its scenery, 23 and
  25, is held as objects). `Misc/DurandalScenario.*` knows this; before it,
  Durandal would have taken Marathon's walls for monsters.
- **No liquids at all:** Aleph One's loader sets every polygon's liquid to
  none.
- **The sky** is a starfield tiled in both directions (8 x 8), so looking up
  with True Look shows more stars: Marathon 2's sky-wrap problem does not
  arise.
- **The HUD** is a Lua script in every case (Marathon has no built-in
  classic HUD). Upstream ships three: the framed original ("Default HUD"),
  the same without the frame ("Basic M1 HUD", off by default) and the
  Xbox-style overlay ("Enhanced HUD").
- **Lighting:** Marathon builds some areas with a light level of exactly 0
  (seen only by the player's own close light), and lights long corridors
  evenly; both mattered for the light redistribution (section 3).
- **Monsters drawn self-lit:** Marathon gives its monsters' body frames a
  minimum light of 0.2 to 1.0 (Hunters, Fighters, Compilers, Wasps), where
  Marathon 2 keeps that for shots and flashes.
- **BOBs are unarmed:** monster types 6 to 9 (the BOBs) have no ranged
  attack; type 14 is a S'pht.

## 3. What works, what differs

The same five tiers, first run on Flagship. Differences from Marathon 2:

| Feature | In Marathon |
|---|---|
| **HUD (new, Look tab, Marathon only)** | Classic (the framed original, upstream's default), Basic (no frame: the view fills the screen) or Enhanced (the overlay; it has its own thin black margin and a 90-degree field of view). Exactly one of the three HUD plugins runs. Applies from the next level or new game. Stored as `hud_style`; Stock leaves the plugins as upstream has them. QA passed 4 Oct 2026 |
| Widescreen (Feel tab) | No effect: Durandal's widescreen works only with the built-in classic HUD. With HUD Basic or Enhanced the view fills a 16:9 (or any) screen anyway |
| Real Liquids, Reflecting Liquids, caustics, ripples, embers, Heat Shimmer | Inert: there are no liquids or lava. The tiers stay as they are; the switches simply have nothing to do |
| Dynamic Lights | Projectiles, explosions, effects and the player's muzzle flash light the room; **monsters do not** (Marathon's Hunters are self-lit on every frame and each became a green lamp) |
| Glow | Wall lamps from its own list (35 entries over its six wall sets, reviewed by eye: lights, computer screens, alien panels, the plasma tube and lava walls). Monsters glow only on frames near full light (shots, flashes), not their half-lit bodies |
| Light Redistribution | May darken a surface only half as far as in the other games (corridors lit evenly made one long group whose brighter end set the average). In all three games since 3-4 Oct: unlit rooms no longer blotch (the ratio has a floor), lines Marathon draws as walls stop light, uncovered surfaces round moving doors start clean |
| Volumetric Fog | Ship air everywhere: Marathon's environment codes name other places, and with no liquids there is no lava smoke or sewage air |
| Character Shadows | Every Marathon monster and its scenery cast them (the per-game collection list) |
| All Weapons and Ammo (Cheats) | Gives Marathon's own seven weapons and their ammunition: never Marathon 2's shotgun or Infinity's SMG, which the engine also knows (a bogus "fist that fires rockets" on key 4 until 4 Oct 2026) |
| Summon BOBs (Cheats) | Left out: the key prints "Summon BOBs: Marathon's BOBs carry no weapons" and the film keeps recording |
| HD art | Its own pack set (section 4) |
| Spinning Pickups, 3D Pickups | The 3D Pickups switch also switches the 3D scenery pack (the only model pack Marathon has) |

## 4. HD art: Get HD Art in Marathon

The ART tab's GET HD ART... works as in Marathon 2, with Marathon's own
list. The dialog opens "The community's HD art for Marathon: ..." and the
packs go in `Application Support/Durandal Marathon/Plugins`.

| Line in the dialog | What it is | Download |
|---|---|---|
| TTEP 1024 | Tim Vogel's Total Texture Enhancement Pack v7, at 1024x1024 (Zetren's plugin, made for the original Marathon data): all six wall sets and the starfield | about 60 MB |
| Updated Starscape | Hopper's 2048x1080 starfield (a nebula pattern from webtreats), from Aleph One's own release; loads after TTEP, whose sky entries it overrides | under 1 MB |
| Texture Renewal Monsters | Rock's Marathon Texture Renewal Project: Monsters Module v0.5 (2024): 4x upscales, the BOBs reworked by hand | about 44 MB |
| M1 Weapons Redux | General Tacticus's HD weapons in hand, projectiles, items and impacts | about 14 MB |
| 3D Scenery M1 | General Tacticus's 3D scenery models (13 models) | about 4 MB |
| **All five** | | **about 123 MB** |

- **Monsters were chosen by the owner** after a side-by-side in Rampant of
  Texture Renewal against xBR Monsters for M1 (Flippant Sol, 2017): Texture
  Renewal (3 Oct 2026).
- Fetched from the authors' Simplici7y pages (plain downloads, no Google
  Drive) and Aleph One's GitHub release. Rock's pack is a 7z archive, which
  the game opens with macOS's own tools. Nothing is re-hosted; no pack
  states a licence beyond the community's custom (the same position as
  Marathon 2's packs).
- The full catalogue of Marathon's HD packs, with the alternatives and why
  they were not chosen: `docs/HD_ASSETS.md` section 10.
- Checked: the in-game fetch installed all five into a scratch folder with
  matching checksums; on screen in Rampant all five apply.

## 5. Benchmarks (4 Oct 2026)

Indicative only: full screen on the MSI at 1080p 240 Hz, uncapped, sound
off, the first two minutes of each film, **while the owner's other apps
were busy** (one at about 100% of a core); not a settled baseline.

HD art installed in both games (each game's five packs). Average fps, the
1% low (p99 frame time) and the worst second:

| Game | Film | Rampant avg / 1% low / worst s | Flagship avg / 1% low / worst s |
|---|---|---|---|
| Marathon | L1 Arrival (test film) | 247 / 130 / 196 | 287 / 133 / 174 |
| Marathon | L8 Cool Fusion (test film) | 253 / 129 / 162 | 279 / 133 / 181 |
| Marathon | L16 Neither High Nor Low (test film) | 241 / 128 / 180 | 268 / 130 / 227 |
| Marathon | the owner's lift film (20261003-01) | 348 / 138 / 232 | 338 / 150 / 223 |
| Infinity | LA COSA NOSTRA (demo) | 230 / 116 / 143 | 268 / 134 / 187 |
| Infinity | MORPHINE (demo) | 219 / 116 / 157 | 267 / 132 / 169 |
| Infinity | WRATH NO MORE (demo) | 241 / 122 / 171 | 278 / 134 / 194 |
| Infinity | YORRICK (demo) | 233 / 113 / 181 | 271 / 131 / 169 |

- **In short:** both games hold well over the 120 fps target on average in
  Rampant (219-348) and stay above 110 at the 1% low; Flagship adds about
  10-20%. The worst second never fell below 143 frames.
- **Memory at the end of a run:** about 0.4-1.0 GB resident with the HD
  art; Rampant's footprint is 0.8-1.0 GB.
- The per-film summaries (GPU and CPU time per stage, presentation
  intervals) are in `docs/benchmarks/b12-inf-rampant.md`,
  `b12-inf-flagship.md`, `b12-m1-rampant.md` and `b12-m1-flagship.md`.
- For comparison, Marathon 2 on the rebench of 3 Oct 2026 (a quiet Mac):
  Rampant 198-276 average, Flagship 235-288 (`docs/benchmarks/b9-unseen-films.md`).

## 6. Not yet done or checked

- Benchmark films recorded by the owner for Marathon (one so far, the lift:
  `tests/benchmark-films/m1/20261003-01.filA`).
- Marathon's own music packs (a pack supplying only `Music/NN.ogg` files is
  not recognised by the Soundtrack switch; the bundled music plays).
- A separate switch class for 3D scenery.

## 7. Notes for the guide

- The Marathon 2 guide's recurring slips apply here too (Get HD Art fetches,
  never ships; Close cancels; installed packs stay on the Mac; Weapon Takes
  the Light is a switch).
- Features that do nothing in Marathon (the liquids, Heat Shimmer, embers,
  Widescreen, Summon BOBs) should be said plainly rather than left out of
  the switch list, so a reader does not hunt for them.
