# Durandal Marathon — a field guide

*Marathon · a native Metal engine for Apple Silicon · Autumn 2026*

*This is the plain edition, which reads on GitHub. The illustrated edition,
written by Leela between interruptions, is
[GUIDE-marathon.html](GUIDE-marathon.html).*

The Marathon is a moon with engines, twenty-one years in orbit over Tau Ceti
IV, and something has just come out of the dark and boarded it. Of the three
minds that ran the ship, one is silent, one is not answering, and the third is
writing this while compilers take her apart. You have a pistol, and her
instructions.

This is the same game Bungie shipped in 1994, tick for tick. What has changed
is how much of the ship you can see, and how far down the corridor.

**Contents**

1. [The light](#i--the-light)
2. [The air](#ii--the-air)
3. [No water](#iii--no-water)
4. [The old colours](#iv--the-old-colours)
5. [Depth](#v--depth)
6. [Built by other hands](#vi--built-by-other-hands)
7. [The eye](#vii--the-eye)
8. [The frame](#viii--the-frame)
9. [The room answers](#ix--the-room-answers)
10. [What the terminals say](#x--what-the-terminals-say)
11. [Five ways to run it](#xi--five-ways-to-run-it)
12. [Watching it back](#xii--watching-it-back)
13. [Rampant](#xiii--rampant)
14. [The edge of the known](#xiv--the-edge-of-the-known)

Durandal Marathon is Marathon for Apple Silicon: the Metal renderer built for
Marathon 2, taught the first game's numbering, its starfield and its darkness.
It does not change what Marathon is. Films replay, saves load, the Pfhor are
exactly as fast and exactly as many.

Each part opens with what the thing is, then goes deeper. Everything is in
**Preferences > DURANDAL** and can be switched off there. The table at the
foot of each part gives the switch and the name it is stored under. Marathon
differs from the other two games more than they differ from each other, so the
switches that do nothing here are said plainly (part III) and not left off the
lists.

---

## I · The light

### Lit for a crew that left

> The ship was lit for a crew of thousands. Most of them left twenty-one years
> ago, and nobody turned the lamps down.

Marathon's levels were lit by hand, surface by surface, and that light is
kept: every wall still averages the brightness its designer gave it. Some
rooms were given none at all, a light level of exactly nothing, seen only by
the small light you carry. They are still that dark. What is new is everything
that moves through it.

**Cast light.** A fusion bolt carries its light down the corridor with it. A
Fighter's staff, a Compiler's shot, a grenade, the flash at the end of your
own barrel: up to sixteen at once, each the colour of the thing that made it.
**And its shadow:** the light stops where Marathon says a line of sight stops,
walking the map polygon by polygon as a Trooper's aim does. One thing differs
from the later games: here the monsters themselves light nothing. Marathon
draws its Hunters lit from within on every frame, and each one became a green
lamp walking about; so the shots light the room and the bodies do not.

**Glow.** Wall lamps, computer screens, alien panels, the plasma tube and the
lava-coloured walls glow, from a list of thirty-five made by eye across the
ship's six wall sets; on a display that can go brighter than white, they do.
Monsters glow only on the frames drawn near full light, a shot or a flash, and
not their half-lit bodies.

**Light that travels.** A lamp at one end of a hall now reaches a little way
along it, settles over a second or so, and leaves each surface as bright on
average as it always was. Marathon lights its long corridors evenly, which
made each one a single group whose bright end set the average; so here the
travelling light may darken a surface only half as far as in the other two
games. Unlit rooms do not blotch, lines the map draws as walls stop the light,
and the surfaces a moving door uncovers start clean.

**The weapon in your hands** takes the light around you: it warms as a bolt
passes close, and flares with its own fire.

**Relief.** Under your own light the old wall art stands up a little, seams
and rivets catching the light on one side. The HD walls carry no normal
maps, so with them in place the walls stay flat.

| Switch | Stored as | Tab |
|---|---|---|
| Dynamic Lights | `dynamic_lights` | Light |
| Light Shadows | `light_shadows` | Light |
| Ceiling Light on Sprites | `sprite_lighting` | Light |
| Glow, Bloom, HDR Sky | `glow`, `bloom`, `hdr_sky` | Light |
| Light Redistribution, and its strength (Rampant sets Strong) | `light_redistribution`, `gi_strength` | Light |
| Weapon Takes the Light | `weapon_lighting` | Light |
| HDR Output | `hdr_output` | Light |
| Surface Relief | `surface_relief` | Look |
| Normal Maps (HD walls) — idle in Marathon: its HD walls carry none | `normal_maps` | Art |

---

## II · The air

### Three hundred years of the same breath

> The air aboard has been round the ship a great many times. It shows.

The air has weight now. It lies thicker near the floor, drifts in slow
patches, and takes its light from the room it is in, so a dark room keeps dark
air and a bolt crossing it lights the haze as it goes. It is held in three
dimensions, in the shape of the map, and stops at walls.

Marathon has one kind: ship air, everywhere. The later games give each kind of
place its own haze, smoke over lava and the thick air of a sewer. This ship
has neither, and its environment codes name other things, so the same air
fills every deck. Where a scenario declares its own fog, that becomes the
colour and thickness of the air instead.

| Switch | Stored as | Tab |
|---|---|---|
| Volumetric Fog | `volumetric_fog` | Light |
| Fog Strength (Light, Medium, Thick) | `fog_strength` | Light |

---

## III · No water

### Nothing aboard is wet

> You will not drown aboard this ship. I can promise very little else.

Marathon has no liquids. Aleph One's loader sets every polygon's liquid to
none, because the first game never had the idea: there is nothing to wade
through and nothing to fall into, and anything red and molten that you see is
a picture on a surface. So a handful of switches that matter in the later two
games have nothing to do here. They are listed so that nobody hunts for them.
The tiers keep them as they are; they are simply idle.

| Switch or effect | In Marathon |
|---|---|
| Real Liquids, caustics, ripples | Idle: there are no liquids |
| Reflecting Liquids (Rampant) | Idle: nothing to reflect in |
| Embers (of Dust and Embers), Heat Shimmer (Rampant) | Idle: there is no lava |
| Normal Maps (HD walls) | Idle: Marathon's HD walls (TTEP) carry no normal maps |
| Widescreen | No effect (part VII) |
| Summon BOBs | Left out (part XII) |

**Smooth Liquids, Lights, Fades** keeps its other two jobs: the lights and the
fades are still smoothed.

| Switch | Stored as | Tab |
|---|---|---|
| Real Liquids — idle in Marathon: there are no liquids | `liquids` | Light |
| Smooth Liquids, Lights, Fades | `smooth_world` | Feel |

---

## IV · The old colours

### 256 of them, chosen by hand

> Marathon never darkened a colour. It chose a darker one.

The original game did not dim a pixel by multiplying it. Each colour sat on a
ramp drawn by an artist, and shadow meant walking down that ramp to the next
colour along. It is why Marathon's darkness has the hues it has, and it is
reproduced here exactly, in the original bands or smoothly between them. Each
texel of the old art takes one shade, not a gradient smeared across it, so it
stays crisp at any distance; far away it is filtered properly and stops
shimmering. If the whole picture sits too dark or too flat on your display,
the scene can be graded without touching the HUD.

Marathon also draws many of its creatures self-lit. Hunters, Fighters,
Compilers and Wasps carry a minimum light of their own on every body frame,
where the later games keep that for shots and flashes. That is kept, and it is
why they stand out of the dark as they always did.

| Switch | Stored as | Tab |
|---|---|---|
| Marathon Shading, and its style (Banded, Smooth) | `shading_tables`, `shading_style` | Look |
| Texel Lighting | `texel_lighting` | Look |
| Crisp Filtering | `crisp_filtering` | Look |
| Smooth Edges | `edge_smoothing` | Look |
| Scene Brightness, Contrast, Gamma | `scene_brightness`, `scene_contrast`, `scene_gamma` | Look |

---

## V · Depth

### Long corridors, flat enemies

> The corridors are long and what comes down them is flat. A shadow helps you
> judge both.

**Ambient shadows** darken the places light has trouble reaching: the foot of
a wall, the inside of a corner, the floor behind a crate. **Character
shadows** lie on the floor in the shape of whatever casts them, and every
Marathon monster casts one, as does the scenery: when something nearby is
burning, the shadow falls away from it and moves as it moves; otherwise it
falls forward, where you can see it. **Contact shadows** sit under items,
scenery and the dead. **Distance shade** lets the far end of a long corridor
fall away into the dark beyond your own light, and is off until you ask for
it.

| Switch | Stored as | Tab |
|---|---|---|
| Ambient Shadows | `ambient_shadows` | Light |
| Character Shadows | `character_shadows` | Light |
| Contact Shadows | `contact_shadows` | Light |
| Distance Shade (0 to 100%) | `distance_shade` | Look |

---

## VI · Built by other hands

### Thirty years of other people's work

> The ship had a crew. The game has had one for thirty years.

The community has redrawn Marathon at many times its resolution: the six wall
sets and the starfield, the monsters, the weapons in your hands, the scenery.
It is theirs: Durandal Marathon fetches it but never ships it. One button,
**Get HD Art** on the Art tab, brings five packs from their authors' own
pages, and any other Aleph One pack still works dropped into the Plugins
folder. Choose, by kind, what to take.

| Line in the dialog | What it is | Download |
|---|---|---|
| TTEP 1024 | Tim Vogel's Total Texture Enhancement Pack v7 at 1024 by 1024, in Zetren's plugin for the original Marathon data: all six wall sets and the starfield | about 60 MB |
| Updated Starscape | Hopper's 2048 by 1080 starfield, from Aleph One's own release; loads after TTEP and takes over its sky | under 1 MB |
| Texture Renewal Monsters | Rock's Marathon Texture Renewal Project, Monsters Module v0.5: four-times upscales, the BOBs reworked by hand | about 44 MB |
| M1 Weapons Redux | General Tacticus's HD weapons in hand, projectiles, items and impacts | about 14 MB |
| 3D Scenery M1 | General Tacticus's thirteen 3D scenery models | about 4 MB |
| **All five** | | **about 123 MB** |

The walls are Tim Vogel's Total Texture Enhancement Pack at 1024 by 1024, in
Zetren's plugin made for the original Marathon data. Hopper's starfield loads
after it and takes over its sky. The monsters are Rock's Texture Renewal
Project, four-times upscales with the Bobs reworked by hand, chosen after a
side-by-side against the xBR set. The weapons, projectiles, items and impacts,
and the thirteen scenery models, are General Tacticus's.

It fetches one pack at a time, each line showing how far it has got, while the
dialog stays open. A pack already in the Plugins folder is left alone. Each
download is checked against the version the engine was tested with; a newer
one installs anyway and says so. Rock's pack comes as a 7z archive, which the
game opens with macOS's own tools. Close cancels the one in progress and keeps
what is done. New packs join the plugin list at once and their art loads from
the next level, wherever the Art tab's HD switches are on. Marathon has one
model pack, the scenery, so here the **3D Pickups** switch turns the 3D
scenery on and off as well. Each image is compressed once and kept, in a cache
the three games share.

| Switch | Stored as | Tab |
|---|---|---|
| Get HD Art... (a button: fetches the five packs from their authors' pages) |  | Art |
| HD Walls and Sky, Monsters, Weapons and Items, Scenery | `hd_walls`, `hd_monsters`, `hd_weapons`, `hd_scenery` | Art |
| 3D Pickups, Spinning Pickups — in Marathon, 3D Pickups also switches the 3D scenery | `models_3d`, `spin_pickups` | Art |
| Texture Cache | `texture_cache` | Art |

The packs, their authors and the alternatives weighed are in
[HD_ASSETS.md](HD_ASSETS.md), section 10.

---

## VII · The eye

### More stars than the windows showed

> The armour lets you aim thirty degrees from level. It used to let you look
> no further.

Marathon tilted the picture rather than the head. Now the camera turns: look
up and the walls converge above you, as walls do. Aim still stops at thirty
degrees, because the game's rules say so, but the view does not, and the
crosshair stays behind on the true aim so you always know where the shot will
go.

Marathon's sky is a starfield tiled in both directions, eight by eight. Look
up and there are simply more stars, and the picture never runs out: the edge
of the sky that the later games have does not arise here. The stars stay fixed
to the world while your head moves. Sidestep and the horizon leans a little
into the turn. The mouse is read every frame, not every tick, and the world
between ticks is drawn in between, up to whatever your display can show.

**Widescreen** has no effect in Marathon. It works only with the built-in
classic HUD of the later two games, and Marathon's HUD is a script in every
case (part VIII). With the Basic or Enhanced HUD the view fills a wide screen
anyway.

| Switch | Stored as | Tab |
|---|---|---|
| True Look Up/Down | `true_look` | Feel |
| Free Look Beyond Aim | `free_look` | Feel |
| Sidestep Sway | `sidestep_sway` | Feel |
| Per-frame Mouse Look | `per_frame_look` | Feel |
| Widescreen — no effect in Marathon | `widescreen` | Feel |

---

## VIII · The frame

### Three ways to wear the visor

> The original put the world in a window and the instruments round it. You may
> now take the window out.

Marathon has no built-in HUD of the kind the later games have: its HUD is a
Lua script in every case, and upstream ships three. A new switch on the Look
tab, **HUD**, found only in Marathon, chooses between them. Exactly one runs.

| HUD | What it is |
|---|---|
| Classic | The framed original, upstream's default: the view in its window, the instruments round it |
| Basic | The same instruments without the frame: the view fills the screen |
| Enhanced | The overlay, with its own thin black margin and a ninety-degree field of view |

The change applies from the next level or new game. The Stock tier leaves the
HUD plugins exactly as upstream has them.

| Switch | Stored as | Tab |
|---|---|---|
| HUD (Classic, Basic, Enhanced) — Marathon only | `hud_style` | Look |

---

## IX · The room answers

### You hear the deck before you see it

> A pistol in a corridor and a pistol in a cargo hold are different weapons.

The game measures the space around you, through every open door, and the sound
takes its shape: tight in a duct, long in a hangar, thin and unanswered where
a deck stands open to the stars. Shut a door and the room behind it goes quiet
by degrees, not all at once.

Marathon's own music plays, from the Music folder bundled in the app. The
**Soundtrack** switch does not yet recognise a Marathon music pack: one that
supplies only numbered tracks is passed over, and the bundled music plays.

| Switch | Stored as | Tab |
|---|---|---|
| Room Reverb | `room_reverb` | Feel |
| Graded Sound Occlusion | `graded_occlusion` | Feel |
| Soundtrack | `soundtrack` | Art |

---

## X · What the terminals say

### She was brief. He is not.

> Read them. She wrote them for you while she was being taken apart. It would
> be rude not to.

The terminals are laid out exactly as they were, line for line and page for
page, and drawn at the sharpness of your display. The same is true of every
word the game puts on the screen.

| Switch | Stored as | Tab |
|---|---|---|
| Crisp Terminals | `crisp_terminals` | Look |
| Crisp Text | `crisp_text` | Look |

---

## XI · Five ways to run it

### From 1994 to the full dark

> Five ways to run the ship. I ran it one way for three centuries and nobody
> asked my opinion.

| Tier | What it is |
|---|---|
| **Stock** | Aleph One exactly: OpenGL, thirty frames a second, nothing added, the HUD plugins as upstream has them. |
| **Classic** | The Metal renderer, Marathon's own shading, crisp art and text, every frame your display can show. |
| **Enhanced** | Classic, with glow, cast light and its shadows, reverb and the new camera. |
| **Flagship** | Enhanced, with the air, travelling light, relief, depth, and the community's HD art. |
| **Rampant** | Flagship, with bounced light, traced shadows from every figure, traced ambient shadows and dust. |

Pick a tier, or switch any one thing and the tier becomes **Custom**. The
first run starts on Flagship. They are the same five tiers as in the other two
games, switch for switch; the liquid switches in them stay as they are and
have nothing to do (part III).

Measured on an Apple M5 with 16 GB, full screen at 1080p on a 240 Hz display,
uncapped, with the five HD packs installed: Flagship averaged 268 to 338
frames a second and Rampant 241 to 348, and the 1% low sat between 128 and 150
on every film. The worst single second was 162 frames. Memory at the end of a
run was about 0.4 to 0.7 GB. These figures are indicative: they come from the
first two minutes of each film, taken while other apps were busy on the same
Mac, and are not a settled baseline.

| Film | Flagship avg / 1% low / worst second | Rampant avg / 1% low / worst second |
|---|---|---|
| L1 Arrival | 287 / 133 / 174 | 247 / 130 / 196 |
| L8 Cool Fusion | 279 / 133 / 181 | 253 / 129 / 162 |
| L16 Neither High Nor Low | 268 / 130 / 227 | 241 / 128 / 180 |
| The lift (recorded for the purpose) | 338 / 150 / 223 | 348 / 138 / 232 |

---

## XII · Watching it back

### Nothing in a film is live

> It has all happened already. I watched it the first time, through every door
> you used.

A Marathon film is a list of keys pressed, replayed against the same random
numbers, and it only works if nothing in the game has changed by so much as
one. Nothing has. Upstream's twenty-seven test films, one for each level from
Arrival to Ingue Ferroque, replay in step three ways: stock, the default, and
everything switched on. Fifty-six checks, the same final number every way.
Marathon's films pass through Aleph One's conversion of the original data and
a film profile of their own, and come out exactly as upstream plays them; so
do its saves. No demo films ship with Marathon's data.

The map file holds thirty-seven levels: the twenty-seven of the campaign, and
the net maps, among them Mars Needs Women, Carnage Palace Deeee-Luxe, 5-D
space and Spiral Insanity.

The cheats on the Cheats tab are for looking around: Level Select, God Mode,
All Weapons and Ammo, Noclip. **Summon BOBs** is left out of Marathon. In the
later games the key calls in five armed security BOBs, and Marathon's BOBs
carry no weapons: press it here and the screen says so, and the film keeps
recording.

---

## XIII · Rampant

### More than this machine was built for

> More than this machine was built for. Metal renderer only.

Rampant is the fifth tier, above Flagship, with its own tab. Six switches, all
Metal only, all read-only on the game world: films, saves and the rules are
untouched. Aboard the Marathon three of them have all their work to do, one has
half of it, and two have none.

**Bounced light** (needs Light Redistribution). Light that reaches a surface
passes on to the next, frame after frame, so a lit room fills in its corners
and colour carries from wall to wall. Figures take the light of the floor and
ceiling around them. When a room's light changes for good, its surfaces settle
again rather than keeping the old light.

**Traced shadows** (needs Dynamic Lights and Light Shadows). Monsters, items,
scenery and corpses cast shadows from cast light, from their own silhouettes
rather than a blob: sharp at the feet, softer with distance, by the size of
the light. Figures cut shafts out of lit haze.

**Traced ambient shadows** (needs Ambient Shadows) are found by tracing short
rays through the level and the figures, so they know what is off screen and
behind things, within ten world units of you.

**Dust and embers.** Dust drifts in every room in view, within sixteen world
units, lit by the room and glinting where cast light passes through it. The
motes are made from the world time, so a film shows the same dust at the same
tick. The embers are the idle half: they rise only over real lava, and there
is none.

**Reflecting Liquids** and **Heat Shimmer** have nothing to do in Marathon:
there is no liquid to reflect in and no lava for the air to waver over. They
stay on the tab, and in the tier, and are simply idle.

**How it traces**

Rampant traces its rays on the GPU through Marathon's own map, the same
polygon walk the engine's line of sight uses, and not with the ray-tracing
hardware. The walk is exact where a map folds through itself, which a triangle
structure cannot represent. The Marathon 2 guide has the trial and its
figures.

**What it costs**

On the four films of part XI, Rampant averaged 241 to 348 frames a second and
held 128 or better at the 1% low. Its memory footprint at the end of a run was
0.8 to 1.0 GB.

| Switch | Stored as | Needs |
|---|---|---|
| Bounced Light | `light_bounce` | Light Redistribution |
| Traced Shadows | `traced_shadows` | Dynamic Lights, Light Shadows |
| Reflecting Liquids — idle in Marathon | `reflections` | Real Liquids |
| Traced Ambient Shadows | `traced_ambient` | Ambient Shadows |
| Dust and Embers — dust only in Marathon: no lava for embers | `dust_embers` | nothing |
| Heat Shimmer — idle in Marathon: no lava | `heat_shimmer` | Bloom |

---

## XIV · The edge of the known

### Where the lamps stop

> I would rather tell you where the lamps stop than have you find out.

Below Rampant, nothing is traced: cast light finds its shadows by walking the
map, which is exact for walls and blind to sprites; ambient shadows are read
from the picture, so they know only what is on screen; and light travels once
across a room and stops. Rampant changes each of those (part XIII).

Scenery can be models; the Pfhor cannot, because nobody has made them. Models
do not animate. The view pitches to about sixty-five degrees and no further.
In Rampant, figures cast shadows only from the four nearest lights bright
enough to matter, and dust is drawn within sixteen world units. Film export is
not available on the Metal display; screen recording does the job. The OpenGL
renderer is still there and gets none of this.

Not yet done: the 3D scenery has no switch of its own and rides on 3D Pickups;
Marathon music packs are not recognised by the Soundtrack switch; the
benchmarks rest on three of upstream's test films and one recorded for the
purpose. The signed, notarised app is on
[Releases](https://github.com/sebcarley/durandal/releases/latest) since 0.2.0. Durandal Marathon needs macOS 12
or later on Apple Silicon, and has been tested on macOS 27 only.

And none of it makes the ship safer. You can see further down the corridor. So
can whatever is in it.

---

**Where it keeps things.** Durandal Marathon is its own app, one of three
sharing the engine, with its own folders:

- settings: `~/Library/Preferences/Durandal Marathon/`
- saves, films, screenshots, plugins: `~/Library/Application Support/Durandal Marathon/`
  (films in `Recordings`, HD packs in `Plugins`)
- log: `~/Library/Logs/Durandal Marathon Log.txt`
- shared with the other two games: the texture cache (`~/Library/Caches/Durandal`)
  and the crash record (`~/Library/Logs/Durandal Crash.txt`)

The game data is Bungie's freely released Marathon data, in its original
format, bundled inside the app with its music. From source:
`GAME=m1 scripts/build.sh`, or the Xcode scheme **Marathon 1**. The tier is
stored as `quality_tier`: Rampant is 5, and Custom stays 4.

**Credits.** *Marathon* is Bungie's, and its data and source were released by
them. Durandal Marathon stands on [Aleph
One](https://github.com/Aleph-One-Marathon/alephone) and thirty years of work
by its developers, under the GPL 3. The HD art is the work of Tim Vogel,
Zetren, Hopper, Rock and General Tacticus, and the other community authors
named in
[HD_ASSETS.md](https://github.com/sebcarley/durandal/blob/durandal/main/docs/HD_ASSETS.md).
None of it is distributed here: the engine fetches it from its authors' own
pages, and hosts none of it.

Durandal Marathon is an unofficial project. It is not affiliated with or
endorsed by Bungie, and Marathon, its worlds and its characters belong to
them.

The other two guides: [Durandal](GUIDE.md) (Marathon 2) and [Durandal
Infinity](GUIDE-infinity.md). What the project is and how to build it:
[DURANDAL.md](DURANDAL.md).
