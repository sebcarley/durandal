# Round 13 — The fifth tier: working through the limits

Written 30 Sep 2026. The field guide's tiers section ends on Durandal's
note, "There is no fifth tier. I would like one. I have started drawing
it." Part XII of the guide lists what the engine cannot yet do. The ask is
a fifth tier that works through those limits. Branch:
`durandal/round-13-rampant`.

## Decided (30 Sep 2026)

1. **Name: Rampant.**
2. **Order:** step 0 (measure), then R4, then the foundation and R1–R3.
3. **Frame rate: 60 fps at the 1% low, reaching for 120 on average**, at
   1080p on the M5 Air, measured once it has warmed up (it is fanless and
   will throttle; `scripts/soak.sh`). That is 16.7 ms at the 1% low, about
   double what the plan first assumed. The ask is for the look to be
   lifelike and for the M5 to work for it. So the tier spends the budget:
   - full-resolution traced effects where they show;
   - soft shadows from lights that have a size;
   - more rays per pixel.

   It does this before it reaches for upscaling.
4. **Film export: low priority.** Screen recording covers it. It moves to
   the end. Straight up and down is a later round.
5. **Memory: no cap.** The texture table can hold every image at full
   size. The Mac has 16 GB; Metal recommends at most 12.7 GB for the GPU.

## The short answer: AO, DLSS, ray tracing?

- **Ambient occlusion** already ships in Flagship as Ambient Shadows
  (screen space, Round 12). The fifth tier replaces it with occlusion
  traced through the world, so it knows what is off screen (R3 below).
- **DLSS** is NVIDIA only. Apple's counterpart is MetalFX, which has three
  parts, and this M5 supports all three (checked 30 Sep 2026):
  - the temporal upscaler, like DLSS Super Resolution;
  - the denoised upscaler, like Ray Reconstruction;
  - the frame interpolator, like Frame Generation.

  None of them is in the plan by default:
  - **Frame interpolation.** The engine already draws real in-between
    frames from the world interpolation. Generated frames add latency and
    ghost the sprites.
  - **The denoised upscaler** is for noisy, path-traced light. Every ray in
    this plan is either deterministic (shadows, reflections) or averaged in
    world space (the surface cache), so there is no noise to remove. The
    denoiser also wants the frame split into albedo, normals and roughness,
    and Marathon's ramp shading does not come apart that way.
  - **Temporal upscaling** softens the 128-pixel art (ROADMAP F6) and needs
    things the renderer does not have: motion vectors, sub-pixel jitter and
    a stored depth buffer. It stays available as a fallback (decision 3).
- **Ray tracing** is what the tier is made of. The M5 has ray-tracing
  hardware, usable from fragment shaders too (`supportsRaytracing`,
  `supportsRaytracingFromRender`, Apple 10 family; checked this session).
  The engine also traces rays already, in software: light shadows, the fog
  and light redistribution all walk Marathon's own polygon map. That walk
  is exact in "5D space", and half of Marathon 2's campaign uses 5D space
  (below). Whether each new kind of ray runs on the hardware or on the walk
  is the first measurement (step 0).

## Step 0 results (30 Sep 2026)

`scripts/trace-spike.swift` traced 1M rays per set on eight levels with the
engine's own walk (`light_reaches`, `trace_radiance`, ported unchanged)
and with a hardware acceleration structure of the same geometry.

Rays per millisecond of GPU time (median of 10, whole-set means):

| Rays | Walk | Hardware | Hardware wrong on 5D levels |
|---|---|---|---|
| Shadow segments, 1–2 WU | 0.82 M | 1.96 M | 2.7% (13% on 5-D Space) |
| Short occlusion, 0.5 WU | 1.10 M | 1.90 M | 1.2% |
| Long, 16 WU, scattered | 0.53 M | 1.18 M | 3.5% |
| Long, 16 WU, coherent (blocks of 64, as neighbouring pixels) | 1.73 M | 1.35 M | 3.5% |

- **Correctness.** On levels without 5D space the hardware never disagrees
  with a strict walk, so the port and the triangle build are right.
