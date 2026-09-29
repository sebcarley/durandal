# Durandal

Marathon 2: Durandal on a native Metal engine for Apple Silicon.

Durandal is a fork of [Aleph One](https://github.com/Aleph-One-Marathon/alephone),
the open source continuation of Bungie's Marathon 2 engine. It replaces the
OpenGL renderer with one written for Metal and builds a modern look, sound
and feel on top of it, while keeping the game itself exactly as it was:
films replay tick for tick, saved games load, and the rules are untouched.

It is a personal project, not affiliated with Bungie or the Aleph One
developers. Like Aleph One it is licensed under the GPL 3.

## What it adds

Everything is switchable, in tiers, from Preferences > DURANDAL. Stock is
upstream Aleph One exactly.

| Tier | What it turns on |
|---|---|
| Classic | Metal renderer and display, Marathon's own colour-ramp shading, texel-exact lighting, crisp filtering, 4x smooth edges, crisp text and terminals, widescreen, smooth frame pacing up to the display's refresh |
| Enhanced | Glow and bloom, HDR output and sky, dynamic lights from bolts and explosions with shadows, real liquids with depth, murk and caustics, contact shadows, room reverb, true look up and down, free look, sidestep sway |
| Flagship | Volumetric fog, light redistribution, surface relief, HD art packs with normal maps, 3D pickups, ambient and character shadows, a compressed texture cache |

## What it keeps

- **Films and saves.** The 43 Marathon 2 films in `tests/replays` replay to
  the same final random seed with the enhancements switched off and
  switched on. That test gates every change.
- **The original look, on request.** Each enhancement has its own switch.
- **Aleph One's content pipeline.** Plugins, MML and shapes patches load as
  they always did.

## Performance

On an M5 MacBook Air at 1080p, Flagship tier with the full HD art set:
130 to 200 frames per second on a 240 Hz display, and about 350 MB of
memory. The texture cache is what makes the HD art affordable: the same
set cost 4.7 GB without it.

## Building

Apple Silicon Mac, macOS 12 or later, Xcode.

    git clone --recurse-submodules <this repository>
    scripts/install-deps.sh      # once, about 8 minutes; keeps everything inside the folder
    scripts/build.sh             # builds Durandal.app (Release)
    scripts/test-films.sh        # the film determinism test

In Xcode: open `Xcode/AlephOne.xcodeproj`, scheme **Marathon 2**, destination
**My Mac**, Run. Release builds enable the enhancements only when launched
with `DURANDAL_QA=1`, which the scheme's Run action sets.

## HD art and music

No third-party art or music is in this repository. Durandal loads
community packs as ordinary Aleph One plugins from
`~/Library/Application Support/Durandal/Plugins`, and the ART tab chooses
which categories to use. `docs/HD_ASSETS.md` and `docs/AUDIO_ASSETS.md`
catalogue the packs, their authors and where to get them.

## Current state

Confirmed in play at `baseline-7` (29 September 2026): all four tiers, the
community HD art set and the texture cache, with the film tests passing
with the enhancements off and on. Next is the audio round, and more
borrowed from the sister project's Quake work where it suits Marathon.
[HISTORY.md](HISTORY.md) has the development to date, round by round.

## Where to read more

- `docs/GUIDE.md`: the field guide, what each thing is and does, in the game's own voice
- `docs/HISTORY.md`: the development to date
- `docs/ROADMAP.md`: the plan, feature by feature
- `docs/AUDIT.md`: how the original renderer produces its look, and what must not change
- `docs/HD_ASSETS.md`, `docs/AUDIO_ASSETS.md`: art and sound packs, and the engine notes behind their support
- `docs/PLAN-true-3d.md`: the camera work
- `CLAUDE.md`: the working notes, round by round, with measurements

## Credits

Bungie, for Marathon and for releasing its source and data. The Aleph One
developers, for thirty years of keeping it alive; this project stands
entirely on their work. The authors of the community art and music packs
named in the two asset documents.

Durandal is developed with Claude (Anthropic).
