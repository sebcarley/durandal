# Durandal — a field guide

*Marathon 2 · a native Metal engine for Apple Silicon · Autumn 2026*

*This is the plain edition, which reads on GitHub. The illustrated edition,
annotated by Durandal himself, is [GUIDE.html](GUIDE.html): open it in a
browser.*

Seventeen years after the Marathon, you are woken above a dead marsh world
by the machine that stole you. Lh'owon was beautiful once. Now its sewers
run under ruins nobody has lit for a thousand years, the water has gone
the colour of old copper, and whatever the S'pht built here is drowned or
buried. Durandal wants something from under all that. He has not said what.

This is the same game Bungie shipped in 1995, tick for tick. What has
changed is how much of Lh'owon you can see, and how much of it you would
rather not.

**Contents**

1. [The light](#i--the-light)
2. [The air](#ii--the-air)
3. [The water](#iii--the-water)
4. [The old colours](#iv--the-old-colours)
5. [Depth](#v--depth)
6. [Built by other hands](#vi--built-by-other-hands)
7. [The eye](#vii--the-eye)
8. [The room answers](#viii--the-room-answers)
9. [What the terminals say](#ix--what-the-terminals-say)
10. [Four ways to run it](#x--four-ways-to-run-it)
11. [Watching it back](#xi--watching-it-back)
12. [The edge of the known](#xii--the-edge-of-the-known)

Durandal is Marathon 2 for Apple Silicon: a Metal renderer grown out of
Aleph One, with lighting, liquids, air and sound built on top of it. It does
not change what Marathon is. Films replay, saves load, the Pfhor are exactly
as fast and exactly as many.

Each part opens with what the thing is, then goes deeper. Everything is in
**Preferences > DURANDAL** and can be switched off there. The table at the
foot of each part gives the switch and the name it is stored under, for
anyone reaching further in; it can be passed over entirely.

---

## I · The light

### Nothing here was lit for you

> The S'pht left their lamps burning. The Pfhor brought their own. Neither
> expected company.

Marathon's levels were lit by hand, surface by surface, and that light is
kept: every wall still averages the brightness its designer gave it. What
is new is everything that moves through it.

**Cast light.** A fusion bolt carries its light down the corridor with it.
A compiler's shot lights the wall it passes and the floor it dies on. A
Fighter's staff, a grenade, the flash at the end of your own barrel: up to
sixteen of them at once, each the colour of the thing that made it. A wall
is lit only on the side that faces the fire.

**And its shadow.** The light stops where Marathon says a line of sight
stops. It walks the map polygon by polygon, exactly as a Trooper's aim
does, and is cut by the same walls, steps and ceilings. It works in the
places where the map folds through itself, because it never leaves the
map.

**Glow.** The eyes of a Hunter, the lights on a Trooper's armour, the lamps
the S'pht set into their walls, water and lava where they catch the light.
These glow, and on a display that can go brighter than white, they do.
The sky over Lh'owon is a sky, not a painted wall: its brightest clouds
bloom.

**Light that travels.** A lamp at one end of a hall now reaches a little
way along it. Light gathers from the lamps, the openings and the sky,
settles over a second or so, and leaves each surface as bright on average
as it always was. Rock lends its colour to the shadows beside it. When a
door opens, the change arrives gently.

**The weapon in your hands** takes the light around you: it warms as a
bolt passes close, and flares with its own fire. (Still in QA.)

**Relief.** Under the headlight the old wall art stands up a little: seams,
rivets and carved glyphs catch the light on one side and lose it on the
other. HD walls bring their own relief and use that instead.

| Switch | Stored as | Tab |
|---|---|---|
| Dynamic Lights | `dynamic_lights` | Light |
| Light Shadows | `light_shadows` | Light |
| Ceiling Light on Sprites | `sprite_lighting` | Light |
| Glow, Bloom, HDR Sky | `glow`, `bloom`, `hdr_sky` | Light |
| Light Redistribution, and its strength | `light_redistribution`, `gi_strength` | Light |
| Weapon Takes the Light (in QA) | `weapon_lighting` | Light |
| Surface Relief | `surface_relief` | Look |
| Normal Maps (HD walls) | `normal_maps` | Art |
| HDR Output | `hdr_output` | Light |

---

## II · The air

### A thousand years of standing still

> Nothing has moved the air down here since the Pfhor sealed the doors.
> You are the draught.

The air has weight now. It lies thicker near the floor, drifts in slow
patches, and takes its light from the room it is in, so a dark room keeps
dark air and a bolt crossing it lights the haze as it goes. Over lava the
smoke rises and glows from beneath. Each kind of place has its own: the
water levels are nearly clear, the sewers are not.

It is held in three dimensions, in the shape of the map, and stops at
walls. Where a scenario declares its own fog, that becomes the colour and
thickness of the air instead.

| Switch | Stored as | Tab |
|---|---|---|
| Volumetric Fog | `volumetric_fog` | Light |
| Fog Strength (Light, Medium, Thick) | `fog_strength` | Light |

---

## III · The water

### It was a marsh world once

> The F'lickta were here before the S'pht and will be here after you.

Stand at the edge and look down. The floor of the pool is there, further
off than it looked, and darker the deeper it lies. Water takes the red out
of what is under it first. Sewage takes nearly everything. Lava shows you
nothing at all and lights the ceiling instead.

The surface moves, and catches the light: a sheen at a low angle, a glint
under the headlight, a flare as a bolt crosses it. Beneath it the light
comes down in moving bands across the floor and the walls.

Go under and the world wavers, the sound closes in, and what you can see is
decided by what you are swimming in. Your own weapon stays in front of your
face, where it belongs.

| Switch | Stored as | Tab |
|---|---|---|
| Real Liquids | `liquids` | Light |
| Smooth Liquids, Lights, Fades | `smooth_world` | Feel |

---

## IV · The old colours

### 256 of them, chosen by hand

> Marathon never darkened a colour. It chose a darker one.

The original game did not dim a pixel by multiplying it. Each colour sat on
a ramp drawn by an artist, and shadow meant walking down that ramp to the
next colour along. It is why Marathon's darkness has the hues it has, and
it is reproduced here exactly, either in the original bands or smoothly
between them.

Each texel of the old art takes one shade, not a gradient smeared across
it, so 128 by 128 stays crisp at any distance. Up close the texels are
sharp-edged and clean. Far away they are filtered properly and stop
shimmering. Edges of walls are smoothed.

If the whole picture sits too dark or too flat on your display, the scene
can be graded without touching the HUD.

| Switch | Stored as | Tab |
|---|---|---|
| Marathon Shading, and its style (Banded, Smooth) | `shading_tables`, `shading_style` | Look |
| Texel Lighting | `texel_lighting` | Look |
| Crisp Filtering | `crisp_filtering` | Look |
| Smooth Edges | `edge_smoothing` | Look |
| Scene Brightness, Contrast, Gamma | `scene_brightness`, `scene_contrast`, `scene_gamma` | Look |

---

## V · Depth

### The big rooms are big

> You will see the shadow on the floor before you see the Hunter.

Marathon's halls are vast and its figures are flat, and both read better
with a little shadow.

**Ambient shadows** darken the places light has trouble reaching: the foot
of a wall, the inside of a corner, the floor behind a crate. They leave the
sky, the water and the weapon in your hands alone.

**Character shadows** lie on the floor in the shape of whatever casts them.
When something nearby is burning, the shadow falls away from it and moves
as it moves. Otherwise it falls forward, where you can see it. A drone's
shadow fades as it climbs.

**Contact shadows** sit under items, scenery and the dead.

**Distance shade** lets the far end of a long hall fall away into the dark
beyond your headlight. It is off until you ask for it.

| Switch | Stored as | Tab |
|---|---|---|
| Ambient Shadows | `ambient_shadows` | Light |
| Character Shadows | `character_shadows` | Light |
| Contact Shadows | `contact_shadows` | Light |
| Distance Shade (0 to 100%) | `distance_shade` | Look |

---

## VI · Built by other hands

### Thirty years of other people's work

> The Marathon was never only Bungie's. It has had a crew the whole time.

The community has redrawn Marathon 2 at many times its resolution: walls,
skies, every frame of every Pfhor, the weapons in your hands, the pickups
on the floor. It is theirs, so it is not kept in this repository, but it
is one command away: `scripts/get-hd-art.sh` fetches the set this guide
was written with from its authors' own pages and installs it, and
`scripts/setup.sh` does that as part of building the game. Any other
Aleph One art pack works too: put it in the Plugins folder and choose, by
kind, what to take from it.

HD walls bring normal maps and their own glow. HD sprites glow as the old
self-luminous colours did. Pickups can be real models, lit like the room
they are lying in.

The first time a level loads its art, each image is compressed and kept.
After that a level's art arrives in under half a second, and the full set
costs a few hundred megabytes where it once cost several gigabytes.

| Switch | Stored as | Tab |
|---|---|---|
| HD Walls and Sky, Monsters, Weapons and Items, Scenery | `hd_walls`, `hd_monsters`, `hd_weapons`, `hd_scenery` | Art |
| 3D Pickups, Spinning Pickups | `models_3d`, `spin_pickups` | Art |
| Texture Cache | `texture_cache` | Art |
| Soundtrack | `soundtrack` | Art |

The packs, their authors and where to find them are in
[HD_ASSETS.md](HD_ASSETS.md) and [AUDIO_ASSETS.md](AUDIO_ASSETS.md).

---

## VII · The eye

### You could never look up

> Thirty degrees. That is how far the armour lets you aim, and until now it
> was how far you could look.

Marathon tilted the picture rather than the head. Now the camera turns:
look up and the walls converge above you, as walls do. Aim still stops at
thirty degrees, because the game's rules say so, but the view does not. Keep
looking and the crosshair stays behind on the true aim, so you always know
where the shot will go.

Sidestep and the horizon leans a little into the turn. The view fills a
wide screen without stretching. The mouse is read every frame, not every
tick, and the world between ticks is drawn in between, up to whatever your
display can show.

| Switch | Stored as | Tab |
|---|---|---|
| True Look Up/Down | `true_look` | Feel |
| Free Look Beyond Aim | `free_look` | Feel |
| Sidestep Sway | `sidestep_sway` | Feel |
| Per-frame Mouse Look | `per_frame_look` | Feel |
| Widescreen | `widescreen` | Feel |

---

## VIII · The room answers

### You hear the hall before you see it

> A pistol in a corridor and a pistol in a cistern are different weapons.

The game measures the space around you, through every open door, and the
sound takes its shape: tight in a duct, long in a cistern, thin and
unanswered under an open sky. Shut a door and the room behind it goes
quiet by degrees, not all at once. Under the surface the high notes go
first.

Music is a matter of taste. If you have installed a soundtrack, choose one.

| Switch | Stored as | Tab |
|---|---|---|
| Room Reverb | `room_reverb` | Feel |
| Graded Sound Occlusion | `graded_occlusion` | Feel |
| Soundtrack | `soundtrack` | Art |

---

## IX · What the terminals say

### He does go on

> Read them. It is the only way to find out what you are dying for.

The terminals are laid out exactly as they were, line for line and page
for page, and drawn at the sharpness of your display. The same is true of
every word the game puts on the screen.

| Switch | Stored as | Tab |
|---|---|---|
| Crisp Terminals | `crisp_terminals` | Look |
| Crisp Text | `crisp_text` | Look |

---

## X · Four ways to run it

### From 1995 to the full dark

> The machine has a temperature and a mood. These numbers were taken when
> it had settled into both.

| Tier | What it is |
|---|---|
| **Stock** | Aleph One exactly: OpenGL, thirty frames a second, nothing added |
| **Classic** | The Metal renderer, Marathon's own shading, crisp art and text, wide screens, every frame your display can show |
| **Enhanced** | Classic, with glow, cast light and its shadows, liquids, reverb and the new camera |
| **Flagship** | Enhanced, with the air, travelling light, relief, depth, and the community's HD art |

Pick a tier, or switch any one thing and the tier becomes **Custom**.

Measured on an M5 MacBook Air at 1080p on a 240 Hz display, Flagship with
the full HD set: 130 to 200 frames a second, about 350 MB of memory. With
the old art it is slower, not faster: the 8-bit shading does more work per
pixel than a photograph does.

---

## XI · Watching it back

### Nothing in a film is live

> It has all happened already. It will happen the same way again.

A Marathon film is a list of keys pressed, replayed against the same
random numbers, and it only works if nothing in the game has changed by so
much as one. Nothing has. Forty-three films play to the same final number
with everything here switched off and with everything switched on, and that
check stands guard over every change made.

The testing cheats on the Cheats tab are for looking around. A film
recorded with them carries them, and replays with them.

---

## XII · The edge of the known

### What lies beyond the lamps

> He has told you some of it. He has not told you the rest.

Nothing here is ray traced. Cast light finds its shadows by walking the
map, which is exact for walls and blind to sprites. Ambient shadows are
read from the picture, not the world, so they know only what is on screen.
The water does not refract and does not reflect. Light travels once across
a room and stops.

Pickups can be models; the Pfhor cannot, because nobody has made them, and
the sprites are better than models would be. Models do not animate. The
view can pitch to about sixty-five degrees and no further, because
Marathon's world was never built to be seen from straight below.

The Jjaro wall set has not had its lamps marked. Film export is not
available on the Metal display. The OpenGL renderer is still there and
gets none of this.

And none of it makes the game easier. The light shows you the shadow. It
does not tell you whose it is.

---

**Credits.** *Marathon 2: Durandal* is Bungie's, and its data and source
were released by them. Durandal stands on [Aleph
One](https://github.com/Aleph-One-Marathon/alephone) and thirty years of
work by its developers, under the GPL 3. The HD art and the soundtracks
are the work of the community authors named in
[HD_ASSETS.md](HD_ASSETS.md) and [AUDIO_ASSETS.md](AUDIO_ASSETS.md); none
of it is distributed here.

What the project is and how to build it: [DURANDAL.md](DURANDAL.md). How it
came to be: [HISTORY.md](HISTORY.md).