- **Build cost.** A level's acceleration structure builds in 0.3–0.7 ms and
  refits in 0.1–0.2 ms.
- **The walk visits 1.5–1.9 polygons per ray.** Marathon's rooms are small;
  divergence, not distance, is what costs.

**Consequence.**
- **The walk is the tracer** for everything that casts coherent rays:
  - shadows toward a light (R1);
  - reflections and refraction off a liquid (R2);
  - the bake (R4).

  It is exact in 5D space and needs no acceleration structure or 5D masks.
- **The hardware stays in reserve** for scattered short rays (R3), if the
  walk is too slow there and a 1% error on 5D levels is acceptable.
- **Step 2 therefore loses its acceleration structure.** Its tables (the
  objects, the textures and the surfaces) arrive with the step that first
  needs them:
  - R1 brings the objects and the sprite textures;
  - R2 brings the surfaces and wall textures, with the trace view.

**Faults found in the engine's own walk** (the spike counted them as the
walk's error):
1. `light_reaches` never checks the height where a segment ends. A segment
   ending over another polygon's floor or under its ceiling counts as lit,
   so light leaks between stacked rooms: 1.6% of segments on level 4, 9.9%
   on level 10.
2. `polygon_exit`'s 1e-3 tolerance is in raw world units, below float error
   thousands of units out: about 0.01% of long rays step back across the
   edge just crossed.

Rampant's traced shadows fix (1). Flagship's light shadows are the
owner's call, since they have passed QA.

## What we know (from the code, 30 Sep 2026)

**Machine.** Apple M5, 16 GB, macOS 27, Xcode 27. Metal ray tracing from
compute and render pipelines is supported, as is every MetalFX scaler.

**One frame.** One command buffer (`DurandalGL` `begin_frame` to
`Present`), in this order:
1. Compute: `radiance_bake`, then `radiance_average`, then
   `volume_kernel`. These must run before the world encoder opens
   (`DurandalMetal.mm:1262, 1310`).
2. The world pass. Attachments: colour RGBA8, glow RGBA16F, distance R32F
   and depth. Depth is never stored.
3. AO at half resolution (`run_ao`).
4. Bloom.
5. The world blit (`DurandalGL::DrawWorldImage`).
6. 2D into the canvas.
7. The output pass to the drawable.

**Budget.** 120 fps leaves 8.3 ms. Flagship's last GPU timings (L06 combat,
Round 9, git-ignored `.deps/bench/r9c`):

| Stage | Average |
|---|---|
| Whole frame | 9.98 ms |
| World pass | 7.89 ms |
| Fog volume | 1.09 ms |
| Light bake | 0.02 ms |

Added since then:
- HD art;
- Ambient Shadows, about 1.7 ms;
- character shadows.

Play runs on 28 Sep (vsync, HD art) averaged 130–150 fps, with 1% lows of
67–86. **Flagship has no headroom left on the fanless Air**, so the fifth
tier cannot be free.

Three stages are not timed separately: AO, bloom and output.
`DURANDAL_GPU_TIMING` times only the fog, bake, averages, world pass and
the whole command buffer.

**What the GPU knows about the level.**
- The per-polygon map (`DurandalLights::BuildMap`,
  `DurandalLights.cpp:237-270`): 9 float4 per polygon for every polygon,
  rebuilt each frame. It holds:
  - the outline;
  - the neighbours;
  - the current floor and ceiling;
  - the floor and ceiling light;
  - the liquid.

  `o[4..8].w` are free. It has no sides, textures or texture coordinates.
- There is no level mesh. Surfaces go out per draw through
  `setVertexBytes`.
- There is no acceleration structure, and no argument buffers, bindless
  tables or residency sets.
- Textures are created lazily, the first time a surface is drawn. A ray
  could meet a texture that does not exist yet.
- There are no motion vectors, previous-frame matrices or jitter.

