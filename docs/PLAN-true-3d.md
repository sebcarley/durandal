# Round 10 — A real camera: true look up/down, roll and sidestep sway

Written 26 Sep 2026 after a request for walls that splay as you look up
and down "like Quake", then a sway when sidestepping. Branch for the
work: `durandal/round-10-camera` from `durandal/round-9-sound` (after
the QA of Round 9 is tagged).

**Status, 26 Sep 2026:** steps 1 to 5 are implemented on the branch
(True Look Up/Down and Sidestep Sway, Feel tab, Enhanced), the film tests
are green both ways, and a third feature was added on request once
the aim limit was found in play: Free Look Beyond Aim (the view pitches past the
physics model's 30 degree aim limit, the crosshair marks the real aim).
See CLAUDE.md for what was built and the checks made. Still to do: a
backward visibility walk for the last stretch to straight up and down,
if wanted, and QA.

## Goal

A real 3D camera: looking up or down rotates the view, so vertical edges
converge as they do in Quake; the view can roll (for sway); everything
else plays exactly like Marathon 2. Rendering only: films, saves and net
play are untouched, and the film tests stay green.

## What we know (from the code, 26 Sep 2026)

Marathon looks up and down by **y-shear**: `view->dtanpitch`
(`render.cpp:634`, world_to_screen_y x tan(pitch)) slides the horizon up
or down the screen and everything is drawn as if the camera were level.
Vertical lines stay vertical; floors stretch. The software renderer needs
it.

Upstream's OpenGL renderer has a "3D Perspective" option
(`OGL_Flag_MimicSW`, 0x4000, in `Get_OGL_ConfigureData().Flags`; **on** in
the owner's prefs, i.e. it mimics the shear). With the flag off:

- `Rasterizer_Shader.cpp:88-121` and our `Rasterizer_Metal.cpp:120-136`
  build a symmetric frustum and rotate the modelview by the pitch
  (`view->mimic_sw_perspective` chooses `yoff` shear or `rotate`).
- `RenderRasterize_Metal.cpp:250` passes `u.pitch` (0 when mimicking)
  and the landscape shader already rotates the sky by it
  (`DurandalMetalShaders.h`, `rotate_pitch` near line 1018).
- `render.cpp:387-390` widens the horizontal cone by a fixed 1.3 for
  OpenGL, a crude allowance for the rotated frustum seeing more at the
  screen corners.

So the **projection is already 3D**. What is still 2.5D is the
visibility and clipping bookkeeping built for the shear, and anything
that assumed it:

- `RenderVisTree.cpp:624-640`: the initial top/bottom clip vectors of the
  screen use `dtanpitch`; `RenderVisTree.cpp:695-751` clips each line's
  vertical extent (y0/y1) with the shear formula. These decide what is
  culled, not where pixels land.
- Per-portal clip planes in the Metal rasteriser (`clip_to_window`,
  `RenderRasterize_Metal.cpp:292-315`) are world-space vertical planes
  from the window's `left`/`right` vectors: valid under any pitch or roll
  (a wall opening's edge is a vertical line; the plane through it and the
  eye is vertical). The object clip planes (`clip_planes[2]`, line 784)
  are horizontal world-space planes: valid too.
- Sprites are quads in view space through the same matrices
  (`render_node_object`, lines 862-866): they rotate correctly.
- The fog volume takes its rays from `inverse(projection x modelview)`
  (`RenderRasterize_Metal.cpp:104`): correct under rotation, verify only.
- Light shadows, redistribution, contact shadows, caustics, relief: world
  space or screen derivatives, unaffected.
- Pitch range: the physics model's `maximum_elevation` limits the
  player's elevation (`physics.cpp:813`); the view's `virtual_pitch` is
  the per-frame look, interpolated (`interpolated_world.cpp:839-845`).
  Aim is elevation, so the limit stays: rendering cannot look further
  than the player aims.
- `view->roll` exists (`render.h:110`) but nothing sets or uses it
  (`Rasterizer_Shader.cpp:125` is commented out).

Other engines did the same move: GZDoom's hardware renderer projects in
true 3D while its software renderer y-shears; Build's Polymost added
"true look up/down" with a depth buffer; Aleph One's own checkbox is the
one above. References at the end.

## Approach: make the camera real, keep the portal renderer

Not a new renderer. The Metal world renderer already draws world-space
geometry through real matrices with per-portal clip planes. A
from-scratch depth-buffered mesh renderer would have to re-solve 5D space
(overlapping polygons that are never visible together z-fight in a plain
depth buffer; the portal walk with clip planes is what makes them work)
and would lose the parity we have. So: keep the portal walk, rotate the
camera, and fix the culling that assumed the shear. Estimate: two to
three sessions, including QA.

## Steps

### 1. The switch: True Look (Feel tab)

- New feature `Durandal::kTrueLook`, tier feature on from Enhanced (Stock
  and Classic keep Marathon's shear: it is the original look). Attr
  `true_look`, label "True Look Up/Down".
- `render.cpp:421`: `view->mimic_sw_perspective = flag && !Enabled(kTrueLook)`
  (Metal display only; the OpenGL path also rotates but is not ours to
  QA).
- Roll: `Rasterizer_Metal.cpp` adds `rotate(m, roll, ...)` about the view
  axis, consistent with `kViewBaseMatrix` (yaw about z, pitch about the
  sideways axis, roll about the forward axis; check the order against
  what the sky shader expects). `RenderRasterize_Metal` passes `u.roll`;
  the landscape shader adds a roll rotation next to `rotate_pitch`.
  Widescreen's Hor+ FOV (`ViewControl.cpp`) must keep working.

### 2. Visibility that contains the rotated frustum (guarded by the flag)

- Horizontal cone (`initialize_view_data`, `render.cpp:386-402`): replace
  the fixed 1.3 by the exact corner yaw. With h = horizontal half FOV,
  v = vertical half FOV, p = |pitch|: the top screen corners' rays,
  rotated by the pitch, have yaw atan(tan h / (cos p - tan v sin p)).
  Use that as `half_cone` (cap at 89 degrees). With roll r, rotate the
  four corner rays by r first and take the widest yaw. The initial
  clipping window is the screen's cone, so this widens what the portal
  walk visits; the GPU viewport clips the extra.
- Vertical planes (`RenderVisTree.cpp:634-635`): the shear encodes the top
  plane's slope as tan v + tan p (half_screen_height + dtanpitch over
  world_to_screen_y). The rotated frustum's top plane slope is
  tan(p + v) and the bottom's tan(p - v), so set
  `dtanpitch_top = world_to_screen_y x (tan(p + v) - tan v)` and
  `dtanpitch_bottom = world_to_screen_y x (tan(p - v) + tan v)` and use
  them in the initial vectors and in the per-line y0/y1 at lines 711 and
  736 (top uses the top value, bottom the bottom value). These only
  decide culling, so exact-or-conservative is enough. Add the roll's
  corner contribution to v the same way as for the cone.
- Keep `dtanpitch` itself for everything else (the shear path and the
  overlays that read it); introduce the two new fields in `view_data`
  with a Durandal comment.

### 3. Objects and overlays

- Sprite quads: already view space. Check `rect.clip_top/clip_bottom`
  (screen space, from the sheared projection) only feed texture
  coordinates and coverage; make sure nothing cuts a sprite by a sheared
  screen row.
- Weapon in hand: a screen overlay, stays fixed to the screen (as Quake).
  Crosshair: centre. Overhead map, HUD, terminals: 2D, untouched.

### 4. Screen-derived effects to verify under pitch and roll

Landscape/sky horizon (`u.pitch`, add `u.roll`); fog volume (matrix
derived, verify the floor mist and lava smoke stay put); liquid waver
(whole-image, fine); relief (derivatives, fine); redistribution and
shadows (world space, fine); the EDR test patch and fps counter (2D).

### 5. Sidestep sway (Feel tab, `Sidestep Sway`, on from Enhanced)

- Quake's roll, from the owner's Quake repo (`darkplaces/common.c:890`,
  `Com_CalcRoll(angles, velocity, cl_rollangle = 2 degrees,
  cl_rollspeed = 200)`): side = velocity . right; if |side| < rollspeed,
  roll = side / rollspeed x rollangle, else roll = rollangle with the
  sign of side. Read that file before writing ours.
- Marathon's sideways velocity: `player->variables.perpendicular_velocity`
  (physics variables; read only, never written). Normalise by the physics
  constants' maximum perpendicular velocity so full-speed sidestep gives
  the full angle; 2 degrees to start, tuned in play. Low-pass it per frame in
  render code (the variables change per tick; the interpolated world
  gives the fraction) so the roll glides.
- New file `RenderMain/DurandalCamera.*`: computes the frame's roll (and
  any later bob) from read-only game state; `render.cpp` sets
  `view->roll` from it when the feature is on. Nothing writes game state:
  the film tests are the gate.

### 6. Tests and QA

- Film tests both ways (`scripts/test-films.sh`, and with
  `DURANDAL_QA=1`): rendering only, must stay green. Ask the owner before
  running them (headless).
- On screen (the owner watches, full screen, main display): a film with lots of
  looking up and down. The owner records one with the QA build (see CLAUDE.md
  for the launch line): a tall room, look at the ceiling and floor, spin,
  sidestep along a wall.
- Parity: with True Look off, frame shots identical to before
  (`scripts/ratio.swift`).
- Missing geometry: look up and down in room corners and through doors;
  with roll. If anything vanishes at the screen edges, the cone or the
  vertical planes are too tight.
- Cost: the wider cone visits more polygons; `scripts/costs.sh` on L06
  and the owner's films, True Look on vs off.

### 7. Later, not this round

- Depth-tested sprites with soft edges where they meet floors and walls
  (roadmap S2): a world depth pre-pass the sprite shader reads.
- Quake-style view bob is Marathon's own camera bob (`scmode_camera_bob`);
  leave it.

## Files to touch

`RenderMain/render.cpp`, `render.h` (view init, the two new dtanpitch
fields, roll), `RenderMain/RenderVisTree.cpp` (cone and vertical clips),
`RenderMain/Rasterizer_Metal.cpp` (roll in the matrices),
`RenderMain/RenderRasterize_Metal.cpp` (`u.roll`),
`RenderMain/DurandalMetalShaders.h` (sky roll),
`Misc/DurandalPreferences.*` (two features, Feel tab, tiers),
`RenderMain/DurandalCamera.*` (new, sway), `CLAUDE.md` (status).

## References

- Aleph One's "3D Perspective" option is the `OGL_Flag_MimicSW` flag in
  its OpenGL settings ([preferences.cpp upstream](https://github.com/Aleph-One-Marathon/alephone/blob/master/Source_Files/Misc/preferences.cpp)).
- Y-shearing and why floors stretch: [ZDoom wiki, Y-shearing](https://zdoom.org/wiki/Y-shearing).
- Build engine's Polymost, "true look up/down" with a depth buffer:
  [EDuke32 wiki, Polymost](https://wiki.eduke32.com/wiki/Polymost),
  [ZDoom wiki, Polymost](https://zdoom.org/wiki/Polymost).
- Quake's sidestep roll: the owner's Quake repo, `common.c:890`
  (`Com_CalcRoll`), cvars `cl_rollangle` 2.0 and `cl_rollspeed` 200 in
  `view.c`. Its code is GPL like ours; borrow the idea and the constants,
  reimplement in our own files.
