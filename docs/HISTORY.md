# Durandal — development to date

A brief record, by round. The round-by-round working notes, with
measurements and the reasoning behind each decision, are in
[`CLAUDE.md`](../CLAUDE.md). This repository's history begins at the first
confirmed state of the whole; the earlier work is summarised here.

## Principles

Set at the start and kept since:

- The game is untouchable. Films replay, saves load, rules stand. A test
  that replays 43 films and checks each one's final random seed gates
  every change, with the enhancements off and with them on.
- Marathon's light is the anchor. New lighting moves light within a
  surface; it never changes how bright the level's designer made it.
- Faithful to the texel. The original art stays crisp, and its colour
  ramps are reproduced, not approximated.
- Everything is switchable, in tiers, and Stock is upstream exactly.

## Rounds

| When (2026) | Round | What was built |
|---|---|---|
| 23 Sep | Phase 0 | Aleph One building on Apple Silicon with the current Xcode; the film test as the gate; a frame-time benchmark |
| 23 Sep | Phase 1 | An audit of how the original renderer makes its look, and a roadmap rated by impact, effort, risk and faithfulness |
| 23 Sep | 1 · Feel | Settings and the feature gate; per-frame mouse look; liquids, lights and fades drawn between ticks; graded sound occlusion |
| 23 Sep | 2 · Metal | A Metal world renderer held to parity with OpenGL (mean difference under 0.25 of 255), then a Metal display with the 2D code running over it unchanged; frame pacing to the display's refresh; HDR output; widescreen |
| 24 Sep | 3 · Look | Marathon's 8-bit colour-ramp shading; texel-exact lighting; crisp filtering and 4x smooth edges; text and terminals at display sharpness |
| 24 Sep | 4 · Glow | A glow image written by every surface; bloom; light above white on HDR displays; the sky as a sky |
| 24 Sep | 5 · Light | Up to 16 dynamic lights from bolts, explosions and flashes; their shadows found by walking the map as Marathon's line of sight does |
| 24 Sep | 6 · Water | Liquids with depth, absorption, murk, ripples, sheen and caustics; contact shadows |
| 24 Sep | 7 · Air | Volumetric fog in the shape of the map, lit by each room and by dynamic lights; the Flagship tier |
| 24 Sep | 8 · Flagship | Light redistribution from a surface cache of traced light, anchored to each surface's authored brightness; surface relief from the art itself |
| 26 Sep | 9 · Sound | Room reverb estimated from the map around the listener; underwater muffling |
| 26 Sep | 10 · Camera | True look up and down; free look beyond the aim limit with the crosshair on the real aim; sidestep sway; a crash record; testing cheats |
| 27 Sep | 11 · HD art | Community art packs chosen by category; block-compressed and normal-mapped textures; 3D pickups; a soundtrack chooser; a catalogue of art and audio packs |
| 27 Sep | 12 · Depth | Ambient shadows; character shadows; redistribution strength; distance shade |
| 27 Sep | Cache | A texture cache (our own BC7 encoder, memory-mapped entries); level entry with HD art from 3.0 s to 0.4 s; footprint from 4.7 GB to about 0.4 GB |
| 28–29 Sep | QA | Four passes in play. The depth image made all-or-nothing, with liquids, the sky and the weapon exempt; the weapon kept out of the murk; doors no longer ripple the light; films carry the cheats they were recorded with |

## What the QA passes taught

Worth keeping, because each was a wrong assumption that looked right:

- A sprite's quad is a rectangle even when the sprite is not. Anything
  that reads depth after the pass must be written only where the texel is
  solid.
- The sky is drawn on the level's own ceiling and walls. To anything
  reading depth it must be far, or its seams become lines across it.
- A marker value that is nearer than its neighbours turns into a phantom
  surface once multisampling averages it. Markers must be far.
- Compression can leave a transparent texel at one part in 255. Alpha must
  be exact at both ends.
- Resident memory hides what the system has compressed. Measure the
  footprint.
- Most of a slow level load was not the images. Measure the phases before
  optimising one.
- A film's header has no room for what was not there in 1995. If a
  recording depends on it, it has to be carried somewhere that is.

## State

Confirmed in play at `baseline-7`, 29 September 2026: all four tiers, the
HD art set, and the texture cache. Film tests pass both ways.

## Next

- The audio round: gapless music, then spatial audio and per-source
  reverb. The research is in [AUDIO_ASSETS.md](AUDIO_ASSETS.md).
- More from the sister project's Quake work, where it suits Marathon.
  Done and in QA: the weapon in hand lit by the lights around the viewer
  and by its own flash. Candidates: heat shimmer over lava; dust and
  embers in the air; a storm under open sky; a photo mode.
- A release: a signed, notarised `Durandal.app` to download, so that
  playing needs neither Xcode nor Terminal, with the HD art fetched from
  inside the game.
- Bloom for the HD skies; wall lamps for the Jjaro set.