**The tracers that exist.** Each walks a 2D segment through polygon
adjacency and checks the opening's heights at each crossing, as Marathon's
own line of sight does.
- `light_reaches` (`DurandalMetalShaders.h:123`) gives the light shadows.
  It runs per fragment per light, at most 24 polygons × 8 edges.
  - It cannot see sprites, models or the texture of a transparent side. A
    grate is an open doorway to it.
  - A sprite's test starts from the render node's polygon, which can be a
    different polygon from the one the figure stands in
    (`RenderRasterize_Metal.cpp:976`).
- `trace_radiance` gives the light redistribution, at most 48 steps.
  - A hit returns the surface's authored brightness × its texture's average
    colour (`DurandalRadiance.cpp:375-411`). It never reads another lumel,
    so light makes **one** bounce.
  - The sky is a constant blue-grey (`kSky`, `:48`).
  - A patch that has settled (192 samples) is never baked again. A change
    in light level does not start a new bake.
  - Sprites, liquids and landscapes get nothing from it.
- `volume_kernel` does the same walk for the fog.

**Liquids.** `liquid_fragment` (`DurandalMetalShaders.h:1014`) reads the
pixel beneath from tile memory (`[[color(0..2)]]`).
- It sees only its own pixel, so it cannot refract.
- Nothing draws the room above, so it cannot reflect. Its "reflection" is
  a Fresnel term on the surface's own lit colour, plus glints.
- Because it reads the attachments, under MSAA it runs once per sample.

**Sprites.** Objects are gathered only from visible polygons
(`RenderPlaceObjs.cpp:122-160`). A figure behind the viewer or round a
corner is never known to the renderer. A figure's frame depends on the
viewer's angle.

**5D space, counted this session** from `Map.sceA`: polygons that are not
neighbours but overlap in plan and in height. **14 of the 28 solo levels
have some.** Pairs per level:

| Level | Pairs |
|---|---|
| Beware of Abandoned Rental Trucks | 44 |
| The Hard Stuff Rules | 43 |
| Charon Doesn't Make Change | 23 |
| God Will Sort The Dead | 11 |
| Slings & Arrows | 9 |
| Six Thousand Feet Under | 9 |
| Nuke And Pave | 6 |
| Eat It, Vid Boi | 4 |
| Seven more | 1–2 each |

The check finds 9 pairs in the net map "5-D Space", which is what it
should find. Platforms were counted at their stored heights. One
acceleration structure of the whole level would let a ray hit walls from
the other space at all of those places.

**Jjaro.** Marathon 2's Shapes file has no Jjaro walls or scenery:
collections 20 and 25 are empty (the file's collection headers were
checked). The guide's "The Jjaro wall set has not had its lamps marked"
never applies to this game; it is an Infinity matter.

## The tier

**Rampant** is Flagship plus R1–R4 below.
- It is stored as `quality_tier` 5. Custom stays 4, as existing prefs files
  store it.
- The menu reads Stock, Classic, Enhanced, Flagship, Rampant, Custom.
- The first run still applies Flagship.

The anchor holds (ROADMAP principle 2): every surface keeps its authored
brightness on average, and new light only moves light within it. Every
feature has its own switch and a way back. All of it is rendering:
objects, polygons, lights and liquids are read, never written.

### R1 Shadows that see everything

**Built (30 Sep 2026), awaiting a look:** figures as cards with their
8-bit frame's silhouette from a mask array with mips (no bindless
textures, so figures out of view need nothing loaded); soft edges from
the mip level set by the light's disc; the end-height fix. Grates wait for
the surface table (R2). See CLAUDE.md, Round 13.

**What.** Dynamic lights (bolts, explosions, flashes) cast shadows of:
- monsters, BoBs, items and scenery;
- 3D pickups;
- the solid bars of grates and other transparent sides.

A figure round a corner or behind the viewer can throw its shadow into
view.

**How.** The walk stays in charge of structure. It already passes through
every polygon between the lit point and the light. At each polygon it will
also:
- test the figures in that polygon, from a per-polygon object list
  uploaded each frame. The list includes figures that overlap the
  polygon's edges. Each figure is a card turned to face the ray, with its
  frame's alpha.
- at a crossing through a transparent side, sample that side's texture
  alpha at the crossing point.

