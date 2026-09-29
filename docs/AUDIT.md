# Durandal — Phase 1 audit

State of Aleph One (upstream `master` at `6497ea3b`, 20 Sep 2026) as built
on this Mac, and what it means for a Metal engine for Marathon 2.
Line references are to this checkout; paths are under `Source_Files/`
unless they start with `Xcode/`, `data/`, `tests/` or `vcpkg/`.

---

## 0. Summary

1. **Two renderers, not three.** There is a software renderer and an
   OpenGL renderer. The OpenGL one always uses the "shader" path; the old
   fixed-function "classic" OpenGL path is ~1,700 lines of dead code. The
   shader path is still *legacy* OpenGL throughout (matrix stack, clip
   planes, `GL_QUADS`, immediate-style arrays, GLSL 1.20). There is no Metal
   code at all.
2. **Metal cuts in cleanly** at `RenderRasterizerClass` for the 3D world.
   Visibility, sorting, object placement and liquid ordering are
   backend-neutral and can be reused as they are. The 2D side (HUD,
   terminals, map, menus, fonts) is spread over about a dozen files and
   needs its own small drawing interface.
3. **Marathon "lights" are dimmer circuits, not lamps.** A light is a
   brightness-over-time function with no position, colour or direction.
   Surfaces subscribe to one by index. What you see on a wall is its
   subscribed brightness, combined with the player's headlight by a fixed
   formula and quantised through 32–256 shading tables.
4. **Marathon 2's walls have no self-luminous palette entries.** I checked
   the Shapes file directly: walls, scenery, landscapes, effects and weapons
   have none. Only monster colour tables do. Glowing screens and lamps in M2
   are made by giving those wall surfaces bright lights. So "emissive from
   palette entries" gives us monsters' eyes and armour lights, but not
   screens. Screens need a derived or curated emissive mask (see
   ROADMAP.md).
5. **Frame interpolation is real but off by default.** The engine
   interpolates objects, platforms, camera and weapon sprites between its
   30.3 Hz ticks. The stock setting is 30 fps (no interpolation); 60, 120
   and Unlimited are opt-in. A headless run of our new benchmark on the L01
   demo presented 104,510 frames, **all distinct world states — no repeats**.
   Liquids, lights, fades, texture animation and the HUD still step at
   30 Hz.
6. **Mouse look is sampled once per tick (30 Hz)** and then interpolated.
   That adds up to about 33 ms plus up to one tick of display lag. It is the
   main thing that will make 120 fps feel less than 120 fps. It can be
   fixed on the render side without touching game state.
7. **There is already a gameplay-integrity gate.** The upstream test target
   replays 43 Marathon 2 films and checks each one ends on its recorded
   random seed. It passes on this build (86/86 assertions, ~72 s, headless).
   Every enhancement must keep it green.
8. **Performance baseline: not measured yet** — it needs the game running on
   your display (section 8). Two display facts matter already. Both
   connected monitors and the built-in screen are running at **60 Hz**. And
   this is a **fanless MacBook Air** (base M5, 8-core GPU, 16 GB).

---

## 1. Where Phase 0 left us

