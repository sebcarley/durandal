# Durandal

Marathon 2: Durandal on a native Metal engine for Apple Silicon.

Durandal is a fork of [Aleph One](https://github.com/Aleph-One-Marathon/alephone),
the open source continuation of Bungie's Marathon 2 engine. It replaces the
OpenGL renderer with one written for Metal and builds a modern look, sound
and feel on top of it, while keeping the game itself exactly as it was:
films replay tick for tick, saved games load, and the rules are untouched.

The same engine plays the other two games of the trilogy, as two more apps
with their own folders, HD art and guides: **Durandal Marathon** (Marathon,
1994) and **Durandal Infinity** (Marathon Infinity, 1996).

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

## Playing it

The quickest way: download the app from
[Releases](https://github.com/sebcarley/durandal/releases/latest), unzip
it, move **Durandal.app** to Applications and open it. It is signed and
notarised, and carries Marathon 2's data; GET HD ART... on the ART tab
fetches the community's HD art. Built and tested on macOS 27.

To build it yourself you need an Apple Silicon Mac, macOS 12 or later, and
Xcode from the App Store (open it once so that it finishes installing).
Then, in Terminal:

    git clone https://github.com/sebcarley/durandal.git
    cd durandal
    scripts/setup.sh

That fetches the game data, the build tools and the community HD art,
builds the game, and leaves **Durandal.app** in the folder. Double-click
it. About 20 minutes the first time, most of it unattended; about 1 GB of
art to download. `scripts/setup.sh --no-art` skips the art (the game's
GET HD ART... button, below, fetches it later).

The other two games build the same way: `GAME=m1 scripts/setup.sh` leaves
**Durandal Marathon.app** (about 0.1 GB of art), `GAME=inf scripts/setup.sh`
**Durandal Infinity.app** (about 1 GB), and `GAME=all` builds all three.
Signed downloads of those two are still to come.

The game starts on the Flagship tier. If it is too slow on your Mac:
Preferences > DURANDAL > Quality.

## HD art and music

The art is the community's, not ours, so it is not in this repository and
is not ours to re-host. `scripts/get-hd-art.sh` fetches the set we play
with (the four Community/Freeverse packs and the 3D Items plugin) from its
authors' own pages and installs it as ordinary Aleph One plugins in
`~/Library/Application Support/Durandal/Plugins`. It never overwrites a
pack that is already there. The game can do the same itself: Preferences >
DURANDAL > ART > **GET HD ART...** lists the five packs, fetches the
missing ones with a line of progress each, and switches them on from the
next level, no restart. The ART tab chooses which kinds to use.

Any other Aleph One art or music pack works the same way: unzip it into
that folder. `docs/HD_ASSETS.md` and `docs/AUDIO_ASSETS.md` catalogue what
exists, who made it and where it lives.

## Building, for developers

    scripts/install-deps.sh      # once, about 8 minutes; keeps everything inside the folder
    scripts/build.sh             # builds Durandal.app (Release)
    scripts/test-films.sh        # the film determinism test

In Xcode: open `Xcode/AlephOne.xcodeproj`, scheme **Marathon 2**, destination
**My Mac**, Run. Everything that has passed QA runs however the app is
launched. Features still in QA need `DURANDAL_QA=1`, which the scheme's Run
action sets; `DURANDAL_STOCK=1` closes the gate altogether.

## Current state

Confirmed in play at `baseline-13` (4 October 2026): all five tiers in
all three games, each game's community HD art and the texture cache, with
the film tests passing three ways (enhancements off, on, and with anything
still in QA) for Marathon (27 films), Marathon 2 (42) and Marathon
Infinity (32). [HISTORY.md](HISTORY.md) has the development to date, round
by round.

## Where to read more

- `docs/GUIDE.html`: the field guide, illustrated and annotated by its namesake (on the web at https://sebcarley.github.io/durandal/); `docs/GUIDE.md` is the plain edition
- `docs/GUIDE-marathon.html` and `docs/GUIDE-infinity.html` (plain editions `.md`): the guides to Durandal Marathon and Durandal Infinity
- `docs/PLAN-trilogy.md`: how the engine was taken to the other two games
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