This is exact in 5D space by construction. The hardware alternative puts
the figures as bounding boxes in a small acceleration structure, with an
intersection function that turns the card to the ray, and accepts hits
only in polygons the walk visited. Step 0 decides between the two.

Also in R1:
- **Receivers.** A sprite's own test starts from the polygon the figure
  stands in, not the render node's polygon.
- **Frame.** The shadow uses the frame the viewer sees (the simplest
  choice, and it matches what is drawn).
- **Fallback.** The fixed over-the-shoulder light of Character Shadows
  stays for figures with no dynamic light near them.
- **Soft edges.** Each light gets a size from its strength: an explosion
  is larger than a bolt. Several rays per lit pixel towards points on the
  light, with a fixed per-pixel pattern and a depth-aware blur as the
  ambient shadows use. The penumbra widens with distance from the
  occluder, as real shadows do.

**Needs from step 2:**
- the object table: every object within reach, not just visible ones;
- sprite and side textures in the texture table;
- side data in the surface table.

### R2 Water that reflects and refracts

**What.**
- Water, sewage and goo show the room above them, sky included, broken up
  by their ripples.
- They bend what lies beneath.
- Lava stays opaque and glowing.
- Murk, absorption and caustics stay as they are.

**How.** `liquid_fragment` traces two rays:
- one reflected about the wave normal, weighted by Fresnel;
- one refracted, with an index set per liquid type (1.33 for water).

A hit is shaded the way Marathon shades that surface:
- its texture through its ramp at its light level;
- the headlight by distance;
- its redistribution lumel;
- the fog volume at that distance;
- dynamic lights, without shadows.

A ray that leaves through a landscape surface samples the landscape by
direction. Figures reflect too (the R1 cards). Murk takes its depth from
the refracted ray's hit distance.

One side effect: the shader no longer reads the attachments beneath, so
under MSAA it runs once per pixel rather than once per sample.

**Risk.** Traced shading must match the drawn frame where the refraction
offset is near zero, or a seam shows at the shoreline.
- **Acceptance:** with the waves off and an index of 1, the traced floor
  matches the drawn floor within about 1/255 mean, scored with the
  frame-comparison tools.
- **Fallback:** the reflected ray at half resolution if it costs too much.

### R3 Ambient shadows from the world

**What.** Occlusion that comes from the geometry and figures around each
point, not from the picture:
- no dark halos that appear and vanish at the edge of the screen;
- a wall just out of view still shades the floor;
- a figure still darkens the corner it stands in.

**How.** At half resolution, as now: a few short rays per pixel, reach
0.5 WU as today, from the world position rebuilt from the distance image.
They are traced through the walk and the figure cards.
- The fixed per-pixel rotation and the depth-aware blur stay, so there is
  no shimmer from frame to frame.
- A walk needs its starting polygon, so the world pass gains a polygon-id
  attachment. Under MSAA it stores sample 0, since integers cannot be
  averaged.
- This replaces the screen-space version in this tier only. Flagship keeps
  it.

**Cheaper variant.** Take static occlusion from the surface cache, which
already gathers less in corners, and trace only the figures. Step 0 and
this step's measurements decide.

### R4 Light that keeps travelling

**What.**
- Light goes round corners: a lit room brightens the corridor off it, and
  a lava pool's glow reaches the next room, faintly.
- Colour bleeds more than once.
- Figures pick up the light bounced around them.
- The sky gives the light of the level's own sky.

**How.** In `radiance_bake`, a ray that hits a surface with settled lumels
takes that lumel's gathered light × the surface's albedo, not only its
authored brightness. Bounces build up over frames, damped. This needs:
1. a patch lookup on the GPU (polygon × 10 + part → patch; the CPU arrays
   exist);
2. albedo stored apart from emission in the `surfaces` buffer;
3. a read copy of the atlas, ping-ponged (about 4–6 MB);
4. slow re-bakes of settled patches so the bounce can flow, plus a re-bake
   when a room's light changes a lot (switches, lights going out).

The shading clamp against the group's average is unchanged, so no
surface's average moves.

Also in R4:
- **Sky light.** The landscape's average colour replaces the constant
  blue-grey.