| Item | State |
|---|---|
| Source | Aleph One `master` `6497ea3b`, working branch `durandal/phase-0` (commit `6310b2e5`) |
| Data | Marathon 2 from the `data-marathon-2` submodule: Map, Shapes, Sounds, Images, Music, Physics, 3 demo films, 5 plugins. Complete; no fallback download needed. |
| Dependencies | vcpkg `2026-07-27` in `.deps/vcpkg`, arm64 triplet, 103 ports, 7.7 min. vcpkg fetched its own CMake; nothing installed system-wide. SDL 2.32.8, OpenAL Soft 1.23.1, Boost 1.88. |
| Project | `Xcode/AlephOne.xcodeproj` (the README's `PBProjects/` no longer exists). Scheme **Marathon 2** builds `Classic Marathon 2.app` with the M2 data bundled in `Contents/Resources/DataFiles`. |
| Changes needed | Deployment target 10.13/11.0 → 12.0 (Xcode 27's floor). Signing moved from Aleph One's team to "Sign to Run Locally". Run action set to Release. |
| Result | arm64, ad-hoc signed, hardened runtime, 66 MB, all dependencies static. |
| Tests | `scripts/test-films.sh`: 43 films replayed, 86/86 assertions pass. |
| Runtime folders | `~/Library/Preferences/Marathon 2/`, `~/Library/Application Support/Marathon 2/`. The name comes from the localised bundle name. A stock Classic Marathon 2 would share these folders. |

---

## 2. The renderer as it stands

### 2.1 Which renderers exist

- The graphics preference offers **Software** and **OpenGL**
  (`Misc/preferences.cpp:1029-1030`). They are stored as `_no_acceleration`
  and `_opengl_acceleration` (`RenderOther/screen.h:149-153`).
- When OpenGL is active, `render_view` always uses `Rasterizer_Shader` and
  `Render_Shader` (`RenderMain/render.cpp:469-470, 489`).
  - `Rasterizer_OGL_Class` is declared but never selected.
  - `OGL_Render.cpp:1178-2910` (`RenderAsRealWall`, `OGL_RenderSprite`,
    legacy models and so on) is unreachable.
- The shader path is legacy GL. It uses the matrix stack, `glClipPlane`,
  `GL_QUADS`/`GL_POLYGON`, client-side arrays, ARB shader objects, `gl_Fog`
  inside GLSL, display-list fonts and `glAlphaFunc`.
  - No context profile is requested (`RenderOther/screen.cpp:893-906`), so
    macOS provides a 2.1 compatibility context.
  - On Apple Silicon, that GL is itself a translation layer over Metal.
- The OpenGL options live in `OGL_ConfigureData` (`RenderMain/OGL_Setup.h:230-260`).
  - `Flags` holds the `OGL_Flag_*` bits at 211-228. Only bit `0x8000` is free.
  - The Marathon 2 first-run file (`data/Scenarios/Marathon 2/Scripts/Default Preferences.xml`)
    sets `ogl_flags="19073"`: Z-buffer, fader, map, HUD and "mimic software".
    Fog, models, bloom and bump are off, and filtering is nearest-neighbour.

### 2.2 One frame, end to end

1. `main_event_loop` (`shell.cpp:712`) calls `idle_game_state`
   (`Misc/interface.cpp:1028`).
   - That runs any due ticks through `update_world`
     (`GameWorld/marathon2.cpp:467`).
   - It then calls `render_screen` if the tick or the interpolation
     fraction has changed (`interface.cpp:1140-1170`).
2. `render_screen` (`RenderOther/screen.cpp:1270-1594`):
   - updates the camera and interpolated world (1236, 1281, 1415);
   - lays out the view, map and terminal rectangles;
   - sets the GL viewport and calls `render_view` (1444);
   - then draws crosshairs, overlays, the HUD (classic or Lua) and the
     terminal blit;
   - then `OGL_SwapBuffers`, which calls `SDL_GL_SwapWindow` (1589).
3. `render_view` (`RenderMain/render.cpp:420-515`):
   - **Visibility.** `RenderVisTreeClass::build_render_tree`
     (`RenderVisTree.cpp:120`) casts 2D rays through the polygon portal
     graph and builds a tree of visible polygons with clipping windows.
   - **Sorting.** `RenderSortPolyClass::sort_render_tree`
     (`RenderSortPoly.cpp:91`) orders them back to front.
   - **Objects.** `RenderPlaceObjsClass::build_render_object_list`
     (`RenderPlaceObjs.cpp:122`) places sprites into the sorted tree.
   - **Drawing.** Rasteriser `SetView`, then `Begin`, then
     `RenPtr->render_tree()`, then the weapon in hand, then `End`
     (482-506). The automap is drawn after that.
4. `RenderRasterizerClass::render_node` (`RenderRasterize.cpp:87-342`)
   emits each window in this order: ceiling, sides, floor, liquid, objects.
   - `RenderRasterize_Shader` overrides the node, floor/ceiling, side and
     object methods.
   - It never calls the thin `RasterizerClass::texture_*` interface; that
     is used only by software.

Frame pacing is a coarse sleep, not display-linked. With a target set, the
loop sleeps 1 ms whenever more than a third of the frame budget remains
(`shell.cpp:793-808`). Vsync is `SDL_GL_SetSwapInterval`
(`screen.cpp:770-773, 906`).

### 2.3 How each world element reaches the screen (OpenGL path)

| Element | How it is drawn |
|---|---|
| Floors / ceilings | World-space polygon (`GL_POLYGON`). UV = (vertex + origin + slide) / WORLD_ONE, ×2/×4 for the 2x/4x modes. The light intensity is the vertex colour. Optional glow pass. `RenderRasterize_Shader.cpp:663-772` |
| Sides (walls) | `render_node` splits each line into full / high / low / split parts plus an optional transparent layer (`RenderRasterize.cpp:222-285`). Each part is a `GL_QUADS` trapezoid (`RenderRasterize_Shader.cpp:774-898`). |
| Portal clipping | The window's left and right edges become `GL_CLIP_PLANE0/1`. Top and bottom are left to the depth buffer (`RenderRasterize_Shader.cpp:247-277`). |
| Liquids | Height = low + (high − low) × *a light's intensity* (`GameWorld/media.cpp:262`). With "see-through liquids" on (the engine default), the order is far-side objects, then the surface drawn as a floor or ceiling, then near-side objects. Objects are cut at the surface with clip plane 5 (`RenderRasterize.cpp:146-184, 300-341`; `RenderRasterize_Shader.cpp:1093-1115`). The underwater tint is a full-screen fader quad; underwater fog is a separate fog set. |
| Landscapes | Walls and ceilings with `_xfer_landscape` use a per-fragment projection from the view direction: flat cylinder or equirectangular sphere (`Shaders/landscape*.frag`). Always full bright. |
| Sprites | The view is picked from 1, 4, 5 or 8 angles by `get_object_shape_and_transfer_mode` (`GameWorld/map.cpp:1012`). Drawn as a world-space billboard (`RenderRasterize_Shader.cpp:1118-1241`). **Depth testing is off unless forced**; order comes from the portal sort. Lit by its polygon's floor light and the headlight. |
| Weapon in hand | Screen-space quads with depth off (`RenderRasterize_Shader.cpp:1247-1443`). 3D weapon models are hard-disabled in this path (`:1267`). |
| HUD | Classic: panel picture plus shapes and text scaled by `glScaled(w/640, h/160)` (`RenderOther/HUDRenderer_OGL.cpp:64-100`). Lua HUD: immediate GL with stencil masks. |
| Terminals | Drawn in software into a fixed **640×320** surface, then uploaded and stretched (`screen.cpp:1097, 1519-1529`). |
| Automap | Immediate GL (`RenderOther/OverheadMap_OGL.cpp`). |
| Menus, chapter screens, movies | Fixed **640×480** surfaces through `OGL_Blitter` (`screen.cpp:188, 2089-2112`). |
| Fades (damage, pickups, underwater) | Full-view quads with fixed-function blends (`RenderMain/OGL_Faders.cpp:84-210`). They advance per tick (`RenderOther/fades.cpp:285`), so they step at 30 Hz. |

### 2.4 Transfer modes (28 `_xfer_*`, `GameWorld/map.h:389-420`)

| Mode | Shader path |
|---|---|
| normal | wall or sprite shader |
| invisibility / subtle invisibility | `S_Invisible`: black at reduced alpha |
| static variants (50%, fade-out, pulsating) | `S_Invincible`: per-block hash noise at roughly 320-line granularity |
| fold in / out | static plus horizontal shrink |
| fade out to black | disabled (`#if 0`, `RenderRasterize_Shader.cpp:829-845`); drawn as textured |
| pulsate, wobble, fast wobble | UV perturbation uniforms |
| slides (8 kinds), wander | UV offset (`RenderRasterize_Shader.cpp:547-593`) |
| landscape, big landscape | landscape shaders |
| 2x, 4x | UV scale |
| smear | not implemented in the shader path |

### 2.5 Textures and shaders

**Textures**
- `shapes.cpp` loads collections, colour tables and shading tables
  (`build_shading_tables8/16/32`, 1754-1921).
- `TextureManager` (`OGL_Textures.cpp:481-1530`) expands 8-bit indices
  through the *brightest* shading table into RGBA.
- It finds glow texels by comparing against the *darkest* table
  (`FindColorTables`, 882-973) and splits them into a separate glow layer.
- Replacement PNG/DDS textures, glow images and normal maps come in through
  MML (`OGL_Subst_Texture_Def.cpp`).

**Shaders**
- There are 22 GLSL programs (`OGL_Shader.cpp:90-114`). Most live in
  `RenderMain/Shaders/*.vert|frag` (861 lines); gamma, blur, bloom and error
  are inline strings.
- MML can replace any of them by name.

**Lighting maths in `wall.frag` / `sprite.frag`**
1. `ml = clamp(selfLum + flare − z/8192, 0, 1)`. This is the headlight:
   `selfLum` is the player's natural light and `flare` is the muzzle flash.
2. `I = ambient > ml ? ambient + ml/2 : ml + ambient/2`. This is the same
   combination as the software renderer.
3. Clamp `I` to [glow, 1], then apply linear, exp or exp² fog.

This is continuous, not banded: the shader renderer does not use the real
shading tables.

**Other shaders**
- Bloom renders the whole tree a second time into a blur pass.
- `bump.frag` does 4-step parallax on normal-map alpha, lit only by an N·V
  camera term. It is the only directional lighting in the engine.

### 2.6 Presentation

- Everything is set up in `change_screen_mode` (`screen.cpp:831-1131`).
- The window is SDL2: `SDL_WINDOW_FULLSCREEN_DESKTOP` or windowed, with
  `SDL_WINDOW_ALLOW_HIGHDPI` when "high DPI" is on.
- The context is 8-bit RGB, 24-bit depth and 8-bit stencil, with optional
  MSAA (no UI for it). Several fallbacks drop to software.
- The world renders into an FBO sized to view × pixel scale. It is
  gamma-corrected and blitted to the back buffer (`Rasterizer_Shader.cpp:52-161`).
- If the void colour is off, the previous frame is drawn first ("smear the
  void").
- Screenshots and movie export use `glReadPixels`.
- SDL 2.32.8 provides `SDL_Metal_CreateView` / `SDL_Metal_GetLayer`, so a
  `CAMetalLayer` can be hosted in the existing SDL window. Input, audio and
  windowing stay on SDL.

### 2.7 How much GL there is, and where Metal cuts in

- About **950 lines with direct `gl*` calls across 25 files**:
  - ~690 in `RenderMain`, including 150 in dead code;
  - ~260 elsewhere: `OverheadMap_OGL`, `HUDRenderer_Lua`, `screen.cpp`,
    `FontHandler`, `HUDRenderer_OGL`, `OGL_Blitter`, `Shape_Blitter`,
    `OGL_LoadScreen`, `shell.cpp` screenshots and `Movie.cpp` export.
- Another ~50 files branch on `OGL_IsActive()`.
- GL-specific code totals ~13,750 lines.
- For scale: `RenderMain` is 25k lines, `RenderOther` 22k, `ModelView` 3.6k.

**Seams**
1. **World:** `RenderRasterizerClass` (`RenderRasterize.h:86-144`). Its 7
   virtuals are the whole contract the shader path uses.
2. **2D:**
   - `HUD_Class` (`HUDRenderer.h:223-239`)
   - `OverheadMapClass` (`OverheadMapRenderer.h:181-226`)
   - `Image_Blitter` / `OGL_Blitter`
   - `Shape_Blitter`
   - `FontSpecifier::OGL_Render`
   - the `OGL_RenderRect` / `TexturedRect` / `Frame` / `Lines` / `Text` helpers
   - `OGL_DoFades`
3. **Presentation:** `change_screen_mode`, the `MainScreen*` functions and
   `OGL_IsActive`.

**Options**
- **A — `RenderRasterize_Metal` behind the existing interface, plus a thin
  `Renderer2D`. Recommended.**
  - Reuses the portal visibility, sort and placement code unchanged, so
    output order and every quirk match.
  - Ships behind a switch next to GL for A/B comparison.
  - Draw counts are in the low thousands per frame, which is fine on Apple
    Silicon with a ring buffer and a texture cache.
- **B — a retained scene renderer built from map data**, using depth
  buffering instead of portal order.
  - More modern, but Marathon's overlapping "5D" polygons, transparent
    sides, liquid ordering and void smearing depend on portal-clipped
    painter's order.
  - High risk of visible differences.
- **C — a render-hardware interface with GL-core and Metal back ends.**
  - Keeps other platforms alive.
  - The largest up-front refactor, and cross-platform support is not a
    goal here.

**Recommendation: A**, with one addition. Alongside the rasteriser, keep a
**level-geometry mirror** built from map data (static triangles plus
refitted platform and liquid geometry). Ray tracing, GI, liquids and audio
use it; the rasteriser doesn't. That gives B's benefits where they matter
without changing what is drawn.

**Porting hazards**
- `GL_POLYGON`/`GL_QUADS` → triangle lists.
- Clip planes → `[[clip_distance]]`.
- `gl_Fog` and the matrix stack → uniforms.
- Alpha test → `discard`.
- Depth range [−1,1] → [0,1]; this affects the depth nudges.
- `GL_TEXTURE_RECTANGLE` → pixel-coordinate samplers.
- sRGB toggling → sRGB texture views.
- Void smear → a load action.
- Stencil HUD masks.
- Display-list fonts → a glyph atlas.

---

## 3. How Marathon lighting actually works

### 3.1 Lights are dimmer circuits

- A light is `static_light_data` / `light_data`
  (`GameWorld/lightsource.h:71-125`).
- It has six state slots: becoming active, primary active, secondary
  active, becoming inactive, primary inactive and secondary inactive.
- Each slot has a function, period ± delta and intensity ± delta, in 16.16
  fixed point.
- The six functions are constant, linear, smooth (cosine), flicker, random
  and fluorescent (`lightsource.cpp:430-498`).
  - Flicker, random and fluorescent draw from `global_random()`, the
    network-synchronised game RNG.
  - So **renderers must read `light->intensity` and never evaluate light
    functions themselves.**
- Lights update first in each tick (`marathon2.cpp:412`).
- They are switched by:
  - control panels and projectiles (`devices.cpp:503-795`);
  - polygon triggers (`marathon2.cpp:769`);
  - platforms (`platforms.cpp:707-724`);
  - terminals (`computer_interface.cpp:2010`);
  - Lua (`lua_map.cpp`).
- Lights also drive non-visual things:
  - liquid heights (`media.cpp:262`);
  - ambient sound volume (`map.cpp:2702`);
  - whether a lighted switch works — above 0.75 intensity
    (`devices.cpp:883-890`).

### 3.2 What each surface is lit by

| Surface | Light |
|---|---|
| Floor | polygon `floor_lightsource_index` |
| Ceiling | polygon `ceiling_lightsource_index` |
| Wall — full side, high side, low side, top of a split side | side `primary_lightsource_index` |
| Wall — bottom of a split side | side `secondary_lightsource_index` |
| Transparent layer (grates, glass) | side `transparent_lightsource_index` |
| Liquid surface | polygon `media_lightsource_index` |
| Sprite | the *floor* light of the polygon it stands in, raised to at least the frame's `minimum_light_intensity` (`RenderPlaceObjs.cpp:139-146, 394`) |
| Weapon in hand | floor light of the camera's polygon (`render.cpp:1068`) |
| Landscape | always full bright |

Sides also carry an `ambient_delta` that is added before clamping
(`RenderRasterize.h:70`). In the shipped data it is mostly +0.25, on 359
sides.

The counts in the next paragraph come from a one-off parse of `Map.sceA`
(scratch script, not in the repo): M2 has 688 lights across 41 levels, and
67% of sides use a different light from their own polygon's floor and
ceiling. In other words, the designers lit walls independently of the room.
That is the art direction.

### 3.3 Shading tables and the headlight

**The formula** (`calculate_shading_table`, `RenderMain/scottish_textures.cpp:146-171`)
- headlight `S = clamp(maximum_depth_intensity − depth×8/WORLD_ONE, 0, 1)`;
- surface light `A`;
- **index = max(A,S) + min(A,S)/2**, scaled to the table count and capped.

**The headlight**
- It is the player's natural light, 0.5 by default (`player.cpp:338`), so
  it fades to nothing about 4 world units ahead.
- Firing adds a short flash: pistol 0.75 over ⅛ s, rocket launcher 1.0 over
  ⅓ s (`weapons.cpp:649-663`; `weapon_definitions.h`).
- **That flash only brightens the shooter's own view.** It is not a light
  in the world.

**Table counts:** 32 (8-bit), 64 (16-bit) or 256 (32-bit) per colour table
(`shapes.cpp:1508-1531`). The classic look is those bands.

**How often shade is evaluated in software:** once per wall column, once
per floor scanline, and once per sprite at its centre depth.

**The shader renderer** reproduces the same formula continuously per pixel
(section 2.5). It does not band and it ignores the actual tables.

### 3.4 Self-luminous colours

- A colour-table entry flagged `SELF_LUMINESCENT_COLOR_FLAG` (0x80,
  `RenderMain/collection_definition.h:159`) gets a **50% brightness floor**,
  not full brightness: `multiplier = N/2 + level/2` (`shapes.cpp:1861`).
- The GL path recovers such texels as a glow layer.

I checked the shipped Shapes file directly. Self-luminous entries per
colour table:

| Collection | Self-luminous entries |
|---|---|
| walls 1/2/3/5, scenery 1/2/3/5, landscapes 1–4 | **0** |
| effects (rocket), weapons in hand, items, player, interface | **0** |
| juggernaut | 42 of 67 |
| hunter | 13 / 13 / 26 |
| fighter, enforcer, cyborg | 13–14 per table |
| defender | 13 |
| hummer | 13–26 |
| yeti | table 2 only (27) |
| civilian | table 3 only (14) |

Glow in M2 therefore comes from three sources:
- **Lights assigned to surfaces**: screens, lamps and panels sit on
  brightly lit sides.
- **`minimum_light_intensity` per sprite frame**. 123 of 128 effect frames
  have it, 70 of them at full; lava scenery and Pfhor scenery have it too;
  so do the weapon muzzle-flash frames.
- **Monster self-luminous colours.**

### 3.5 What can be a light emitter, derived faithfully

| Candidate | Source in data | Faithful? | Notes |
|---|---|---|---|
| Monster glow (eyes, armour lights) | self-luminous colour entries | Yes | Per-texel emissive mask straight from the colour table. |
| Projectiles, explosions, lava/Pfhor scenery, muzzle flashes | per-frame `minimum_light_intensity` and frame colour | Mostly | Short-lived point lights at the object. They light the *world* around them, which the original never did — keep behind a switch. |
| Bright wall fixtures (screens, lamps, panels) | the side's light is bright and used only by sides, or its peak is well above its room's ceiling light | Needs care | Treat the surface as an area light scaled by the live light intensity. Only the bright parts of the texture should emit. There are no palette flags, so this needs a derived mask (luminance and saturation threshold per bitmap, reviewed by eye). About 150 wall bitmaps in total, so a curated set is feasible. |
| Bright ceilings | ceiling light in the top band and above its neighbours | Plausible | Area lights. Risk of over-lighting; must be anchored (3.6). |
| Lava / goo | the liquid's light | Enhancement | Liquids never emit in the original. `media.minimum_light_intensity` is stored but unused. |
| Sky | landscape surfaces (376 sides, 1,651 floor/ceiling surfaces in M2) | Yes | Rays escaping through a landscape surface sample the landscape as sky light. |

### 3.6 Keeping modern light anchored to the original

**The fundamental mismatch.** Marathon stores *apparent brightness per
surface*. Physically based lighting derives brightness from emitters.
Replace one with the other and every room changes, as the path-traced Doom
ports showed. Their sector lighting was ignored, and the atmosphere went with
it.

**The faithful approach is redistribution, not re-exposure:**
1. Compute each surface's original brightness exactly as now:
   `B = clamp(I_light(t) + ambient_delta, 0, 1)`, combined with the
   headlight.
2. Compute ray-traced irradiance `E(x)` (direct light, shadows and one
   bounce) from the emitter set above.
3. Shade with `B × clamp(E(x) / Ē_surface, 1−α, 1+β)`.
   - Each surface's *average* stays exactly where the designers put it.
   - Ray tracing only moves light around *within* the surface: falloff
     near fixtures, contact shadows, occlusion.
   - α and β are the "how modern" dial; 0 is stock.
4. Colour bleed from bounce is a separate, capped term.

This keeps level readability, dark-room design and gameplay-relevant
brightness intact. It also keeps the lighted-switch rule, which reads the
raw light, not pixels.

### 3.7 Geometry for ray tracing

**Scale**
- World units: `WORLD_ONE = 1024`; coordinates are `int16`, so a map spans
  about ±32 units.
- Polygons are convex with up to 8 vertices and flat floors and ceilings.
- Walls are vertical quads.
- A level extrudes to about **3,000 triangles**, which is trivial for a BVH.

**Per-tick changes**
- Platforms move (543 in M2, typically 16–34 units per tick).
- Liquid heights follow their light.
- Lua can change heights and textures.
- So: a static BVH plus a refit of platform polygons, their sides and
  liquid planes each frame. Use the *interpolated* heights, to match the
  rasterised frame (`interpolated_world.cpp:504-515`).

**Sprites** are alpha-tested cards.

**The catch: "5D space".** Marathon polygons may overlap in plan; they are
only consistent through portal adjacency. A single global BVH will let rays
hit geometry that isn't actually connected. Two ways round it:
- trace by walking the portal graph (as the engine's own
  `line_is_obstructed` does);
- or validate BVH hits against the polygon adjacency, keeping a BVH per
  connected region.

How many M2 solo levels rely on this is not yet counted; net maps such as "5D Space" use it deliberately. Counting it is a small Phase 2 task.

---

## 4. Tick rate, interpolation and feel

**Tick rate**
- `TICKS_PER_SECOND 30` (`GameWorld/map.h:66`).
- The timer period is integer milliseconds (33 ms, `vbl.cpp:1441`), so the
  real rate is **30.3 Hz**. Measured: 30.30 ticks/s.

**Interpolation** (`GameWorld/interpolated_world.cpp`, added upstream in
2021)
- After each tick, it snapshots the previous and current state of:
  - object positions;
  - platform floor and ceiling heights and texture offsets;
  - line heights;
  - the camera (position, yaw, pitch, fine aim);
  - the headlight;
  - weapon-sprite positions and shell casings;
  - contrails.
- At render time it writes interpolated values into the live arrays, and
  restores the true values before the next tick.
- The fraction is quantised to 1/(target÷30). At 120 fps that is quarter
  steps; Unlimited is continuous.

**Still stepping at 30 Hz**
- liquid heights;
- light intensities;
- texture and sprite animation frames;
- sprite facing;
- fades and flashes;
- HUD and motion sensor;
- objects created this tick.

At 120 fps, liquids and flicker visibly step.

**Default**
- `fps_target = 30` (`Misc/preferences.cpp:4160`), and the M2 first-run
  file doesn't change it.
- **Out of the box there is no interpolation**; the player must choose
  "60 / 120 / Unlimited (interpolated)" in Graphics.

**Verified: real frames, not repeats.** A headless benchmark of the L01
demo, uncapped:
- 104,510 frames over 104 s;
- every frame showed a distinct (tick, fraction) world state;
- the simulation held 30.30 ticks/s.

The GPU numbers from that run mean nothing, because it used the software
path with a dummy display.

**Input**
- `mouse_idle` runs once per timer batch (`Misc/vbl.cpp:1463-1476`), so
  look is sampled at 30 Hz, folded into the tick's action flags and then
  interpolated.
- Worst case, a mouse movement reaches the screen about 33 ms late plus up
  to one tick of interpolation lag.
- Events are polled every 16 ms in game (`shell.cpp:718-725`).
- **Fix, render side only:** add the not-yet-consumed mouse delta to the
  view angles every frame, and poll every frame. Aim still enters the
  simulation per tick, so films and net play stay deterministic.
  - This is the usual way high-refresh source ports make a fixed-tick
    engine feel native.

**Controller:** linear response with a dead zone, applied per tick
(`Input/joystick_sdl.cpp:168-212`).

---

## 5. Preferences, MML and plugins — where our settings live

**Preferences file and format**
- The file is `~/Library/Preferences/Marathon 2/Marathon 2 Preferences`:
  XML, `<mara_prefs>` with `graphics`, `player`, `input`, `sound`,
  `network` and `environment` children.
- Unknown elements are ignored and not re-written.

**Adding a preference** takes five edits:
1. the struct;
2. the default;
3. validate;
4. write (`*_preferences_tree`);
5. read (`parse_*`).

**Where it's written:** `write_preferences()` runs at every start-up
(`shell.cpp:545`), so command-line switches such as `-w` persist.

**Dialogs**
- All dialogs are SDL widgets.
- `Graphics → Advanced` is `SdlOpenGLDialog`
  (`Misc/preference_dialogs.cpp:298`), a tabbed dialog with bindable
  preferences.

**MML**
- MML is XML dispatched to a hard-coded list of `parse_mml_*` functions
  (`XML/XML_MakeRoot.cpp:62-158`).
- It is re-parsed on every level load: reset, then base, then plugins, then
  level scripts.
- Rendering-only sections: `opengl` (textures, models, shaders, fog),
  `landscapes`, `infravision`, `faders`, `view`, `overhead_map`,
  `interface`.
- Game-state sections (these break films): platforms, liquids, weapons,
  items, monsters, scenery, player, dynamic limits, `map_patch`.

**Marathon 2 plugins**

| Plugin | On by default? | What it is |
|---|---|---|
| Marathon 2 Theme | yes | menu skin |
| Marathon 2 Stats | yes | stats upload, only if allowed |
| Basic HUD | no | Lua HUD |
| Enhanced HUD | no | Lua port of the Xbox Live Arcade HUD, plus 90° FOV |
| Transparent Liquids | no | liquid opacity |

**Recommendation**
- **Engine features are hard-coded and gated by preferences.** Put them in
  a new `durandal_preferences_data` struct, stored as its own
  `<durandal schema="1" …>` element in the same file. It stays out of
  upstream's `ogl_flags`, which has only one free bit, and has its own
  schema number for migration.
- **One control sets the tier.** A single `quality_tier` (Stock …
  Flagship) plus one toggle per feature. `apply_quality_tier()` maps the
  tier onto our toggles and the existing knobs: frame-rate target,
  anisotropy, bloom and so on. Touching any individual toggle flips the
  tier to Custom.
- **UI:** a new "Enhancements" tab in Graphics → Advanced, or a top-level
  button. Hidden in Release builds until you've done QA.
- **Assets** (emissive masks, material maps, per-texture parameters) ship
  as an auto-enabled **plugin** that uses only rendering MML sections.
- **Give the app its own name** (localised `CFBundleName` and bundle ID).
  Otherwise we share folders with a stock install, and a stock build
  rewriting the preferences would silently drop our `<durandal>` element.

---

## 6. Films as a repeatable benchmark

**Format** (`Misc/vbl.cpp`, `vbl_definitions.h`)
- A header holding the seed, level, map checksum and film version, then
  run-length-encoded action flags per player.
- An optional embedded save.
- Playback feeds the recorded flags into the same fixed-point simulation.
- The film version selects a `FilmProfile` of behaviour quirks.

**Unattended mode.** `-l/--replay-directory` suppresses menus and chapter
screens and quits when the film ends (`interface.cpp:396, 2859, 3252`).

**What I built** (branch `durandal/phase-1-audit`, command-line only, no
effect on normal play):
- `--benchmark out.csv [--benchmark-size 1920x1080|native] [--benchmark-fps N]`
  plays a film unattended.
- It logs every presented frame (time, tick, fraction) and writes
  `out.csv.summary.txt`:
  - average fps;
  - **worst second**: the fewest frames in any one-second window;
  - 1% low;
  - longest frame;
  - the share of frames between ticks;
  - distinct world states shown.
- Preference overrides are in memory only; `write_preferences()` is a no-op
  while it runs.
- Code: `Misc/DurandalBenchmark.*`, plus one-line hooks in `shell.cpp`,
  `screen.cpp`, `interface.cpp`, `preferences.cpp` and `shell_options.*`.
- The film tests still pass with it in.

**Standard films** (`scripts/benchmark.sh`)

| Name | File | Length | Why |
|---|---|---|---|
| L00-demo | `data/…/Demos/L00.filA` | 3 min 29 s | Shipped demo, Waterloo Waterpark: typical exploration, water. |
| L06-combat | `tests/…/Tooncinator Films/M2 L06 We're Everywhere.46946.filA` | ~3 min 30 s | Single-player combat: projectiles and explosions. |
| net-5D-space | `tests/…/Net Games/2023-08-06 5D space.56028.filA` | ~5 min | 8 players on a 5D map: the worst case for sprites and effects, and for portal edge cases. |

**Running it**
- Each size takes ~12 min, after a 3.5 min untimed warm-up film (for the
  fanless Air).
- Run it plugged in, with other apps closed.

**Timedemo mode (for later)**
- A fixed-step mode would show identical frames on every run, which is
  better for A/B image and performance comparisons between builds.
- Movie export already runs one tick per N frames, independent of the
  clock (`vbl.cpp:1455`, `interpolated_world.cpp:762`, `marathon2.cpp:532`).
  A timedemo can reuse that path.

---

## 7. Sound

**Pipeline**
- OpenAL Soft renders into SDL's audio callback as a loopback device
  (`Sound/OpenALManager.cpp:246-409`), with 23 ms buffers.
- 3D mode is off by default. When on, it uses inverse-distance falloff,
  with HRTF available.
- Ambient and random sounds are always 2D-panned.

**Obstruction today** (`_sound_obstructed_proc`, `GameWorld/map.cpp:2556-2626`)
- A 2D top-down walk along the line from source to listener; height is
  ignored.
- Plus liquid tests.
- It selects a real **EFX low-pass** (`SoundPlayer.cpp:352-363`), smoothed
  over 300 ms.
- Only the filter functions of EFX are loaded. **No reverb exists.**

**Hooks for geometry-driven audio**
- **Reverb:**
  1. Load the EFX effect and slot functions in `OpenALManager::Init`.
  2. When the listener changes polygon, flood through open portals and
     sum volume and surface area.
  3. Estimate RT60 with Sabine's formula, with material hints from the
     texture collection and liquid type.
  4. Set an EAX reverb.
  5. Use an underwater preset when submerged.
- **Graded occlusion:** fork a sound-only version of `line_is_obstructed`
  that interpolates ray height across each crossed line and returns a
  0–1 value, rather than true or false.
- **Constraint:** never change the shared `line_is_obstructed`, because the
  AI uses it.

**Bug spotted**
- The listener velocity is read from unrelated player fields
  (`map.cpp:2546`), so Doppler is slightly wrong.
- The listener is also only updated per tick.

---

## 8. Baseline performance — pending

This needs the game on your display. The runner (`scripts/benchmark.sh
baseline-0`) opens a game window and takes ~28 minutes:
- a 3.5 min warm-up;
- 3 films at 1920×1080 windowed;
- 3 films at native full screen.

Results go to `docs/benchmarks/baseline-0.md`, and I'll fill this section
from them. I'll only start it once you say so.

**What will be measured:** stock renderer settings (the M2 first-run
preferences), uncapped, vsync off, sound off. Average fps, worst-second fps,
1% low and longest frame, per film and per size.

**Display facts that affect the targets**
- **Main display (MSI MAG 272U X24):** 5120×2880 backing ("looks like"
  2560×1440) at **60 Hz**. "Native" full screen will therefore render
  **5120×2880**, about 7× the pixels of 1080p.
- **Built-in panel:** 2560×1664 at 60 Hz, no ProMotion.
- **ASUS VG289:** portrait, 60 Hz.
- 120 Hz presentation can be measured uncapped, but it can't be *seen* on
  any display as currently set up. If the MSI offers a 120 Hz or faster
  mode, it's under System Settings → Displays → Refresh rate.
- Whether EDR/HDR is available depends on that monitor's HDR mode, also in
  System Settings → Displays.

**Settled hardware.** This is a fanless MacBook Air: base M5, 4 super plus
6 efficiency cores, 8-core GPU, 16 GB. Sustained load throttles, which is
why the runner warms up first. The flagship target (120 fps at 1080p) must
be met *after* warm-up, not in the first minute.

---

## 9. Rules that protect films, saves and gameplay

These are also in `CLAUDE.md`.

**Never, from render, audio or enhancement code:**
- call `global_random()` or `set_random_seed()`, or `local_random()`
  (which feeds sound and Lua);
- write game-world arrays, except through the interpolation save/restore
  pattern;
- change physics, definitions, tick order or action-flag encoding;
- ship game-state MML;
- evaluate light functions.

**Instead:** rendering randomness uses its own `GM_Random` instance.

**Before any change lands:**
- `scripts/test-films.sh` must pass.
- Loading a save made before the change must still work.
