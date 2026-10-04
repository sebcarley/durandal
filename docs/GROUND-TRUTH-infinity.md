# Ground truths for the guide: Durandal Infinity

Facts from the repository as of 4 October 2026 (tag `baseline-12`), for
writing the Marathon Infinity guide in the form of the Marathon 2 one
(`docs/GUIDE.html`, `docs/GUIDE.md`). Everything here was checked against the
code, the data or a run; where something is approximate or unchecked it says
so. The Marathon 2 guide's facts about the engine (the five tiers, what each
switch does, the art credits, the limits) hold for Infinity unless this file
says otherwise.

## 1. The app

- **Name:** **Durandal Infinity** (`Durandal Infinity.app`, bundle id
  `local.durandal.Infinity`). The owner chose three separate apps, one per
  game, sharing one engine (3 Oct 2026). Marathon 2 stays **Durandal**.
- **Its own folders,** so its settings, saves, films and plugins never mix
  with Durandal's or with a stock Aleph One install of Infinity:
  - settings: `~/Library/Preferences/Durandal Infinity/`
  - saves, films, screenshots, plugins:
    `~/Library/Application Support/Durandal Infinity/` (films in
    `Recordings`, HD packs in `Plugins`)
  - log: `~/Library/Logs/Durandal Infinity Log.txt`
  - shared with Durandal: the texture cache (`~/Library/Caches/Durandal`,
    keyed by file, so the packs both games use are built once) and the
    crash record (`~/Library/Logs/Durandal Crash.txt`).
- **The game data** is Bungie's freely released Marathon Infinity data, from
  Aleph One's repository, bundled inside the app
  (`Contents/Resources/DataFiles`): it plays with a double-click.
- **Minimum macOS 12** (tested on macOS 27 only), Apple Silicon.
- **Not yet released** as a signed download. `scripts/make-release.sh`
  takes `GAME=inf` and makes `Durandal-Infinity-<version>.zip`; the owner
  decides when.
- **Building from source:** `GAME=inf scripts/build.sh`; Xcode scheme
  **Marathon 3**.

## 2. The game in the data

- **57 levels** in the map file: **33 flagged for solo play** (from "Ne Cede
  Malis" to "You Think You're Big Time? You're Gonna Die Big Time!",
  including the three "Electric Sheep" dream levels), the other 24 for net
  play ("Duality", "Thrud", "Wrath No More?", "Morpfhine", "La Cosa
  Nostra", "Fortress Lh'owon" and others). Counted from `Map.sceA`'s level
  info (entry-point flags).
- **Four demo films** ship with the data: LA COSA NOSTRA, MORPHINE, WRATH NO
  MORE and YORRICK (`Demos/`).
- **Upstream's 32 test films** (levels 1 to 33, level 26 missing) replay in
  step through Durandal three ways (stock, default, QA): **66 assertions**
  (two per film, two for set-up). Durandal's changes leave Infinity's
  gameplay, films and saves exactly as upstream plays them.
- **Wall sets:** five, as Marathon 2's numbering (collections 17 water, 18
  lava, 19 sewage, 20 Jjaro, 21 Pfhor), but drawn differently: none of
  Infinity's wall bitmaps is identical to Marathon 2's. The Jjaro set (20)
  is Infinity's own.
- **Liquids:** water, lava, sewage, goo and Jjaro (Infinity's own).

## 3. What works, what differs

Every switch on the DURANDAL dialog works in Infinity as in Marathon 2, in
the same five tiers (Stock, Classic, Enhanced, Flagship, Rampant), with the
first run on Flagship. The differences:

| Feature | In Infinity |
|---|---|
| Widescreen | Works: Infinity has the same built-in classic HUD as Marathon 2 |
| Glow (wall lamps) | Its own list: every Infinity wall set reviewed by eye (lights, screens, switches, lava), the Jjaro set for the first time (3 Oct 2026). 53 lamp entries over collections 17-21 |
| Real Liquids, Reflecting Liquids, caustics | All five liquids, Jjaro included (Jjaro's style was already defined but is still to be seen in play) |
| Volumetric Fog | The haze per environment as Marathon 2's table (water, lava, sewage, Jjaro, Pfhor) |
| HD art | Its own pack set (section 4) |
| Summon BOBs (Cheats) | Summons five armed security BOBs, as in Marathon 2 (checked 4 Oct 2026: Infinity's monster 14 is the BOB with the pistol) |

Nothing else needed changing: the renderer, Marathon's shading, the lights
and shadows, relief, ambient shadows, the camera (True Look, Free Look,
Sidestep Sway), reverb, the texture cache and the five Rampant features read
only what all three games share.

## 4. HD art: Get HD Art in Infinity

Preferences > DURANDAL > **ART** > **GET HD ART...** works as in Marathon 2,
with Infinity's own list. The dialog opens with "The community's HD art for
Marathon Infinity: walls and sky, monsters, scenery, weapons and 3D models,
from its authors' own pages on Simplici7y." The packs go in
`Application Support/Durandal Infinity/Plugins` (the ART tab and the dialog
say so).

| Line in the dialog | What it is | Download |
|---|---|---|
| CFP - Walls MInf | Community/Freeverse Walls MInf 1.1 (herecomethej2000): Infinity's walls at 1024x1024 (Goran Svensson's sets with TheMan's and kaosof's work), the 4K landscapes, normal maps on all five wall sets (153; version 1.1 added the Pfhor and Jjaro ones) | about 425 MB |
| CFP Monsters | the same pack as Marathon 2's (it covers Infinity, VacBobs included) | about 516 MB |
| CFP Scenery | the same pack as Marathon 2's | about 24 MB |
| CFP Weapons MInf | Community/Freeverse Plugin - Weapons Minf 2.4 (herecomethej2000): Infinity's weapons and items | about 73 MB |
| 3D Items | thedoctor45's 3D pickups (made for Infinity) | about 3 MB |
| **All five** | | **about 1.0 GB** |

- Fetched from the authors' own Simplici7y pages (the two MInf packs are on
  Google Drive). Nothing is re-hosted. The versions tested have their
  checksums recorded; a newer one installs all the same.
- **Checked on screen** in Rampant on the four demo films: walls,
  landscapes, weapons and monsters replaced, wall lamps glowing.
- Level entry builds the texture cache on the first visit to each
  environment (frames of about 1.3-1.9 s once), as Marathon 2 does.
- `GAME=inf scripts/get-hd-art.sh` does the same from Terminal.

## 5. Benchmarks

See section 5 of `docs/GROUND-TRUTH-marathon.md` for the conditions; the
Infinity rows are in the table there and in
`docs/benchmarks/b12-inf-rampant.md` and `b12-inf-flagship.md`.

## 6. Not yet done or checked

- Infinity's Jjaro liquid and environment 3 (Jjaro) haze seen in play;
  reverb under Jjaro liquid.
- The dream levels and 5D space played through.
- The largest levels against the light redistribution's atlas.
- Benchmark films recorded by the owner (the four demos stand in).
- A signed release.

## 7. Corrections to carry over from the Marathon 2 guide's slips

The Marathon 2 guide's recurring slips (see the placing notes in `CLAUDE.md`)
apply here too: Get HD Art fetches the packs, it does not ship them; Close
cancels a fetch; installed packs stay on the Mac; "Weapon Takes the Light"
is a switch (Light tab).