- **Figures.** A figure samples the floor lumel under its feet and the
  ceiling lumel over its head, under the same clamp. Today sprites get
  nothing from redistribution.

R4 needs no ray-tracing hardware and nothing from step 2, so it can come
first.

## The other limits in part XII

- **Film export on the Metal display** (every Metal tier, not a tier
  feature). `Movie.cpp:272-278` refuses it today. The capture already
  exists: `RequestOutputCapture`/`TakeOutputCapture` in `DurandalGL.mm`
  are used by the output shots.
  - `Movie::AddFrame` gets a Metal branch.
  - Read-back becomes asynchronous: a ring of shared buffers with
    completion handlers, instead of `waitUntilCompleted` every frame.
  - EDR is mapped to SDR for the file, and the frame is scaled to the
    movie's size.
  - Effort S–M.
- **Looking straight up and down.** A second, backward visibility walk:
  - a mirrored view with its own visibility tree, sort and object
    placement;
  - the vertical plane through the eye as an extra clip plane
    (`clip_planes[3]` is full);
  - the 89° caps lifted (`render.cpp:673`, `DurandalCamera.cpp:144`);
  - `dtanpitch`'s division by the cosine made safe near 90°;
  - the backward walk must not mark automap polygons.

  Effort L, risk medium. Feel tab, part of Free Look. It was asked for
  once (the full 180 degrees).
- **Jjaro lamps.** Nothing to do in Marathon 2 (above). Correct the guide.
- **Models do not animate.** Upstream animates Dim3 models
  (`RenderRasterize_Shader.cpp:1081-1098`); the Metal path keeps the
  vertices as first seen. Porting it is S–M, but no animated model pack
  for Marathon 2 exists, and the guide's own line stands: the sprites are
  better. Not planned; revisit if a pack appears.
- **OpenGL gets none of this.** By design: Stock is upstream's renderer
  exactly, the way back.

## Declined

- **MetalFX frame interpolation and the denoised upscaler.** Reasons
  above.
- **Temporal upscaling by default.** It stays a fallback only
  (decision 3). The groundwork would be:
  - a velocity attachment;
  - cached previous-frame matrices, platform heights and object positions;
  - projection jitter;
  - stored depth.

  That is M–L effort, and it softens the art.
- **Reflections on walls and floors.** Marathon has no shiny materials.
  This would invent them.
- **Full path tracing.** As in ROADMAP: it replaces the authored light.

## Steps

The branch is `durandal/round-13-rampant` (or the chosen name), from
`durandal/main`. Every step ends with:
- the film tests four ways: Stock, default, QA, and `quality_tier=5`;
- a benchmark on screen;
- a commit;
- the owner's QA.

Every feature ships in the QA gate (`Released()` false) until it passes.

0. **Measure (S).** No visible change.
   - Time AO, bloom, the world blit and output as separate stages under
     `DURANDAL_GPU_TIMING`.
   - Take a fresh Flagship benchmark with the HD art, on screen, over the
     standard films and the owner's own. This is the baseline the tier is
     judged against.
   - The tracer spike runs as a standalone tool with no window
     (`scripts/trace-spike.swift`). It reads the levels from `Map.sceA`
     and runs the walk and the hardware on the same rays. The game never
     starts.
   - **The tracer spike.** A dev switch that, during a benchmark film,
     traces the same batch of rays two ways:
     - the polygon walk;
     - a hardware acceleration structure of the level, with hits in the
       other space rejected.

     The batch is shadow segments plus long, reflection-like rays from the
     frame's pixels. It logs rays per millisecond. This decides where
     R1–R3 run, and whether Flagship's light shadows could get cheaper.
   - The 5D count is done (above).
1. **Tier scaffold and R4 (M).**
   - `kTierRampant = 5`.
   - `FeatureTier`, `TierIncludes` and `ApplyTier`.
   - The parse whitelist (`DurandalPreferences.cpp:211-215`).
   - The `DURANDAL_SET` range (`:314`).
   - The dialog's `tier_labels`, `tier_to_index` and `index_to_tier`
     (`:432-436`).
   - The scripts' `quality_tier` defaults.

   R4 gives the first visible change without waiting for step 2.
