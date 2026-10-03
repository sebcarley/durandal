# Durandal — Marathon 2 on a native Metal engine for Apple Silicon

A personal project. Aleph One (GPL 3, the continuation of Bungie's Marathon 2
engine) turned into a native Metal engine for Apple Silicon that improves how
Marathon 2: Durandal looks, sounds and feels while keeping the game intact.
Target: 120 fps or better on an M5 at 1080p in the flagship tier, measured on
settled hardware, with real interpolated frames (not repeats).

The owner directs, reviews and tests, and works through Xcode (Run) and chat
only. Claude does all command-line work (git, deps, builds); never ask the
owner to use Terminal. Give numbered click-by-click steps for Xcode or
Finder and say exactly what to paste back. Warn before any step where an
unforced error is likely. British English throughout; direct, peer-level.

## Standing rules

- Only legitimately released code and data: Aleph One (GPL) and Bungie's
  freely released Marathon data. Never leaked or decompiled proprietary code.
  MetalQuake (the owner's earlier project) techniques may be reimplemented; its code
  is not shared.
- **Git:** work on branches, tag every safe point (`baseline-0`, then
  `baseline-N` / descriptive tags), small described commits. Never merge to
  `master` — the owner merges after testing. Never push. This project brief
  explicitly overrides the global "never branch/tag" rule; the global
  "never push" rule still stands.
- Every enhancement is switchable, with a way back to the stock look. Nothing
  may break save games, films (replays) or the original gameplay rules.
- New user-visible behaviour ships behind a feature flag (DEBUG on, Release
  off) until manual QA has passed; then it is released to every launch
  (see Feature gate).
- Don't overwrite the owner's files or anything outside this folder without asking.
- Ask before installing anything system-wide (Homebrew, global tools). Prefer
  what vcpkg fetches itself. Don't add dependencies without asking.
- EDR/HDR results are judged on the display, not in screenshots.
- Don't launch the game (or anything that opens a window/grabs the mouse) on
  the owner's display without asking in chat first in the same sitting.
  Since 26 Sep 2026 every run (benchmarks, look checks, A/B tests) goes
  full screen on the main display so the owner can watch the work; no
  hidden or headless runs while the owner is at the machine unless told
  otherwise.
  The film tests are headless by nature: ask before running them.

## Determinism rules (films, saves, net play)

The film test (below) is the gate. To keep it green:
- Never call `global_random()` / `set_random_seed()` from render, audio or
  enhancement code. Rendering randomness uses its own `GM_Random` instance.
  Don't use `local_random()` either (it feeds sound permutations and Lua).
- Never write to game-world arrays except via the existing interpolation
  save/restore pattern (`GameWorld/interpolated_world.cpp`).
- Never change physics, platform, media, monster, weapon or item definitions,
  dynamic limits, action-flag encoding, or tick order in
  `update_world_elements_one_tick()`.
- Never ship gameplay-affecting MML sections (platforms, liquids, weapons,
  items, monsters, scenery, player, dynamic_limits, map_patch).
- Light *functions* use `global_random()`; renderers only ever read
  `light->intensity`, never evaluate light functions themselves.

## Layout

- Repo root = the Aleph One clone (`origin` = upstream GitHub; upstream
  default branch is `master`).
- `data/Scenarios/Marathon 2/` — M2 data (git submodule `data-marathon-2`).
  Bundled into the app by a build phase (`Contents/Resources/DataFiles`), so
  the game finds its data automatically.
- `Xcode/AlephOne.xcodeproj` — the project (the old `PBProjects/` is gone).
  Target and scheme **Marathon 2** builds `Durandal.app` (renamed in Round
  1; bundle id `local.durandal.Durandal`). Since 3 Oct 2026 (trilogy T0)
  **Marathon 3** builds `Durandal Infinity.app` (`local.durandal.Infinity`)
  and **Marathon 1** builds `Durandal Marathon.app`
  (`local.durandal.Marathon`), signed to run locally like Marathon 2, their
  shared schemes copied from Marathon 2's (Release, `DURANDAL_QA=1`). Their
  data comes from `data/Scenarios/Marathon Infinity` and
  `data/Scenarios/Marathon` (submodules); their folders are named
  "Durandal Infinity" and "Durandal Marathon" (prefs, Application Support,
  Plugins). The Steam targets are upstream's, untouched.
- `.deps/` (git-ignored) — private vcpkg (`.deps/vcpkg`), CLI DerivedData,
  build/test logs. vcpkg fetched its own CMake; nothing system-wide.
- `vcpkg/installed-arm64-osx/` (git-ignored) — installed dependencies; the
  Xcode project's search paths point here.
- `scripts/` — our helper scripts (`xcode-add-files.py` registers new
  source files in both engine libraries). `docs/AUDIT.md`,
  `docs/ROADMAP.md`, `docs/HD_ASSETS.md` — ours.
  Everything else under `docs/` is upstream.
- `Assets/` (git-ignored) — the downloaded hi-res art packs and their
  checksums (`docs/HD_ASSETS.md` catalogues them). None of it may go in
  git (Bungie's community-use notice). The game loads packs as plugins
  from `~/Library/Application Support/Durandal/Plugins`, where the four
  CFP packs are symlinked from `Assets/cfp/` (27 Sep 2026).

## Build and test

- Dependencies (once, ~8 min): `scripts/install-deps.sh`
- CLI build: `scripts/build.sh [Release|Debug] [tests]` — uses
  `.deps/DerivedData`, so it never collides with Xcode's Run.
- Which game: every script reads `GAME` (`m2` default, `inf`, `m1`; build
  and film tests also take `all`) through `scripts/game.sh`, which gives
  the scheme, app name, data folder, test films and a default film
  (Infinity's demo "LA COSA NOSTRA"; Marathon 1 has no demos, so its first
  test film, L1 Arrival).
- Film determinism test (run before finishing any session):
  `scripts/test-films.sh` — replays the 42 Marathon 2 films under
  `tests/replays/Marathon 2` headless and checks each final RNG seed. Must
  report `All tests passed (86 assertions in 1 test case)` (two per film, two
  for set-up; "43 films" in older notes and the guide was a miscount).
  `GAME=inf` replays Infinity's 32 films, `GAME=m1` Marathon's 27,
  `GAME=all` the three in turn (one tests app serves all three games).
- Xcode: scheme **Marathon 2**, destination **My Mac**. Run uses **Release**
  (set in the shared scheme). Other schemes are hidden by the user's own
  `xcschememanagement.plist` under `xcuserdata` (local, git-ignored).

### Project changes made for this Mac (Phase 0)

- `MACOSX_DEPLOYMENT_TARGET` raised from 10.13/11.0 to 12.0 (Xcode 27's
  floor).
- Marathon 2 and Aleph One Tests targets: signing switched from Aleph One's
  team (27XZ82KJ77) to "Sign to Run Locally" (`CODE_SIGN_IDENTITY = "-"`,
  manual style, no team).
- Marathon 2 scheme: Run action uses Release.

## Where things live on disk at runtime

The folder name comes from the *localised* bundle name
(`Xcode/App_Resources/Marathon2/en.lproj/InfoPlist.strings`, now
"Durandal").
- Prefs: `~/Library/Preferences/Durandal/Marathon 2 Preferences` (our
  settings are the `<durandal>` element)
- Saves, films, screenshots: `~/Library/Application Support/Durandal/`
- Log: `~/Library/Logs/Aleph One Log.txt`
- Film tests: `~/Library/Preferences/org.bungie.source.AlephOneTests`,
  `~/Library/Application Support/AlephOne`
- Before Round 1 the app used `.../Marathon 2/` folders; anything there is
  from Phase 0 runs.
- Never launch the app headless with `-g`, `-w` or `-f`: upstream writes
  command-line overrides into the preferences at startup (the benchmark is
  exempt — it never writes preferences).

## Feature gate and settings

- `Source_Files/Misc/DurandalPreferences.*`. Since baseline-7 (29 Sep
  2026) everything that has passed QA runs however the app is launched
  (`Durandal::Available()`, closed only by `DURANDAL_STOCK=1`), so a
  built app plays as the owner plays. A feature still in QA
  (`Durandal::Released()` false: none since 2 Oct 2026) runs only
  in Debug builds or with `DURANDAL_QA=1` (`Durandal::QA()`; the shared
  scheme's Run action sets it), and has no switch in the dialog
  otherwise. When the owner passes a feature, take it out of
  `Released()`'s list. A feature is live only when its gate is open and
  its switch is on.
- Preferences → DURANDAL: Quality (Stock / Classic / Enhanced / Flagship / Rampant / Custom) and one switch
  per feature. The first run applies Flagship (fps target 0 = display
  rate). Stock restores upstream exactly (30 fps, no features).
- Add a feature: extend `Durandal::Feature`, `kFeatureAttr`, the dialog
  labels, `FeatureTier`, `FeatureTab` and `Released`.
- Run the film tests three ways: `DURANDAL_STOCK=1 scripts/test-films.sh`,
  `scripts/test-films.sh` and `DURANDAL_QA=1 scripts/test-films.sh`.

## Benchmark, parity check and hidden runs

- **Never put anything but dash flags on the app's command line.** macOS
  treats other command-line words (paths, numbers) as documents to open and
  shows an error alert, which blocks the app and appears on screen.
  All benchmark settings therefore come from environment variables:
  `DURANDAL_BENCHMARK` (CSV path), `DURANDAL_BENCHMARK_FILM`,
  `DURANDAL_BENCHMARK_SIZE` (WxH or native), `DURANDAL_BENCHMARK_FPS`,
  `DURANDAL_BENCHMARK_SPEED`, `DURANDAL_BENCHMARK_SHOTS` (dir),
  `DURANDAL_BENCHMARK_SHOT_EVERY`, `DURANDAL_BENCHMARK_HIDDEN=1`,
  `DURANDAL_BENCHMARK_RENDERER=gl|metal`. The `--benchmark*` flags still
  exist but must not be used for paths.
- Benchmark: plays the film unattended, logs every presented frame, writes
  `<csv>.summary.txt` (average fps, worst second, 1% low, interpolation
  check, where the CPU spent the frames and the slow ones: wait for the
  display / game tick / render / present, dynamic light counts,
  presentation intervals against the display's refresh) and quits; never
  writes prefs. CSV columns: frame, time, dt, tick, fraction, wait_ms,
  tick_ms, render_ms, present_ms, lights, casters. `scripts/benchmark.sh
  <label>` runs the standard films plus the owner's own (`tests/benchmark-films`,
  recorded 26 Sep 2026: 260926-1 level 7, -2 level 6, -3 level 1) full
  screen; `FILMS_ONLY="1 4"` picks a subset. `DURANDAL_BENCHMARK_END_TICK=n`
  stops a run after game tick n (A/B runs over one stretch);
  `DURANDAL_BENCHMARK_VSYNC=1` runs with vsync as in play;
  `DURANDAL_DRAWABLES=2|3` sets the swap chain. `scripts/costs.sh <out>
  <film> <end-tick> label=settings ...` plays one stretch per variant on
  screen and tabulates fps and GPU time (interleave base runs: the fanless
  Air and macOS daemons drift). `scripts/ratio.swift a.png b.png out.png`
  grids the brightness ratio of two frame shots (finds shading steps).
  Code: `Source_Files/Misc/DurandalBenchmark.*`.
- Benchmark hygiene (26 Sep 2026): with a third display attached the MSI
  presented at 120 Hz and Flagship measured 135 avg / 56 1% low; alone at
  240 Hz the same build measured 178 / 102. After a reboot `duetexpertd`
  and the Spotlight indexers can pin a core for many minutes and drag
  every run to ~140 fps: check `ps -Ao %cpu,comm -r | head` before
  measuring. GPU stage timing (`DURANDAL_GPU_TIMING=1`) may cost a few
  percent itself.
- Hidden runs (approved on 23 Sep 2026 for development; since 26 Sep
  everything runs on screen instead; keep the scripts, use them only when
  the owner says so): hidden
  window, `SDL_MAC_BACKGROUND_APP=1`, no relative mouse mode or warps.
  `scripts/parity.sh <dir> [film] [every] [speed] [WxH]` renders the world
  with OpenGL and Metal at set ticks and saves `-gl`, `-metal`, `-diff`
  PNGs plus `parity.csv`; it kills the run if no shot arrives for 40 s.
  `scripts/bench-hidden.sh <out> gl|metal [film] [WxH]` is an indicative
  hidden benchmark. Hidden-window numbers are not the official baseline.
- `scripts/frames.sh <dir> [film] [every] [WxH]`: whole-frame comparison
  (HUD, overlays, fades included) of OpenGL vs Metal display, two hidden
  runs at a fixed 30 fps, scored by `scripts/compare-frames.swift`.
- `DURANDAL_SET="attr=value,..."` (dev runs only: benchmark or menu shot)
  overrides Durandal settings by their stored names, e.g.
  `shading_tables=1,shading_style=0`; needs `DURANDAL_QA=1` for Release.
  `scripts/looks.sh <dir> <film|""> <every> <WxH> label=settings ...`
  renders the same ticks with the Metal display under each variant.
  `xcrun swift scripts/crop.swift <out> x y w h scale <png>...` crops and
  enlarges side by side; `scripts/stats.swift x y w h <png>...` prints mean
  RGB. `DURANDAL_DUMP_RAMPS=<file>` writes the 8-bit ramp tables as built.
  Keep frame-shot spacing under 40 s of film time (the scripts' watchdog).
- `DURANDAL_BENCHMARK_SHOW_FPS=1` turns the in-game fps counter on for a
  benchmark run (in-game text in frame comparisons: `SHOW_FPS=1
  scripts/frames.sh ...`).
- `DURANDAL_MENU_SHOT=<png>` (with `DURANDAL_BENCHMARK_HIDDEN=1`, `-Q`; add
  `DURANDAL_MENU_SHOT_DIALOG=durandal` to capture the DURANDAL dialog):
  saves the main menu after 3 s and quits. Dev runs (benchmark or menu
  shot) always use a fixed-size window and never write preferences.
- `DURANDAL_METAL_DISPLAY=1|0` forces Metal display mode on/off for a run;
  `DURANDAL_BENCHMARK_OFFSCREEN=1` renders Metal frames off-screen without
  presenting (hidden windows' drawables are throttled by macOS, so hidden
  Metal benchmarks must use it).
- The Release app can't be debugged (hardened runtime); use `sample <pid>`
  to see where a hidden run is stuck.

## GitHub

- Remote `github` (`sebcarley/durandal`): **public since 3 Oct 2026**, on
  the owner's word, pushed at `baseline-11` with the guide F placed. It
  holds `master` (the untouched upstream mirror), `durandal/main` and the
  tags `baseline-7` to `baseline-11`, and opens on `durandal/main`.
  Everything pushed there is public at once: scan new commits for the
  owner's name and for secrets before every push. Its history begins at
  baseline-7: one commit on upstream `master` carrying the whole fork.
  `docs/HISTORY.md` summarises what came before, `docs/GUIDE.md` is the
  field guide, `docs/DURANDAL.md` the write-up. Push only when the owner
  asks.
- The working history before baseline-7 (every round's branch, with the
  owner's name in its commit messages and early notes) is kept locally on
  the old `durandal/*` round branches, with its tags renamed
  `archive/baseline-0` to `archive/baseline-7`, and on GitHub in a second
  private repository (remote `github-archive`). That one must never be
  made public. The rewrite was a single exception to the rule against
  forcing anything, given by the owner on 29 Sep 2026; the rule stands.
- Keep the owner's name out of all new text, commit messages included.
- New work branches from `durandal/main` as `durandal/<feature>`. When its
  QA has passed it is fast-forwarded into `durandal/main` and tagged
  `baseline-N`, on the owner's word.

## Conventions

- Branch names: `durandal/<phase-or-feature>`, from `durandal/main` (the
  published line). Tags: `baseline-N` at each safe point the owner has
  confirmed. Branches and tags named in the Status notes below from
  before baseline-7 live in the archive history.
- Keep our code in clearly named files (`Durandal*`, `Metal*`) to ease
  upstream merges; keep upstream files' edits small and commented.
- Commit messages in British English, describing the why.

## Status

- Tags: `baseline-0` (Phase 0), `baseline-1` (Round 1), `baseline-2`
  (Round 2; QA passed, white-box text fixed after it).
- Round 2 (Metal renderer) on `durandal/round-2-metal`:
  - 2a (done, awaiting QA): Metal world renderer bridged into the GL
    frame (`RenderMain/DurandalMetal*`, `Rasterizer_Metal.*`,
    `RenderRasterize_Metal.*`), switch "Metal Renderer (Experimental)" in
    Preferences > DURANDAL (independent of tiers, off by default). Parity
    vs OpenGL on L00, L06 and 5D Space: mean diff < 0.25/255; only
    differences are weapon-in-hand sub-pixel rounding and static noise.
    Not ported: bloom, bump maps, 3D models.
  - 2b (done, awaiting QA): Metal display mode. With the switch on
    (applies at launch), the window is a Metal window with no OpenGL
    context; the world composites straight into the drawable; all 2D code
    runs unchanged on `RenderMain/DurandalGL.*` (the GL 1.x subset it uses,
    implemented over Metal; `DurandalGLShim.h` is included by the 2D files
    and passes through to real GL when Metal display is off). Whole frames
    match OpenGL on L01 demo, T01, L06, 5D Space (mean diff <= 0.115/255)
    and the main menu is pixel-identical. Movie export is refused in Metal
    mode (not ported). Lua HUD, automap, terminals, dialogs, chapter and
    load screens not yet verified by the tooling - the owner's QA.
  - 2c (done, awaiting QA): everything draws into an 8-bit canvas
    (as the GL back buffer), and a final pass writes it to the drawable.
    Pacing: in game the main loop takes the next drawable at the top of
    each iteration (before input), 3 drawables (since 26 Sep 2026: with 2
    and vsync at 240 Hz an 8 ms frame was shown every third refresh, 69
    fps; with 3 every refresh, 202 fps), 2 frames in flight. HDR Output switch (independent, live): half-float EDR layer in
    extended linear Display P3, exact sRGB decode, so SDR looks the same;
    `DURANDAL_EDR_TEST=1` (scheme switch, off by default) shows a 1.0 patch
    beside a headroom patch, top right. `DURANDAL_HDR=1|0` overrides for
    dev runs. Headroom: `DurandalEDR.mm` (AppKit kept out of engine
    headers). Benchmark reports presentation intervals on screen. The
    bridge no longer follows the setting (dev/parity use only).
    CAMetalDisplayLink not used: the loop is not callback-driven and
    drawable-paced waiting gives the same latency at our frame cost.
  - Widescreen (16:9 was asked for): tier feature, on in Classic/Custom.
    With the classic HUD on a window wider than 2:1 above the HUD, the
    view fills the width (screen.cpp view_rect) and FOV becomes Hor+
    beyond 2:1 (ViewControl.cpp); at or below 2:1 nothing changes. HUD
    panel stays classic, black either side (dressing it is a follow-up).
  - Settings missing from an older prefs file default on unless the tier
    is Stock.
- Round 3 (faithful look core) on `durandal/round-3-look`:
  - 3a (done, awaiting QA), all Metal renderer only, tier features:
    - L1 Marathon Shading: 8-bit Marathon darkens a pixel by walking down
      its authored colour ramp, not by scaling RGB. `DurandalShading.*`
      records each colour table's runs from shapes.cpp's own colour list
      (same run rule as build_shading_tables8); DurandalMetal bakes a
      256x160 shade texture per table (rows 0-31 the 32 tables, then 128
      smooth rows); TextureManager::PlaceIndexImage uploads each shapes
      texture's colour indices (RG8Uint, mipmapped Quake-style: per 2x2
      the block colour nearest the average). Programs kWallRamp and
      kSpriteRamp; Shading Style Banded (exact tables) or Smooth. The
      ramps include the self-luminous floor, so their glow pass is
      skipped (the glow texture is still placed once, else NeedsImages
      rebuilds the texture every frame). Replacement textures, landscapes,
      infravision, invisibility and static stay true colour.
    - L2 Texel Lighting: headlight depth and fog distance taken at the
      texel centre via screen-space derivatives (all wall/sprite shaders).
    - L4 Crisp Filtering: sharp bilinear when magnified (1 tap inside a
      texel), trilinear + 16x anisotropic when minified (true colour;
      wall/sprite textures now always have mipmaps); on the 8-bit path,
      two index mip levels blended. Smooth Edges: 4x MSAA world pass,
      memoryless, previous frame redrawn as the backdrop for the void
      smear.
    - L3: nothing to do. Smear is not drawn by any renderer (the software
      one rejects the mode) and fade-to-black is `#if 0` in Bungie's
      source; static is already at original-resolution blocks.
    - Cost (hidden, off-screen, L00, 1080p): all off 763 avg / 179 1% low;
      all on 397 / 144 (banded 371 / 139).
  - 3b (done, awaiting QA):
    - T2 Crisp Text: FontSpecifier rasterises glyphs at the screen pixels
      per text unit they are drawn at (DurandalGL::PixelsPerUnit, quarter
      steps 1-8), keeping base advances; scale 1 is the original path.
      Only matters for scaled text (classic HUD at HUD scale, Retina);
      the Enhanced (Lua) HUD and 1080p messages are already 1:1.
    - T1 Crisp Terminals (`RenderOther/DurandalTerminal.*`): the terminal
      is still drawn at 640x320, but TrueType text into it is recorded
      (ttf_font_info::_draw_text); Compose() scales the image by a whole
      number (ceil of display height / 320, max 4) and draws the text
      from the fonts at that size, each glyph at its base position times
      k, so layout and page breaks are identical. Metal only (OpenGL's
      blitter tiles at 256 px; in Metal mode OGL_Blitter now uses one
      tile). `DURANDAL_BENCHMARK_TERMINAL_SHOTS=1` saves a frame shot for
      every terminal page shown (L01 Tooncinator film has three).
    - The Metal Renderer switch is now a tier feature: Classic turns it
      on, Stock off (upstream OpenGL); HDR Output stays independent.
  - Tag `baseline-3` (QA passed; Smooth shading preferred, now the
    default and set by Classic). After it: colour tables not laid out as
    ramps (M2 weapons in hand, landscapes) keep true-colour shading (the
    walk made the pistols look bloody in shadow); the Run action no
    longer attaches LLDB (hardened runtime refused it).
- Round 4 (Glow) on `durandal/round-4-glow`, done, awaiting QA.
  New tier Enhanced (2) = Classic + Glow, Bloom, HDR Sky
  (`Durandal::FeatureTier`); a feature missing from the prefs file is on
  in Custom and in tiers that include it.
  - Glow image (E1): every world fragment writes colour and glow (MRT,
    `WorldFrag`, RGBA16F `world_glow`; memoryless scratch when off; 4x
    memoryless with Smooth Edges). Sources: self-luminous colours (shade
    texture alpha 255: monster eyes and armour lights); each sprite
    frame's minimum light (`u.emissive`: projectiles, explosions, muzzle
    flashes, lava scenery); wall lights per texel (index texture is now
    RGBA8Uint: index, opacity, glow), from `RenderMain/DurandalGlow.*`,
    a hand-reviewed list of M2 wall bitmaps (water, lava, sewage, Pfhor;
    Jjaro set 20 not reviewed, no test film loads it) with a rule per
    bitmap (colours / white / hot / reds only), each keyed by a
    fingerprint of the bitmap's pixels, so only M2's own textures match.
    Wall glow scales with the surface's light.
  - Output: the world blit writes canvas alpha 0; overlays raise it, so
    (1 - alpha) is where the world is uncovered. With HDR Output and Glow,
    the output pass adds the glow (linear) x (min(headroom, 4) - 1).
  - Bloom: half-res 5-level chain from the glow (Jimenez downsample, tent
    upsample, linear light), added in the output pass at 0.35 in SDR and
    EDR, only where the world is uncovered.
  - HDR Sky (V2): landscape as a cylinder (identical at view centre, less
    stretch at wide FOV); near-white sky highlights glow (smoothstep
    0.75-1 luminance x 0.5, so a bright sky does not wash out).
  - Cost (hidden, off-screen, L00, 1080p): Enhanced 306 avg / 178 1% low;
    Classic 367 / 151.
  - Dev: `DURANDAL_GLOW_VIEW=1` shows the glow image as the world;
    `DURANDAL_BENCHMARK_OUTPUT_SHOTS=1` captures frame shots after the
    output pass (glow and bloom included; EDR clipped); 
    `DURANDAL_DUMP_BITMAPS=<dir>` writes wall bitmaps and prints their
    fingerprints; `scripts/sheet.swift` makes contact sheets.
  - The DURANDAL dialog is now full; the next feature needs pages.
- Tag `baseline-4` (Round 4, QA passed). The All Weapons cheat now
  skips items with no shapes in the scenario (in M2: Infinity's SMG and
  its ammo, items 34-35).
- Round 5 (Light) on `durandal/round-5-light`, done, awaiting QA
  (all Enhanced, Metal display; DURANDAL dialog now in tabs: Feel,
  Look, Light, Cheats):
  - E2 Dynamic Lights (`RenderMain/DurandalLights.*`): per frame, objects
    in the visible polygons drawn with a minimum light (projectiles,
    effects; monsters only on frames >= 0.5) plus the viewer's flash
    become up to 16 point lights (radius ~1-2 WU by strength, colour from
    the frame's bright pixels, cached). Shaders add the light to the
    surface light level before shading (bands like Marathon's), additive
    only: unlit + (lit - unlit) x light colour. Walls use a face normal
    from derivatives (a light only reaches its side of the plane).
  - E3 Light Shadows: no ray tracing hardware; the map (9 float4 per
    polygon: edges, neighbours, current floor/ceiling) is uploaded per
    frame and the shader walks polygons from the lit point's polygon
    (`u.polygon`, the node being drawn) to the light's, checking each
    crossing against the opening heights, as Marathon's line-of-sight
    does; 5D space is handled by adjacency.
  - S1 Ceiling Light on Sprites: sprite light = max(floor, (floor +
    ceiling)/2).
  - S2 not done: sprites are not depth-tested in this renderer (they are
    cut by the portal clip windows), so there are no depth intersection
    lines to soften.
  - Benchmarks were unreliable while another app was loading the Mac;
    lights cost roughly 5-15% back to back.
- Tag `baseline-5` (Round 5, QA passed).
- Round 6 (Water) on `durandal/round-6-water`, done, awaiting QA.
  Real Liquids (W1; Enhanced, Metal display; forces see-through liquids,
  which the owner's OpenGL settings have off):
  - Every world fragment also writes its distance from the viewer to a
    memoryless R32F colour attachment 2 (`WorldFrag.distance`).
  - Liquid surfaces (media surface = height and texture match the
    polygon's media) use `liquid_fragment`, blending off, which reads
    the frame beneath via [[color(0/1/2)]] (Apple tile memory):
    path under the surface = distance below - surface distance ->
    per-channel absorption and murk towards the surface's lit colour;
    animated wave normals shimmer the texture; Fresnel sheen, dynamic
    light and headlight glints; lava opaque and glowing. Per-type
    styles in `RenderRasterize_Metal::liquid_style`. No true refraction
    (a single pass has no copy of the frame).
  - Caustics: surfaces in a polygon with liquid, below its height, get a
    moving caustic pattern added as light (bands) - `u.caustics`.
  - Underwater: the world image wavers (DurandalGL::SetWorldDistortion).
  - With MSAA, a shader that reads the frame runs per sample, so the
    liquid shader is kept light (no texel snapping, no light shadows).
  - Benchmarks were unreliable again (another app loading the Mac).
- Tag `baseline-6` (Round 6, QA passed with fixes). After it:
  - Liquids: the light reaching what is below falls off with its depth
    under the surface (x1.5 of the view path's absorption), so floors
    darken as liquid deepens; water murk 1500 -> 1000. Underwater waver
    halved and slowed.
  - Contact Shadows (Light tab, Enhanced; `DurandalLights::GatherCasters`,
    shader `contact_shadow`, buffer 4, up to 32 nearest): items, monsters
    and scenery in visible polygons (not self-lit frames, not the viewer)
    darken upward-facing surfaces at their floor height, radius from the
    shape's width, fading as they rise; walks the ramps on the 8-bit path.
  - `SPEED=n scripts/looks.sh ...` plays films faster (ticks shift by a
    few). Water film: Saved Games `M2 L01 - L03` ticks 3432-3960;
    monsters on floors: ticks ~6900, 7800.
- Round 7 (Air) on `durandal/round-7-air`, awaiting QA. New tier
  Flagship (3) = Enhanced + Volumetric Fog (V1; Light tab):
  - A compute pass (`volume_kernel`, `DurandalMetal::RunVolume`) builds a
    fog volume each frame: one thread per 8x8-pixel column, 64 slices
    from 0.25 to 64 WU spaced in log distance (RGBA16F: in-scatter,
    transmittance). Each column walks its view ray through the map
    polygons (like light_reaches; stops at walls, steps, ceilings) and
    integrates haze lit by the room's own floor/ceiling light (so dark
    rooms keep dark air), dynamic lights that reach it (x3), and lava
    beneath it; thicker near the floor (mist) and stirred by drifting
    dust noise. Under a liquid the viewer is in: that liquid's murk;
    seen from above, liquid_fragment still draws the depth.
  - Per-environment haze in `RenderRasterize_Metal::volume_params`
    (water/Jjaro subtle, lava and sewage thicker). MML fog, when a
    scenario has it, becomes the haze colour and density and classic fog
    is switched off.
  - World fragments apply it at their distance (`fogged_frag`, texture
    3); the world render encoder now opens at the first draw so the
    compute pass can run first in the same frame; lights, map and
    casters are cached and bound when it opens. BuildMap's spare w
    components carry floor/ceiling light, liquid height and type + 1.
  - Fog after the first look ("not convincing"): drifting patches
    (thresholded 3-octave noise, smoothed where slices are deep), more
    floor mist, smoke rising off lava for its glow, dimmer haze colour;
    Fog Strength (Light / Medium / Thick, stored `fog_strength`) on the
    Light tab scales the density.
  - Fix (QA: What About Bob cellars glowing orange): BuildMap records a
    polygon's liquid only when it is above the floor (drained lava no
    longer smokes and glows in the fog); dynamic lights light the haze
    x0.8 (was x2: one bolt flooded a corridor yellow).
- Round 8 (Flagship) on `durandal/round-8-flagship`, awaiting QA:
  - E4 Light Redistribution (Flagship, Light tab;
    `RenderMain/DurandalRadiance.*`, kernels `radiance_bake` and
    `radiance_average`, shader `redistribution`). Surface cache: a patch
    of lumels (8 per WU, at most 256 across) per polygon floor, ceiling
    and side-owning edge, shelf-packed into one RGBA16F atlas 2048 wide.
    Each frame (before the world encoder opens) up to 384 8x8 tiles of
    unsettled patches in view, nearest first, get 8 cosine rays per
    lumel traced through the polygon map (walls, steps, floors and
    ceilings as Marathon's own geometry, so 5D space is right); a hit
    sees that surface's authored brightness x its texture's average
    colour (sky through landscape surfaces, DurandalGlow wall lights x2),
    hits nearer than 1.5 WU weighted down (so corners and wall feet
    gather less); running average to 192 samples, then left alone;
    patches around a moving platform drop back to 16. Per-patch averages
    are reduced on the GPU. Surfaces are shaded at their own brightness
    x clamp(lumel / patch average, 0.6, 1.35) (walks the ramps on the
    8-bit path) with a colour bleed capped at 12%, so each surface's
    average stays as designed (AUDIT 3.6). Patch per draw from
    `DurandalRadiance::FloorPatch/CeilingPatch/WallPatch` (a side's
    texture definition pointer gives the side). BuildMap's layout is
    shared with the fog and light shadows.
  - M1 Surface Relief (Flagship, Look tab; `RenderMain/DurandalRelief.*`,
    shader `surface_relief` / `relief_light`): at texture load, walls'
    brightness (3x3 blur, contrast set per texture around its mean)
    becomes a height 1-255 in the index texture's fourth channel (0 =
    none: sprites, DurandalGlow wall lights), averaged down the mips.
    8-bit shading path only. Per texel: normal from neighbouring heights
    in the texture frame (screen derivatives); the headlight scaled by
    relief vs flat face (0.55-1.45), a highlight where the relief turns
    to it more than the face, the room's light shaped gently as if from
    above and in front (averages out over the texture), dynamic lights
    use the relief normal; parallax at most half a texel, up close only.
    No per-bitmap hand-tuning yet (roadmap asks for it).
  - Not yet: F6 (not needed at 1080p).
- Round 9 (Sound) on `durandal/round-9-sound`, awaiting QA:
  - A1 Room Reverb (Enhanced, Feel tab; `Sound/DurandalReverb.*`).
    Main thread, every 100 ms (SoundManager::Idle, 2D and 3D sound): a
    breadth-first walk from the listener's polygon through openings of
    at least 1/4 WU (closed doors stop it), out to 24 WU / 400 polygons,
    sums volume, hard surfaces (absorption 0.1) and open ones (landscape
    ceilings and sides, plus half of what leaks past the reach); 1 WU =
    2 m. Sabine RT60 -> decay (0.2-5 s); mean free path -> reflection
    and late delays, reflections stronger in small rooms, openness damps
    both. Under a liquid: EFX_REVERB_PRESET_UNDERWATER (darker for goo,
    sewage, lava) and world sounds' highs cut to 25% (UnderwaterFilter
    for 2D, folded into GetLowPassFilter for 3D).
  - OpenALManager: one EAX reverb effect (plain reverb fallback) in one
    auxiliary slot, glided over 0.4 s on the audio thread; sounds with a
    world source (`SoundParameters::in_world`) send to it, interface,
    ambient loops, music and streams don't (sends reset on pooled
    sources). The owner plays with 3D sounds off (2D panning), so both paths
    matter.
  - `DURANDAL_REVERB_LOG=1` prints the estimate as it changes. Hidden
    runs with sound: drop `-s` and set `SDL_AUDIODRIVER=dummy` (silent,
    the mixer still runs); never play sound on the speakers unasked.
- 26 Sep 2026 (on `durandal/round-9-sound`, after the first look at
  Round 8/9; all awaiting QA):
  - Swap chain: three drawables (see Round 2c pacing note).
  - Surface relief is zero-mean: its three lighting terms are linear in
    the unnormalised height gradient (`Relief::tilt`) at a gentle gain and
    act as one factor on the finished shade (`classic_shade`); filtering
    and texel lighting use the unshifted texture coordinate. Relief on vs
    off now differs by < 4% per region (was 10-20%: clipped, skewed
    gradients shifted the mean and drew a rectangle where relief faded).
  - Light redistribution groups patches: adjacent coplanar surfaces with
    the same texture, light and heights (floors/ceilings across shared
    edges; walls continuing along a line; platforms alone) share one
    average (`Patch::info.w` = group; `radiance_average` writes the
    group's average to every member), so a gradient runs through polygon
    edges instead of resetting (per-polygon steps of up to 50% drew
    rectangles along polygon outlines: the frame reported). Stronger
    variation along long corridors as a result: to be judged in QA.
  - World fragments carry a fixed zero-mean dither (< 1 level,
    interleaved gradient noise) against banding in murk and haze.
  - Scene grade (Look tab; `scene_brightness` -25..25, `scene_contrast`
    50..200, `scene_gamma` 50..200, percent; neutral 0/100/100 and in
    Stock): the output pass grades the world image only (canvas alpha 0),
    `pow(saturate((c - 0.5) * contrast + 0.5 + brightness), 1 / gamma)`
    in the sRGB-encoded canvas before EDR decode.
  - The liquid surface's own light is capped at its room's (floor and
    ceiling average) except lava, so a lit pool in a dark room is no
    longer floodlit (last session's fix, `render_node_floor_or_ceiling`).
  - Next asks, not started: soft shadows and shading on walls (what
    looks wrong is still to be said); true look up/down and sidestep sway:
    planned in full in `docs/PLAN-true-3d.md` (Round 10, branch
    `durandal/round-10-camera`): upstream's `OGL_Flag_MimicSW` (0x4000,
    on in the owner's prefs) is what keeps verticals vertical, and
    `Rasterizer_Metal` already rotates by the pitch when it is off, so
    the job is a Durandal switch plus visibility bookkeeping that
    contains the rotated frustum, roll in the matrices, and Quake's
    roll formula for the sway.
- Round 10 (Camera) on `durandal/round-10-camera`, awaiting QA.
  Plan: `docs/PLAN-true-3d.md`. Both Feel tab, Enhanced, Metal world
  renderer only (the OpenGL path keeps upstream's own 3D Perspective
  option; the M1 exploration view is never touched):
  - C1 True Look Up/Down (`true_look`): the camera rotates with the
    pitch (`view->mimic_sw_perspective` off) instead of shearing the
    screen, so verticals converge as in Quake. The 2.5D visibility walk
    assumes a level screen, so with it on `render.cpp`
    (`durandal_true_look_view`, per frame) takes the horizontal cone
    from the widest yaw of the rotated frustum's four corner rays (and
    world_to_screen from that cone, as upstream's fixed 1.3 widening
    did, so every window the walk finds is inside the tree's screen) and
    the top and bottom clip extents from its highest and lowest rays:
    `view_data::dtanpitch_top/bottom`, used by RenderVisTree (default
    and per-line clips) and RenderPlaceObjs (object rects) in place of
    `dtanpitch`; off, both equal `dtanpitch`. Parity off vs the previous
    build (260926-3, 31 shots): 24 bit-identical, the rest <= 0.14/255
    (shot timing). Portal clip planes (vertical world planes), the liquid
    plane, sprites, the fog volume (matrix derived), light shadows,
    redistribution, relief and the weapon in hand need no change.
  - Roll: `view_data::durandal_roll` (degrees, positive clockwise as
    Quake) applied in Rasterizer_Metal about the view axis before the
    pitch; `u.roll` un-rolls the sphere sky (unverified: M2 has no
    spherical landscape); flat landscapes take it from the eye-space
    direction.
  - C2 Sidestep Sway (`sidestep_sway`; `RenderMain/DurandalCamera.*`):
    Quake's Com_CalcRoll on the player's `perpendicular_velocity` (read
    only) over the running model's maximum: full 2 degrees from 80% of
    it (running gives 2, walking 1.6, as Quake's 320/160 against
    cl_rollspeed 200), glided with an 80 ms time constant on world time
    so a film replays the same. Constants at the top of
    DurandalCamera.cpp; `DURANDAL_CAMERA_LOG=1` prints them per tick.
  - Cost (L06 stretch to tick 1200, full screen, another app loading the
    Mac): off 255 avg / 147 1% low, on 258 / 142, off again 224 / 119:
    within the drift.
  - Film tests green both ways after it (26 Sep 2026). The film
    `tests/benchmark-films/260926-4.filA` (outdoor level, recorded with
    the new view): the player dies at about tick 640 and the rest of that level
    is the dead view, then a second level (ticks restart; the benchmark's
    frame shots are named by tick, so a second level overwrites the
    first's). Not in the benchmark set.
  - Look limit: Marathon's physics models cap the aim at 30 degrees up
    and down (`maximum_elevation`, QUARTER_CIRCLE/3 in both models), and
    upstream's Auto-Recenter View (Preferences > Controls) levels the view
    while running. True Look alone draws whatever the aim is. The owner asked
    for the full 180 degrees and chose a free look beyond the aim with
    the crosshair on the real aim (26 Sep 2026):
  - C3 Free Look Beyond Aim (`free_look`; Feel tab, Enhanced; needs True
    Look; `DurandalCamera`). The mouse's pitch is integrated into a free
    view pitch. Within the aim limit the game is the master: the tick
    gets the mouse delta untouched, exactly as before. Beyond it the tick
    is asked for one unit past the limit every tick (the physics clamps
    it, as when a player pushes against the limit, and it keeps absolute
    pitch mode so there is no auto-recentre meanwhile); on the way back,
    for the exact delta that puts the aim under the view. Hooked where
    the tick's aim is sampled (vbl.cpp `parse_keymap`,
    `FreeLookTickInput`) and after the per-frame look (screen.cpp,
    `ApplyFreeLook`). Live mouse look only, never a replay or net game;
    films record what the tick asked for, so they replay exactly, but
    show the aim rather than the free look. Limit: the visibility walk
    covers only the half plane in front of the viewer, so the view may
    pitch to 90 degrees less the vertical half FOV, computed per frame
    (about 65 degrees at FOV 80 on 16:9, 39 with extravision); the last
    stretch to straight up would need a second, backward walk (not
    done). Crosshair: marks the aim, pinned a little in from the edge
    when the aim is off the screen (`CrosshairOffset`, roll included).
    The Enhanced (Lua) HUD draws its own reticle, so while the aim is
    off centre `Screen.crosshairs.active` reads false (the reticle
    vanishes) and the engine's crosshair, scaled with the view (x3 at
    1080p), marks the aim; `Screen.crosshairs.aim_x/aim_y` give a Lua
    HUD the offset. Dev: `DURANDAL_LOOK_OFFSET=<degrees>` pitches the
    view that far beyond the aim in film runs (checked at +35 and -35 on
    260926-3: no missing geometry).
- Crash record (26 Sep 2026, after crashes under Xcode's Run left no
  report; `Misc/DurandalCrash.*`, installed from main.cpp): on a fatal
  signal or an uncaught C++ exception the game appends the signal and
  address, the main thread's phase (main loop / update_world /
  render_view / sound idle), game state, level, tick, the player's
  position, resident memory, the longest main-loop gap and a backtrace
  with the load address to `~/Library/Logs/Durandal Crash.txt`, then
  lets the signal proceed. Names are mangled: pipe through `c++filt`,
  or `atos -o <binary> -l <load address> <addresses>` against the
  build's dSYM. Every five minutes a diary line (uptime, level, tick,
  memory, longest gap) goes to the Aleph One log.
  `DURANDAL_CRASH_TEST=<seconds>` faults on purpose to check it. Under
  Xcode the debugger stops first; Continue once and the record is
  written before the app dies.
- Dev tooling since Round 7:
  - `xcrun swift scripts/check-shaders.swift <files>` compiles the raw
    MSL strings with the Mac's Metal device: run it after every shader
    edit, as a failed compile at launch crashes the game.
  - Dev runs (hidden benchmark, menu shots) register
    `ApplePersistenceIgnoreState` in memory (DurandalEDR.mm), so a crash
    in one no longer makes macOS's modal "reopen windows?" prompt block
    the next.
  - Dialog shots: `DURANDAL_METAL_DISPLAY=1` with the menu-shot
    variables, under `perl -e 'alarm 40; exec @ARGV'`;
    `DURANDAL_MENU_SHOT_TAB=<n>` picks the tab (0 Feel, 1 Look, 2 Art,
    3 Light, 4 Rampant, 5 Cheats).
- Testing cheats (`Misc/DurandalCheats.*`, Preferences > DURANDAL >
  Cheats tab, stored as `cheat_*`, off in Stock): Level Select on New
  Game (upstream's Shift+Ctrl / Option+Cmd click also works), God Mode
  (damage_player absorbs; energy and oxygen topped up each tick in
  update_players), All Weapons and Ammo (items.cpp
  durandal_give_all_weapons at entering_map). Since 26 Sep 2026 the
  cheats a single-player game begins with are bits 0x4000/0x8000 of
  `game_data::cheat_flags` (carried by films and saved games; vbl.cpp
  keeps them on replay), so films recorded with them replay with them
  and films without never see them (film tests unaffected). Switching a
  cheat in mid-game stops that game's recording. Awaiting the owner's QA:
  record a game with All Weapons on, replay it, it should stay in sync.
  `DURANDAL_MENU_SHOT_DIALOG=cheats` captures the dialog.
  Since 26 Sep 2026 (as asked): Noclip (`cheat_noclip`, flag 0x2000;
  physics.cpp `instantiate_physics_variables` skips the wall and object
  collisions for the local player and takes the destination polygon's
  floor and ceiling; the void still blocks, gravity still applies), and
  keys that switch God Mode and Noclip during a game (Cheats tab,
  `cheat_god_key` / `cheat_noclip_key`, SDL scancodes, default G and N:
  `DurandalCheats::Update` reads the keyboard state each main-loop pass,
  flips the preference, prints "God mode on" on screen, writes the
  preferences, and the flag change follows as for the dialog, stopping
  any film recording).
  Summon BOBs (2 Oct 2026, QA passed the same day; `cheat_summon_key`,
  default C, unbound upstream and in the owner's keys; Cheats tab "Summon
  BOBs Key"): a press stops the film
  recording and asks for five security BOBs (`_civilian_security`), which
  arrive at the start of the next tick (`DurandalCheats::BeforeTick`,
  called in `update_world` after `exit_interpolated_world`, so the world
  is real, not interpolated, when objects are linked in). Spots beside and
  behind the player (75-180 degrees off the facing first, then 40; rings
  1-3 WU), each reached in a straight line without a solid line, steps
  within 1/3 WU, room for the BOB's height and radius, no lava, goo or
  liquid over half its height, no platform/teleporter/exit/ouch polygon,
  clear of solid objects. Created invisible and activated, so they teleport in (effect
  and sound) and hunt. While the cheat is available the BOB's collection
  is marked at level entry (`DurandalCheats::MarkCollections` in
  `entering_map`: a level without BOBs would not load it, and a monster's
  animation drives its attacks), and in any build whenever the level
  already holds a security BOB (a save made after a summon: unmarked, its
  BOBs would stand frozen and invisible, and a film begun from it would
  replay differently). Never in replays or net games.
  `DURANDAL_SUMMON_TEST=<tick>` summons at that tick of a film run (frame
  shots; the film desyncs after it): checked on L06 tick 150, five BOBs
  teleported in beside the player.
- Round 11 (HD art) on `durandal/hd-assets`, awaiting QA. The
  catalogue of packs and the engine notes are `docs/HD_ASSETS.md`
  (section 7: what was built). Everything is Flagship tier, on the new
  ART tab of the DURANDAL dialog, Metal display only:
  - `RenderMain/DurandalArt.*`: scans each installed plugin's MML once
    for `<texture normal_image>` and `<model>` entries and classes the
    plugin by the collections it replaces: Walls and Sky (17-21, 27-30),
    Monsters (2, 3, 5, 6, 8-16, 31), Weapons and Items (1, 4, 7), Scenery
    (22-26). Switches `hd_walls`, `hd_monsters`, `hd_weapons`,
    `hd_scenery`; `Apply()` (end of Durandal::AfterRead, and on the
    dialog's Accept, which then re-reads the MML as the Plugins dialog
    does) switches each pack with the category most of its entries
    replace (CFP Monsters also replaces a few alien projectiles; the
    all-in-one SuperPlugin is mostly monsters). With the gate open that
    overrides Environment > Plugins for those packs
    (the state is still written there); with it closed the plugins are
    stock Aleph One's. Takes effect at the next level (shapes patches
    load with the collections). The tab lists what is installed.
    Soundtrack (27 Sep 2026, `soundtrack`, the chosen plugin's name,
    empty = none, cleared by Stock): the scanner also finds soundtrack
    plugins (a map patch or MML naming `<music>` tracks, or a solo Lua
    script using the Music API) and enables exactly the chosen one, since
    a Lua script's tracks would stack on a map patch's playlist. The
    Solar Soundtrack is solo Lua, so it loads at the next new game and
    turns achievements off. Linked into the Plugins folder from
    `Assets/audio/`: "M2SE Music" and "The Solar Soundtrack for M2"
    (`docs/AUDIO_ASSETS.md`).
  - QA (27 Sep 2026): the HD look passed ("looks great"), with
    slightly less glow on Pfhor staffs and S'pht bolts than the 8-bit
    art. Cause: the 8-bit path glows self-luminous palette colours (the
    palette's brightest) at full strength; the HD sprites draw the same
    bolts as soft mid-toned pixels, so the glow image gets less. Fix:
    `glow_gain` (shader `glow_gain`, uniform set for substitute sprites
    only, `TextureManager::IsSubstitute`) lifts a replacement sprite's
    glow in proportion to its brightness, default 2 (bright pixels x2,
    dim ones hardly); `DURANDAL_HD_GLOW_GAIN=<f>` for film runs. Judged
    on L06 tick 1051 (fusion bolt ring, fighter, muzzle flash) at 1/2/3.
  - 3D Pickups (`models_3d`; Flagship; ART tab) and Spinning Pickups
    (`spin_pickups`): the 3D Items plugin (thedoctor45 1.3, 21 OBJ
    pickups with 256-512 skins, made for Infinity, item sequences shared
    with M2; linked from `Assets/thedoctor45/`) is classed as category
    "3D Pickups" (`<model>` entries) and, when on, upstream's loader
    (`OGL_LoadModels`, MML rotations and scale baked into the vertices)
    hands `rect.ModelPtr` to the Metal rasteriser as it always did.
    `render_model` places the model as the OpenGL path does (translate,
    yaw, scale; `Uniforms::model`, applied in `world_vertex` and identity
    for everything else), lights it as a wall (room light, headlight by
    depth, dynamic lights on the faces from derivatives), depth tests
    and writes, no contact shadow on itself, skin glow image as a second
    pass. `DurandalMetal::DrawModel` keeps a vertex/index buffer per
    OGL_ModelData and `PlaceModelSkin` a texture per skin image
    (`create_texture`, shared with PlaceTexture); `ReleaseModels` is
    called from `OGL_UnloadModels`. Sidedness sets the winding/cull per
    draw. Spinning Pickups turns items one revolution per 4 s with a
    per-position phase (a look only; the objects never move; films
    unaffected). Static models only (Dim3 animation not ported). Not
    covered by the plugin: the powerups and chips (sprites), and the
    SMG (absent in M2). `DURANDAL_MODEL_LOG=1` prints each model's first
    draw with its tick and position, for finding frames.
  - Next priorities (27 Sep 2026): soft shadows and lighting, then a
    texture cache. Spinning Pickups is off unless switched on (the
    owner's choice after seeing it), no longer a tier feature.
- Round 12 (Depth) on `durandal/hd-assets`, done, awaiting QA. The ask:
  light across rooms, character shadows, shadows in the distance and fog,
  for depth in the larger levels. Pieces so far:
  - Ambient Shadows (`ambient_shadows`; Flagship; Light tab; Metal
    display): screen-space ambient occlusion. The world pass's distance
    attachment is now stored (resolved under MSAA) when the switch is on;
    `ao_fragment` (half resolution, 8 hemisphere samples on a per-pixel
    rotated frame, positions rebuilt from distance along each pixel's ray
    via the frame's clip-to-world matrix, `DurandalMetal::SetView`, normals
    from screen derivatives, range-checked) writes an R8 occlusion image;
    the world blit (`DurandalGL::DrawWorldImage`, `world_blit_fragment`)
    blurs it 3x3 depth-aware and darkens the world image before gamma,
    fading out over the last quarter of its reach so haze and far halls
    are untouched. Sprites write distance, so their silhouettes shade the
    floor and walls behind them (a soft shadow of sorts). Defaults radius
    512, strength 1.2, reach 24 WU; `DURANDAL_AO="radius,strength,far"`
    and `DURANDAL_AO_VIEW=1` (shows the occlusion) for film runs. Cost on
    L00 full screen: 221 -> 161 fps average (about 1.7 ms).
  - Character Shadows (`character_shadows`; Flagship; Light tab; Metal):
    `draw_sprite_shadow` (before each monster, BoB, scenery or item
    sprite) projects the sprite's quad along a light direction onto the
    floor of the polygon under its feet (`world_point_to_polygon_index`:
    the render node's polygon can be another one entirely) and draws it
    with program `kShadow` (`shadow_fragment`: black, alpha from the
    sprite's own alpha at mip 1.5, no glow), blended, depth tested so
    walls cut it, not culled (a flat quad's winding depends on the
    light), then the sprite draws over it. Direction: the nearest dynamic
    light in range (bolts and explosions throw moving shadows), else a
    light 40 degrees up behind the viewer's left shoulder (azimuth
    relative to the view, since Marathon has no sun), so the shadow falls
    forward and right of every figure, never hidden behind its sprite;
    never flatter than 30 degrees. Fades as a figure rises (flying
    monsters), none for emissive, tinted or static frames. Seen from eye
    height a floor shadow is a low band, faint on dark floors and clear
    in lit rooms (L06 tick 1051). `DURANDAL_SHADOW="strength,elevation,
    azimuth"` (0.6, 40, 135), `DURANDAL_SHADOW_LOG=1`. Cost: nil.
  - Redistribution Strength (Light tab, `gi_strength` 0 subtle, 1
    medium, 2 strong; default medium = the Round 8 look): how far a
    surface's light may fall below or rise above its authored level and
    how much colour bleeds (`u.gi_range`: subtle 25%/20%/40%/8%, medium
    40%/35%/50%/12%, strong 55%/80%/60%/18%), so with Strong the gradients
    from lamps and openings run across a room.
  - Distance Shade (Look tab, `distance_shade` 0..100%, default 0, 0 in
    Stock, not a tier feature): in the world blit, surfaces darken with
    distance beyond the headlight's reach (smoothstep 8 to 48 WU, up to
    70% at full strength) from the stored distance image (kept when
    either this or Ambient Shadows is on); the sky is left alone. Judged
    on the L06 corridor at 60%.
  - QA (28 Sep 2026, morning): dark rectangles around HD sprites
    (flick'ta, drones, corpses, worst in liquid), black-mould creases
    under water, a light aura around the weapon in hand; the music was
    fine. Cause of the rectangles: the distance attachment was written
    unblended by every fragment that passed the alpha test, and the HD
    sprites' quads are blended draws with a 1/255 alpha test, so a
    transparent texel at 1/255 (which the BC7 p-bit can leave: alpha
    0 needs p=0, white colour wants p=1) drew the whole quad into the
    distance image that ambient shadows, the liquid murk and the
    distance shade read. Fixes: `WorldFrag::distance` is float4 with
    the fragment's alpha and blended draws blend it by alpha (a
    transparent texel leaves what is behind); `DurandalBC7::quantise`
    weighs alpha 64x when choosing the p-bit, so 0 and 255 stay exact
    (cache format 3 rebuilds the entries); the weapon in hand is always
    a blended draw with `Uniforms::distance_alpha` 0, so it never
    writes the distance image; ambient shadows are skipped while the
    viewer is under a liquid (`DurandalMetal::SetViewerUnderLiquid`);
    the character shadow leaves the floor's distance. Spinning Pickups:
    prefs schema 2 turns it off once for files from the day it was a
    tier feature.
  - Second look (28 Sep 2026): floors and 3D models "clipping"
    the gun (the floor's ambient shadows painted over a weapon that no
    longer wrote the distance image), black lines in goo (the pool
    bottom's creases darkening the surface, which never wrote it), dark
    vertical bands in the ribbed Pfhor walls and under water (those
    textures have partial alpha between the ribs, and a distance blended
    by alpha against the empty far value is nonsense). Now the distance
    is written all or nothing per texel (alpha over a half writes the
    fragment's distance, else it leaves what is behind), additive draws
    never write it (`Uniforms::distance_mode` 0), and the weapon in hand
    (mode -1) and liquid surfaces (LiquidFrag) write the far value, as
    the sky: no ambient shadow or distance shade on them, they occlude
    nothing, and with Smooth Edges the MSAA average of the distance at
    their edges stays far (a zero or negative marker averaged with the
    wall's distance made phantom near surfaces: a dotted line along the
    water line). The occlusion normal now comes from the neighbouring
    distances (on each axis the one nearer in depth, never a far one)
    instead of screen derivatives, which drew dotted outlines at the
    gun's edge and the water line.
  - Third look (28 Sep 2026): a dark "ring" under water in one or
    two places on Slings and Arrows: the weapon in hand, murked as if
    1-2 WU away (its fog distance came from its screen-space vertices);
    it now sits at 1/8 WU (world_vertex, by `distance_mode` -1). The
    ripple round door frames as a door opens (noted in Round 8): the
    redistribution clamped the door's surrounding patches to 64 samples
    every frame it moved, so each bake of 8 noisy rays landed at 11%;
    now a settled patch near a platform that moved gets `kRefreshBakes`
    (48) more bakes at a fixed 1/24 blend, so the change comes in over
    about a second without noise (`State::refresh`, `kRefreshBlend`).
  - Fourth look (29 Sep 2026, What About Bob, film
    `tests/benchmark-films/260929-1.filA`): dark lines and soft bands
    in the sky running off the level's geometry. The sky is drawn on the
    level's own ceiling and wall polygons, which wrote their real
    distances, so ambient shadows shaded the creases and steps between
    them (and Distance Shade would have darkened the sky). Landscape
    draws now write the far value (`distance_mode` 2).
  - Films and cheats (29 Sep 2026): that film replayed out of sync (the
    player died at tick 700 with 4 pistol magazines; it was recorded
    with God Mode and All Weapons). A film's header never had a cheat_flags
    field (StreamToGameData skips it), so the cheats were never in any
    film. They now travel in the header's spare second game parameter
    for solo games (vbl.cpp pack/unpack_recording_header; zero in
    every film made without cheats, so old films and the film tests
    are unchanged). The repo's copy of 260929-1 has 0xC000 (god, all
    weapons) patched in at offset 350; the original recording is untouched.
  - Loading stays upstream's (plugin loader, MML merge with later plugins
    winning per attribute, shapes patches, PNG and DDS decoders, the
    substitute path in TextureManager). Durandal adds: DDS in
    `DurandalMetal::PlaceTexture` (BC1/2/3 with the file's mip chain; a
    single-level DDS is decompressed and mipmapped); in
    `OGL_TextureOptionsBase::Load`, `offset_image` is read for Normal
    Maps regardless of the OpenGL bump flag and DDS mip chains are kept
    on the Metal display; `OGL_LoadTextures` decodes a collection's
    images across the cores (dispatch_apply_f) on the Metal display.
  - Normal Maps (`normal_maps`): TextureState::Bump binds beside the
    colour (texture 5; a flat 1x1 map otherwise). `normal_map_relief`
    (wall_fragment) tilts the light with Surface Relief's three terms
    (`relief_light`, now shared; `relief_factor` applies them to the
    finished intensity), and dynamic lights see the tilted normal. The
    CFP maps are y-up (OpenGL convention; checked at gain 3: a raised
    ceiling panel's far edge darkens under the headlight);
    `DURANDAL_NORMAL_MAP_Y=down` reads the other convention,
    `DURANDAL_NORMAL_MAP_GAIN=<f>` sets the strength (1.5). Replacement
    textures are true colour, so Marathon Shading, DurandalGlow's wall
    lights and Surface Relief do not apply to them; the packs' glow
    images do: `pack_glow` writes their `glow_bloom_*` share (and the
    colour image's `normal_bloom_*`) into the glow image for Glow, Bloom
    and EDR, as upstream's bloom pass computed it.
  - Measured on L06 with the four CFP packs (full screen, 1080p): level
    load 5.4 s against 2.7 s without HD art (11.2 s before the parallel
    decode); 260 fps average, 123 1% low, longest frame 11 ms; resident
    memory 3.06 GB on the 16 GB test Mac (the engine keeps every decoded image for the level,
    the 48 weapon-in-hand frames at 2048x2048 are 16 MB each, and the
    Metal copies are shared-storage). Follow-ups done the same evening:
    the texture cache and `<model>` (below); left: `landscape_bloom`.
- Texture cache (27 Sep 2026, on `durandal/hd-assets`, awaiting the owner's
  QA; `texture_cache`, Flagship, ART tab, Metal display):
  - `RenderMain/DurandalTextureCache.*`: a replacement image (colour with
    its mask, glow with its mask, or offset map) is block-compressed the
    first time it loads, BC7 mode 6 with a box-filtered mip chain
    (`RenderMain/DurandalBC7.*`: principal axis, least-squares endpoint
    refits, alpha-weighted so sprite edges keep their opaque pixels;
    walls 54 dB, sprites 40-53 dB on their opaque pixels, checked on
    the GPU against the CPU decoder), and written to
    `~/Library/Caches/Durandal/Textures/<hash>.dtx` (header, the key,
    the levels). Keyed by the files' paths, sizes and modification
    times plus the loading parameters; an updated pack rebuilds its
    entries; the folder can be deleted at any time; the ART tab shows
    its size. Later loads map the file (private, writable) and hand the
    mapping to the ImageDescriptor (`AdoptCompressed`, `MappedBase`,
    `FreePixels`; new format `ImageDescriptor::BC7`; `Opaque` flag), so
    the CPU side of a level's images leaves the footprint entirely and
    the GPU holds BC7. `MakeRGBA` decodes BC7 (opacity hacks still work,
    at RGBA cost for those few entries); `FindSilhouetteVersion` zeroes
    the blocks' colour endpoints. Hooked in `OGL_TextureOptionsBase::Load`
    (`DurandalTextureCache::Fetch`/`Store` around each upstream load).
  - Level entry with the CFP set was mostly not the images: the plugins'
    MML took 1.7 s a level (`InfoTree::read` walked boost's
    case-insensitive tree with locale comparisons and threw for every
    missing optional attribute: 4686 `<texture>` elements x 20
    attributes), the 3D Items OBJ files 0.55 s (read a byte per system
    call), the shapes patches 0.19 s (two bytes per call). Fixes:
    `InfoTree::find_node` (ordered scan with strncasecmp, no throw;
    nodes over 64 children use the index), `LoadModel_Wavefront` and
    `Plugins::load_shapes_patches` read the file whole first. MML now
    0.14 s, models 0.02 s, patches 0.01 s.
  - Measured on L06 with the four CFP packs and 3D Items, full screen:
    from the level's MML to the last collection loaded 3.0 s -> 0.42 s
    (first visit to an environment builds its entries: 12.6 s wall for
    3000 images, once); footprint after 10 s of play 4.7 GB -> 405 MB
    (347 MB with no HD art; `ps` resident memory hid 3 GB of it in the
    compressor); 167 fps against 162 without the cache; the same frame
    differs by 0.49/255 mean. Cache for that level: 2994 entries, 1.2 GB.
  - Dev: `DURANDAL_CACHE_LOG=1` prints level-entry phase times
    (`DurandalTextureCache::Mark`, `Tally`; marks in shapes.cpp,
    marathon2.cpp, Plugins.cpp, XML_LevelScript.cpp) and per-collection
    cache hits and builds; the benchmark summary now has a memory line
    (`DurandalCrash::ResidentMB`/`FootprintMB`). `texture_cache=0` in
    `DURANDAL_SET` is the PNG path for A/B runs.
- Tag `baseline-7` (29 Sep 2026): HD art, the depth round, the texture
  cache and the fixes from four QA passes, all judged in play in the
  Flagship tier with everything on; QA passed. Films now carry their
  testing cheats. Left for later: `landscape_bloom`, the audio round
  (`docs/AUDIO_ASSETS.md`), de-personalising before the repository goes
  public (see GitHub above).
- Weapon Takes the Light (29 Sep 2026, on `durandal/main`, in QA;
  `weapon_lighting`, Enhanced, Light tab; after the sister project's
  Quake work). The weapon in hand is drawn in screen space and took no
  dynamic light. `render_viewer_sprite` now sums the frame's dynamic
  lights at the viewer's position (strength and colour, the same falloff
  as `dynamic_light`; the lights come from the polygons in view, so each
  has a line to the eye; the viewer's own flash is among them) and hands
  the result to the sprite shaders as `Uniforms::viewer_light`, which
  stands in for the light they would have gathered. Gain 1.5
  (`DURANDAL_WEAPON_LIGHT=<gain>` for film runs). Checked hidden on the
  level 6 film, off against on: only the weapon's region differs, most at
  tick 1053 (the fusion bolts close by). Subtle on a black pistol in a
  still; to be judged in play.
- The short route from GitHub (29 Sep 2026): `scripts/setup.sh` takes a
  fresh clone to `Durandal.app` at the top of the folder (game data
  submodule, dependencies, HD art, build); `scripts/get-hd-art.sh` fetches
  the four CFP packs and 3D Items through their Simplici7y items'
  `downloads/new` redirects (Google Drive files become
  `drive.usercontent.google.com/download?id=...&confirm=t`), checks each
  is a zip, notes when its SHA-256 differs from the version tested,
  finds the folder holding `Plugin.xml` and moves it into the Plugins
  folder (`DURANDAL_PLUGINS_DIR` to install elsewhere). It never
  overwrites. The art is not ours to re-host (the CFP art derives from
  Freeverse's; only the scripts repository is GPL), so it is fetched,
  never shipped. The signed, notarised app: Releases (below).
  Fetching the art from inside the game: Get HD Art (below).
- The field guide (29 Sep 2026): `docs/GUIDE.html` is the owner's
  illustrated edition ("Durandal, by Durandal": the guide annotated by
  the AI, with diagrams), self-contained but for Google Fonts;
  `docs/index.html` redirects to it for GitHub Pages once the repository
  is public (Pages needs a public repository on a free plan).
  `docs/GUIDE.md` is the plain edition that reads on GitHub. Its nine
  images in `docs/images` are frame shots from earlier rounds' captures
  (made with a small CoreGraphics crop tool; a missing image removes its
  own frame, so none is required). Corrections made on placing it, all
  reported to the owner: the HD art paragraph (the fetch script), the
  calendar (the Marathon arrived in 2773, the Pfhor came in 2794), and
  "a thousand years" of S'pht slavery, as the game's first terminal says.
  Keep the two editions' facts in step when a feature changes.
- Round 13 (Rampant, the fifth tier) on `durandal/round-13-rampant`,
  in progress; plan and decisions in `docs/PLAN-fifth-tier.md` (60 fps at
  the 1% low reaching for 120, no memory cap, lifelike; film export last).
  Stored as `quality_tier` 5 (Custom stays 4 for existing files); the menu
  offers it after Flagship while QA is open or once one of its features
  has passed; RAMPANT tab in the dialog. The M5 has hardware ray tracing
  (the Round 5 "no ray tracing hardware" meant none used). 14 of the 28
  solo levels use 5D space (counted from Map.sceA), so rays check against
  the polygon walk; M2 has no Jjaro walls (collections 20, 25 empty).
  - Step 0: `DURANDAL_GPU_TIMING` now also times ambient shadows, bloom,
    the world blit with the 2D ("blit and 2D") and the output pass; a
    world frame's timing ends after the display's output pass
    (`DurandalMetal::TimeDisplayPass`, `EndFrameTiming`).
    `scripts/trace-spike.swift`: the polygon walk against the hardware on
    the same rays, from Map.sceA, no window.
  - R4 Bounced Light (`light_bounce`, Rampant, in QA; needs Light
    Redistribution): a bake ray that hits a surface sees it as it is drawn
    (`bounced`: its light x its lumel against its group's average, the
    draws' clamp) from a copy of the atlas taken before each bake
    (`radiance_previous`; lookup `surface_patch_buffer`, polygon x 10 +
    part -> patch), so gathered light passes on over the frames. Settled
    patches in view are re-baked in turn (`kBounceTiles` 128 tiles a frame
    at 1/16; big patches a slice at a time), so it keeps flowing and a
    room whose lights change catches up. The sky gives off the landscape's
    own average colour. Figures (sprites, HD sprites) take the floor and
    ceiling lumels at their position (`figure_light`, 35% ceiling;
    `Uniforms::figure_patches` from the polygon under the feet).
  - R1 Traced Shadows (`traced_shadows`, Rampant, in QA; needs Dynamic
    Lights and Light Shadows; `RenderMain/DurandalOccluders.*`): each frame
    the monsters, items, scenery and corpses within reach of a light (in
    view or not, up to 128, nearest the viewer; not cloaked, teleporting or
    self-lit) become cards listed in their polygon and its neighbours
    (fragment buffers 7-9). Their frame's opacity, from the 8-bit bitmap, is
    rasterised once into a slice of a 256x256 R8 mask array (512 slices,
    mips built on the CPU, least recently used slice reused; texture 6).
    `light_transmittance` walks as `light_reaches` and in each polygon
    tests its figures between where the segment enters and leaves it: the
    card faces the ray (the viewer's frame, axis (d.y, -d.x), so a light at
    the eye throws the silhouette the viewer sees), sampled at the mip
    that blurs it by the light's disc there (`Light::info.y`, 0.08-0.4 WU
    by strength): sharp at the feet, soft further off. It also checks the
    end height (stacked rooms leaked light; the trace spike found it). A
    figure's own card is skipped (`figure_patches.zw`, its position), and
    its walk starts from the polygon under its feet. `Uniforms::rampant`
    x on, y polygon count. Grates (transparent sides) not yet.
  - R2 Reflecting Liquids (`reflections`, Rampant, in QA; needs Real
    Liquids; `RenderMain/DurandalSurfaces.*`): `liquid_fragment` traces
    the reflected ray (about the ripple normal) through the map
    (`trace_surfaces`, the walk, so 5D-exact; 32 WU reach) and shades what
    it hits from the surface table (fragment buffer 10: entry 0 the sky's
    mapping, then 18 float4 per polygon: floor, ceiling, each edge's upper
    and lower side part: slice, texture origin or x0 and hanging height,
    light), rebuilt each frame. Wall art is a 128x128 RGBA colour array
    (texture 7, 256 slices, mips) made from the 8-bit bitmaps through
    their ramps at full brightness, laid out x across the wall / floor's y;
    the landscape is a texture of its own (texture 8), sampled by
    direction (2^HorizExp repeats, square angular pixels, horizon at the
    middle). Hit light as classic_intensity with the headlight by the
    hit's distance from the viewer; classic fog over the reflected path.
    Mixed in by Fresnel in place of the surface's own colour; a ray that
    meets nothing keeps the old sheen. Not lava; not from below. Not yet:
    refraction (needs a copy of the frame), figures in reflections, HD art
    in reflections (the 8-bit art is used). Floor and sky orientation in
    reflections to be checked by eye.
  - R3 Traced Ambient Shadows (`traced_ambient`, Rampant, in QA; needs
    Ambient Shadows): `ao_fragment` keeps its half-resolution pass,
    per-pixel rotation and the world blit's depth-aware blur, but with the
    switch on each pixel finds its polygon from a 1 WU grid of the level
    (`DurandalLights::BuildGrid`, rebuilt only on a level change; heights
    pick between stacked rooms) and walks four short rays (the AO radius)
    through the map (`trace_surfaces`) and the figure cards
    (`figures_between`; the figures within 12 WU of the viewer are
    gathered too), weighting hits by nearness. A sprite pixel skips its
    own figure's card (`figure_under`). No pixel polygon found: the
    screen-space estimate as before. AO pass bindings: buffer 1 map, 2-3
    grid, 4-6 occluders, texture 1 masks.
  - First look (30 Sep 2026, on screen, L06 and the L01-L03 water film):
    black blobs on walls near the viewer (the owner saw them). Cause: the
    distance image held the vertices' distances blended across each
    triangle, which run long on big surfaces close to the viewer, so the
    traced pass placed pixels behind their walls; screen-space AO only
    compares distances, so it got away with it. With Traced Ambient
    Shadows on (`Uniforms::rampant.w`) world fragments now store their
    own distance (`world_frag` `exact`, from `fogged_frag`) and the liquid
    measures its murk path from its own exact distance; Flagship is
    unchanged (the owner's call whether it gets the fix). A pixel inside
    two polygons at once (5D) takes the one the view ray from the
    viewer's polygon arrives in. Dev views: `DURANDAL_AO_VIEW=2` traced
    surfaces only, 3 figures only, 4 the polygon each pixel is placed in,
    5 why the grid finds none, 6 stored distance against the map's view
    ray. Reflections seen working (goo reflects the wall, L06 tick 300);
    the water film's pool reflects its dark ribbed wall, darker than the
    old sheen: to be judged on the display. `ON_SCREEN=1` and `END_TICK=n`
    for `scripts/looks.sh`.
  - The headroom spent (30 Sep 2026, evening; the owner chose all four):
    - Spikes: traced shadows walked every figure for every light at every
      lit pixel (a firing line of sixteen flashes: world p99 17 ms, worst
      30). Light under 0.03 of a level is not walked; figures' shadows come
      from the four lights nearest the viewer (`Light::info.z`,
      `DURANDAL_FIGURE_LIGHTS`) where over 0.15; the walk stops at the
      light's own polygon first. Stage timing of the passes after the world
      is from their fragment start (vertex start counted their wait).
    - Quality: traced AO 8 rays (nearby figures picked once per pixel);
      Bounced Light bakes up to 512 tiles a frame, refresh 1/8; choosing
      Rampant sets Redistribution Strength Strong, the other named tiers
      Medium (`ApplyTier`, and the dialog's selector follows the tier).
    - Shadows: soft edges from structure with no extra rays
      (`light_transmittance`: at an opening, the margin to its floor or
      lintel and, beside a solid edge, to the jamb, against the light's
      disc radius there, smoothstep; a solid wall near an end where an
      opening begins lets part through); grates (surface table now 26
      float4 per polygon, 18 + e the see-through part; the colour array's
      alpha is the art's opacity); figures shade the fog's light
      (`VolumeParams::figures`, figure-casting lights over 0.1).
    - Water: HD walls in reflections (`decode_hd`: the pack's image, right
      side up, BC7 decoded; colour array 256 px); figure silhouettes are
      RGBA (colour at full brightness, alpha opacity; `Occluder::info.z`
      the figure's light) and reflected rays meet figures about the liquid
      polygon (`reflected_figure`); ripples round waders and splashes
      (`DurandalLights::GatherRipples`, fragment buffer 11, `ripple_slope`);
      refraction as a ratio of the art where the refracted and straight
      rays land, applied to what is drawn below.
    - Air (`RenderMain/DurandalAir.*`): Dust and Embers (`dust_embers`)
      motes made per polygon in view from hashes and world time, lit on
      the CPU (room light x0.3, dynamic lights x1.5), embers rising off
      real lava (media), glowing; `DurandalMetal::DrawMotes` (own
      pipelines, reads the attachments, hidden where the distance is
      nearer; vertex depth remapped as world_vertex, without which none
      showed). Heat Shimmer (`heat_shimmer`): the output pass wavers world
      pixels where the bloom is warm (`OutputParams::heat`). Frame shots
      are taken before the output pass: use
      `DURANDAL_BENCHMARK_OUTPUT_SHOTS=1` to see the shimmer.
      `DURANDAL_AIR_LOG=1|2`.
    - On screen, L06 to tick 1200, everything on: Rampant 177 fps average,
      92.6 at the 1% low; Flagship 226 / 125.
  - Owner's first play (1 Oct 2026): every wall and ceiling shimmered
    like a pool's ceiling, on levels without lava too. Two causes, both
    fixed: Heat Shimmer was gated on warm bloom only, and the HD packs'
    brown walls bloom a little (now each lava surface drawn notes its
    rectangle on screen, `RenderRasterize_Metal::note_hot`, and the output
    pass wavers only there and in the air above, `OutputParams::hot`); and
    Bounced Light's refresh of settled patches blended a fresh 8-ray
    estimate at a fixed 1/8 every pass, so the lumels moved by a few per
    cent each time (now the running average carries on to 4096 samples,
    `kBounceCap`, so each pass moves a patch less). Second look: "looking
    good".
  - Headless pass (1 Oct 2026; the Mac busy with Spotlight, Xcode and
    another app, so only relative numbers): six standard films hidden,
    Flagship against Rampant, by GPU stage. Trimmed: traced AO only within
    10 WU (8 rays to 4 WU, 4 beyond), bake back to 384 tiles a frame (128
    refresh), fog shafts within 8 WU, reflections reach 16 WU; bake and
    fog worst frames down a quarter to a third, AO and the world pass
    hardly (to measure again on a quiet Mac). Bounced Light re-settles a
    room whose smoothed light (0.5 s) moves more than 0.15 from where it
    settled (`resettle_around`, from 32 samples). `DURANDAL_TRACE_VIEW=1`
    (`trace_view_fragment`, run in EndWorld) draws the world by tracing
    from the surface table: on L06 walls, floor and ceiling match the
    drawn frame, so the table's placement is right (the sky not yet seen).
  - Embers halved in brightness and glow (the owner, 1 Oct 2026).
  - Full screen on six films not used before (2 Oct 2026, 1080p 240 Hz,
    first 2 min each, alternating order; `docs/benchmarks/r13-unseen-films.md`):
    L08, L16, L21, L23, L28 and the net game Giant Flaming Pit. Rampant
    111-134 fps average, 62-91 at the 1% low (60 held on all six; the
    worst second 58 once, L28 at 65 s); Flagship 142-189 / 82-133.
    Rampant's extra at the p99: world pass +4-7 ms, traced ambient
    shadows +2.5-3.5 ms, fog +0.5-2 ms; its slow frames are GPU-bound.
  - Reflections a quarter weaker (the owner, 2 Oct 2026: "slightly less
    reflective", ripples kept): the mirror's weight is 0.75 x Fresnel.
  - Ground truths for the guide's update (handed to Claude chat):
    `docs/RAMPANT-GROUND-TRUTH.md`.
  - QA passed (2 Oct 2026): Weapon Takes the Light, the six Rampant
    features and Summon BOBs are released (`Released()` returns true for
    everything; the summon key needs no gate). The new illustrated guide
    (`docs/GUIDE.html`, five tiers, part XII Rampant, XIII the limits)
    was placed with corrections reported to the owner (HD art fetched,
    not shipped; Weapon Takes the Light restored; contents and terminal
    numbering), and `docs/GUIDE.md` brought in step. The owner wants the
    HD packs included when the repository goes public, or failing that a
    one-button in-game download: see `docs/HD_ASSETS.md` section 6 for
    the licence position (CFP art has no grant; needs the maintainer's
    permission to mirror; git cannot hold files over 100 MB anyway).
- Tag `baseline-8` (2 Oct 2026): Rampant, Weapon Takes the Light and
  Summon BOBs released (QA passed), the new guide placed; `durandal/main`
  fast-forwarded to it.
- Optimisation (2 Oct 2026, on `durandal/optimisation`; the owner asked for
  a plan, measured mainly headless). Plan and every number:
  `docs/PLAN-optimisation.md`. Findings:
  - The world shaders are compiled in Metal's safe (IEEE) maths mode
    (Round 2, for OpenGL parity). Relaxed mode roughly halves GPU time per
    frame (Rampant L06 175 -> 311 fps, Flagship 211 -> 358; L28 125 -> 216
    and 155 -> 270) with no visible change on four films (5-D Space
    included). Relaxed is the default since the owner's look on the
    display (2 Oct 2026: "pretty solid 200 fps+ at Rampant");
    `DURANDAL_MATH=safe` puts strict IEEE back for parity work, `fast` is
    fast maths.
  - Three exact fixes (pixel-identical, checked): glow passes with a
    minimum glow of 1 get no lights (`setup_glow`; classic_intensity clamps
    them away), wall and sprite fragments discard transparent texels before
    walking lights (all derivatives and implicit-LOD samples stay above
    the discard), and the weapon in hand skips the light walk its
    `viewer_light` replaces.
  - After those the 1% low is the limit: firefights, GPU-bound, twice the
    usual number of cast lights. Most of the lights' cost is the per-pixel
    loop over all 16, not the shadow walks: per-surface light culling is
    the first job. Compiling Rampant's paths out of the lower tiers and a
    RG11B10 glow image were measured and gave nothing.
  - Bounced Light makes frames differ slightly run to run (0.5/255 on
    L28), and under relaxed maths light redistribution does too (0.13 on
    L06 from tick 750): compare frame shots with `light_bounce=0,
    light_redistribution=0` when a change should be exact.
  - Fix (the owner, 2 Oct 2026: dead monsters cast "a light as a bar in the
    same plane as the sprite", in dark corners): traced ambient shadows'
    `figure_under` took any pixel within 48 units of a card facing the
    camera-to-figure ray, within its width, as the figure's own, so the
    strip of floor and walls at a corpse's depth skipped the corpse's
    occlusion while the rest did not. Figures are drawn square to the
    view's yaw through their position (the sprite transform), so a pixel
    is now a figure's only on that card (16 units), facing the way it
    does, and never a floor or ceiling. Off-centre figures no longer
    shade themselves either. `DURANDAL_AO_VIEW=7` blacks out the pixels
    taken as a figure's own.
  - Step 2a, light culling: `RenderRasterize_Metal::draw` tests each
    frame light's sphere and each contact-shadow caster's reach against the
    polygon's bounds (+32 units: lighting is taken at texel centres) and
    sets `Uniforms::culling` (x lights, y casters, bits; all set for draws
    that skip draw(), e.g. 3D pickups); `dynamic_light_with`,
    `contact_shadow` and the liquid glints loop over the set bits with
    `ctz`, lowest first as before, so results are bit-identical. Firefight
    (L06 0-300): Enhanced 340 -> 392 fps, Flagship 228 -> 274, Rampant
    197 -> 211; world p99 down 1.4-1.8 ms.
  - Skies (the owner, 2 Oct 2026: "they wrap and sway"): the flat
    landscape mapping (`landscape_uv`, a port of the GL shader written for
    the sheared view) took the eye-space direction, which with True Look
    holds the pitch and the Sidestep Sway roll, and subtracted the pitch
    again, so the sky turned and leaned with the head. `level_eye` now
    takes the roll and then the pitch off (Rasterizer_Metal's order: base,
    roll about forward, pitch about side, yaw), and the cylinder is
    written with atan2 and the horizontal length so it runs on through
    the zenith; with pitch and roll 0 (no True Look) it is the old
    formula. Wrap: a landscape without vertical repeat is one mirrored-
    repeat period around the horizon (v from floor(offsety); M2's
    defaults with the CFP 4096x2160 skies: about 45 degrees either side),
    and True Look sees past it, where the mirrored copies came round again
    (a second, upside-down planet overhead). `landscape_colour` clamps to
    the period and fades to the edge row's average beyond it (`u.sky` bit
    2, set when `!VertRepeat`). Checked at `DURANDAL_LOOK_OFFSET` 0, 25 and
    55 on 260929-1 tick 450: level unchanged (0.05/255). Sphere landscapes
    were already corrected; reflections' sky is mapped separately.
  - Round closed after 2a (the owner, 2 Oct 2026: "big fights feel as
    smooth as they can be"); steps 2b onwards stay in the plan, unbuilt.
    Next: after a reboot, a full-screen rebench (1080p 240 Hz) on films
    not used before (check `ps` for Spotlight and duetexpertd first). The
    in-game HD art button waits until after that (see
    `docs/HD_ASSETS.md` section 6 and the owner's wish to ship the packs).
  - Tools: `scripts/feature-costs.sh <out> <film> <end-tick> label=settings
    ...` (hidden, off-screen, GPU stage timing; `REPEAT=n` alternates the
    order; settings may start with `ENV=value ...@`, and `APP=<binary>`
    there runs a kept build for A/B), `scripts/feature-costs.py <out>
    [--base <label>] [--md]`. Hidden runs only when the owner says the
    screen is not free.
- Tag `baseline-9` (2 Oct 2026, the owner's word after playing it):
  `durandal/optimisation` fast-forwarded into `durandal/main`: relaxed
  shader maths, the exact light fixes, per-surface light culling, the
  corpse-bar fix in traced ambient shadows and the skies that stay with
  the world. Next: a full-screen rebench after a reboot on films not used
  before, then the in-game HD art button.
- Rebench after a reboot (3 Oct 2026, full screen 1080p 240 Hz, seven
  films not used before: L05, L11, L14, L22, L24, L27, net House of Pain;
  `docs/benchmarks/b9-unseen-films.md`): Rampant 198-276 avg, Flagship
  235-288. L24's Rampant worst second was 35 (world pass up to 140 ms at
  ticks 675-700): an explosion filling the view, its many blended layers
  each walking the four figure-casting lights through ~50 figures listed
  in one polygon. Draws already at full light (`u.color` >= 1: self-lit
  frames, explosions) now get no lights in `RenderRasterize_Metal::draw`
  (exact: the shaders take min(colour + light, 1); not liquids, whose
  glints add), pixel-identical on that stretch; L24 Rampant then 236 avg,
  98 1% low, worst second 93. `DURANDAL_OCCLUDER_LOG=1` prints the
  figures gathered and the longest polygon list per frame. If a crowd
  still spikes, the next lever is capping each polygon's figure list.
  `scripts/diff-shots.swift` compares two directories of frame shots
  (compile with `xcrun swiftc -O ... -o .deps/diff-shots`).
- Tag `baseline-10` (3 Oct 2026, the owner's word; film tests green three
  ways): `durandal/rebench-fixes` fast-forwarded into `durandal/main`:
  the full-light skip and the rebench write-up. Next: the in-game HD art
  button.
- Get HD Art (3 Oct 2026, on `durandal/hd-button`; QA passed the same day,
  so the button shows on every launch, closed only by `DURANDAL_STOCK=1`;
  `docs/HD_ASSETS.md` section 9). ART tab "GET HD ART..." opens a dialog (one
  line per pack, DOWNLOAD and CLOSE, which cancels a running fetch and
  closes once it has stopped). `Misc/DurandalFetch.*` does in the game what
  `scripts/get-hd-art.sh` does: per pack, skip if its folder is in the
  Plugins folder (never overwrite), the Simplici7y `downloads/new`
  redirect (Google Drive pages become the direct download), NSURLSession
  download with progress, `unzip -tq`, SHA-256 against the version tested
  (a newer one installs all the same, and the line says so), `ditto -x
  -k`, the shallowest folder holding Plugin.xml moved in under the pack's
  name. Only what macOS provides (Foundation, CommonCrypto, ditto, unzip);
  needs about three times the download free. New packs join the plugin
  list without a restart (`Plugins::add_directory`, a small upstream
  addition: a full re-enumeration would forget the Environment dialog's
  disabled plugins, applied once at launch), `DurandalArt::Rescan` and
  `Apply` switch them; their art loads from the next level.
  `DURANDAL_PLUGINS_DIR` overrides the folder for tests; a command-line
  harness (compile `DurandalFetch.mm` with `-DDURANDAL_FETCH_HARNESS`)
  fetched CFP Scenery and 3D Items into a scratch folder and installed
  both (3 Oct 2026). The owner's packs are symlinks into `Assets/`, so the
  button can be tried by moving one link out of the Plugins folder.
- Tag `baseline-11` (3 Oct 2026, the owner's word after QA; film tests green
  three ways on the commit before, the release only shows the button):
  `durandal/hd-button` fast-forwarded into `durandal/main`: Get HD Art
  released. Ground truths for the guide since `baseline-8`:
  `docs/GROUND-TRUTH-baseline-11.md` (the button, the new bench figures and
  the illustrated edition's chart data). Neither guide edition has been
  changed since `baseline-8`; bring `docs/GUIDE.md` in step when the
  owner's updated guide arrives.
- Guide F placed (3 Oct 2026, `docs/GUIDE.html`, the owner's update from
  those ground truths; `docs/GUIDE.md` in step). Corrections made on
  placing it, reported to the owner: it was built on the edition before
  the last placing, so the contents' "Five ways", the Weapon Takes the
  Light paragraph and table row, "Fifty-one switches" and part XIII's box
  number (13) were put back; the Get HD Art box numbered 6 (the boxes
  carry their part's number); "the game stays playable" while fetching
  became "while the dialog stays open" (Close cancels); the credits' "keeps
  nothing" became "hosts none of it" (installed packs stay on the Mac).
  Then pushed and the repository made public.
- Guide H placed (3 Oct 2026; a redesign in Marathon's terminal colours,
  with a prologue, a note on rampancy and Tycho's epilogue; switches listed
  once in an appendix; `docs/GUIDE.md` keeps the per-part tables).
  Corrections on placing, reported to the owner: Weapon Takes the Light
  (paragraph, row, "Fifty-one") again; "the game stays playable" while
  fetching and "keeps nothing" again; the S'pht enslaved "a thousand
  years" ago (the game's terminals; "thousands" had crept in at guide E),
  and the calendar's 2773 arrival restored; Summon BOBs allows shallow
  water; memory is at the end of a run, not a level; the Get HD Art box's
  empty header filled. Links to the other docs point at GitHub (Pages
  would serve the bare .md files raw). GitHub Pages on since 3 Oct 2026:
  `durandal/main`, `/docs`, `docs/.nojekyll` (served as-is, no Jekyll),
  https://sebcarley.github.io/durandal/ (`docs/index.html` redirects to
  the guide).
- Release 0.1.0 (3 Oct 2026, the owner's ask, "as for Quake"): GitHub
  Release `v0.1.0` on `sebcarley/durandal`
  (https://github.com/sebcarley/durandal/releases/latest), the app built
  from `ec2147d0`, Developer ID signed (team A4MCX56UAS), notarised
  (submission d24f4934) and stapled; `Durandal-0.1.0.zip` (35 MB, the app
  alone, Marathon 2 data inside) and its SHA-256. `scripts/make-release.sh
  <version>` does it all: build from a clean commit, check the binary links
  only macOS and the data is bundled, sign inside out with the hardened
  runtime and the build's entitlements (microphone), notarise, staple,
  Gatekeeper check, ditto zip, into `.deps/release/<version>/`.
  Notarisation uses the keychain profile the owner stored for the sister
  project (`RELEASE_NOTARY_PROFILE=metalquake`; the script's default name is
  `durandal`, not stored). The first attempt got HTTP 403 (Apple's updated
  developer agreement unaccepted): only the account holder can accept it,
  at developer.apple.com/account; it took about four minutes to reach the
  notary service after. Release notes: `.deps/release/notes-0.1.0.md`. Tested
  on macOS 27 only (deployment target 12.0).
- Trilogy plan (3 Oct 2026, the owner's ask: "the same with Marathon 1 and
  Infinity, to complete the trilogy"): `docs/PLAN-trilogy.md` on
  `durandal/trilogy`. Decided: three apps, Durandal Marathon and Durandal
  Infinity. Open: releases, M1's HD art, the guides. Rounds T0 baseline (targets,
  scripts, the 27 M1 and 32 Infinity films three ways, a first look), T1
  Infinity, T2 Marathon 1, T3 together. The plan's inventory lists every
  M2 assumption found (collection numbers, wall lamps, haze by environment,
  liquids, summon monster, HD packs, Lua HUD and widescreen, skies, app
  identity, scripts).
- Trilogy T0 (3 Oct 2026, on `durandal/trilogy`; the owner chose three
  apps, named Durandal Infinity and Durandal Marathon): the two targets
  renamed and signed to run locally (see Layout), their Info.plists take
  the build's name and minimum macOS as Marathon 2's does, and the scripts
  take `GAME` (`scripts/game.sh`). Film tests green three ways for all three
  games on the first try: Marathon 2 86 assertions, Infinity 66 (32
  films), Marathon 56 (27 films), so Durandal's upstream edits leave
  Marathon 1's converted films and Infinity's in sync. GET HD ART... shows
  only in Marathon 2 (its pack list is Marathon 2's art) until T1/T2 give
  the others theirs (`Scenario::instance()->GetID()`). `setup.sh` still
  builds Marathon 2 alone (T3). Next: the first look on screen (the owner
  plays each at Flagship and Rampant; frame shots of the demo films).
- Play launch (Terminal): `DURANDAL_QA=1
  .deps/play/Durandal.app/Contents/MacOS/Durandal` - a copy of a good
  build (`cp -R` from DerivedData) that rebuilds never touch. Saved
  films land in `~/Library/Application Support/Durandal/Recordings`.
- Test display: MSI, 240 Hz at 1080p, with HDR. Decisions: projectile
  lights on from Enhanced (roadmap E2).