2. **The level on the GPU (L).** Three tables, then a check.
   - **Surface table.**
     - Per polygon: floor and ceiling texture, origin, transfer mode and
       light.
     - Per side: primary, secondary and transparent texture, x0/y0, light
       and transfer mode.
   - **Texture table.** An argument buffer holding every wall texture,
     landscape and sprite frame the level uses:
     - both forms: 8-bit index plus ramps, and true colour or HD;
     - preloaded at level entry and kept resident with a residency set;
     - hit shading may use a capped mip (128 px) to hold memory.
   - **Object table.** Built each frame, per polygon: every object within
     reach, with its frame's texture, size, flip and transfer mode.
   - **The check.** A dev view, `DURANDAL_TRACE_VIEW=1`, draws the world by
     tracing one ray per pixel and shading the hit. It is scored against
     the drawn frame with the frame tools. When the two agree, the tables
     are right. This view is also the base of R2's hit shading.
   - **If step 0 picks the hardware:** the acceleration structure. Static
     level geometry, refitted each frame for platforms and liquids at
     their interpolated heights, and figures as bounding boxes. It is
     built on the frame's own command buffer.
3. **R1 Shadows (M).**
4. **R2 Water (L).**
5. **R3 Ambient shadows (M).**
6. **Film export (S–M).** Last: screen recording covers it for now.
7. **Straight up and down (L).** A later round.
8. **Docs.** Both editions of the guide:
   - the fifth tier row;
   - Durandal's note on it, which comes true;
   - part XII rewritten to what remains;
   - the Jjaro correction.

   Also CLAUDE.md status, and the ROADMAP tier table. Keep the two guide
   editions' facts in step.

Overall: L to XL, spread over several sessions.

## Budget (estimates, replaced by step 0's measurements)

| Item | Estimate at 1080p | Notes |
|---|---|---|
| R1 shadows | 0.2–1 ms | Only pixels within a light's radius, with a light that passes the facing and radius tests |
| R2 water | 0.5–2 ms | Only liquid pixels; worst when underwater or over a large pool; half-resolution reflection as a fallback |
| R3 ambient shadows | about the 1.7 ms of today's version, which it replaces | Half resolution, 4–8 short rays |
| R4 bounce | < 0.1 ms | Inside the existing bake budget (384 tiles a frame) |
| Step 2 tables | CPU: object table per frame (≤384 objects); memory: the texture table | Measure the footprint; the guide quotes about 350 MB today |

## Determinism

Everything is rendering.
- The object table reads objects the same way `DurandalLights` already
  does (`get_object_shape_and_transfer_mode`).
- No `global_random` or `local_random`. Ray patterns stay fixed per pixel
  or seeded by world position, as now.
- Nothing writes game-world arrays.

## Risks

- **Memory.** Every image resident at once for the texture table. With HD
  art, a level's cache is about 1.2 GB of BC7, while the lazy footprint is
  about 405 MB. Memory is not capped (decided), but the footprint is
  reported in every benchmark summary so it stays known.
- **Heat.** The Air is fanless. A tier that works it hard throttles after
  some minutes, so the 1% low is judged on a warmed-up machine (soak runs),
  never on the first minute.
- **Seams** where traced shading meets drawn shading (R2). The trace-view
  check exists to catch them early.
- **Command queues.** `DurandalMetal`'s mip generation and zero fills run
  on a second queue and rely on commit order (`DurandalMetal.mm:1787`).
  Metal does not promise that ordering across queues. Anything new goes on
  the frame's own command buffer, and the existing case should be checked
  while there.
- **Measurement drift.** The fanless Air drifts between runs. Interleave
  base runs (`scripts/costs.sh`) and check `ps` for busy daemons first.
- **Shader compiles.** Every shader edit gets `scripts/check-shaders.swift`
  before a run. A failed compile at launch crashes the game.

## Decisions

Answered on 30 Sep 2026: see Decided, at the top.

Every run in this plan (spike, benchmarks, look checks) goes full screen
on the main display, and is asked for in chat first.
