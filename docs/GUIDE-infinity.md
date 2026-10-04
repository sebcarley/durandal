# Durandal Infinity — a field guide

*Marathon Infinity · a native Metal engine for Apple Silicon · Autumn 2026*

*This is the plain edition, which reads on GitHub. The illustrated edition,
written in several timelines by whoever held the terminal, is
[GUIDE-infinity.html](GUIDE-infinity.html).*

Durandal won at Lh'owon, and it did not matter. The Pfhor, losing, did
something to the sun, and there was a thing asleep inside it that the Jjaro
had put there a very long time ago and meant never to wake. Now you fall from
one version of events into the next: serving Durandal in one, Tycho in
another, dreaming in between, looking for the telling in which it ends
differently.

This is the same game Bungie shipped in 1996, tick for tick. What has changed
is how much of each timeline you can see before it fails.

**Contents**

1. [The light](#i--the-light)
2. [The air](#ii--the-air)
3. [The liquids](#iii--the-liquids)
4. [The old colours](#iv--the-old-colours)
5. [Depth](#v--depth)
6. [Built by other hands](#vi--built-by-other-hands)
7. [The eye](#vii--the-eye)
8. [The room answers](#viii--the-room-answers)
9. [What the terminals say](#ix--what-the-terminals-say)
10. [Five ways to run it](#x--five-ways-to-run-it)
11. [Watching it back](#xi--watching-it-back)
12. [Rampant](#xii--rampant)
13. [The edge of the known](#xiii--the-edge-of-the-known)

Durandal Infinity is Marathon Infinity for Apple Silicon: the same Metal
renderer as Durandal, with lighting, liquids, air and sound built on top of
it, reading Infinity's own walls, its own lamps and its fifth liquid. It does
not change what Infinity is. Films replay, saves load, the Pfhor are exactly
as fast and exactly as many.

Each part opens with what the thing is, then goes deeper. Everything is in
**Preferences > DURANDAL** and can be switched off there. The table at the
foot of each part gives the switch and the name it is stored under. Every
switch works in Infinity as it does in Marathon 2; what is Infinity's own is
its wall lamps, its fifth liquid and its HD art.

---

## I · The light

### The same lamps, in every telling

> I lit this place for you in another timeline. You did not get this far.

Infinity's levels were lit by hand, surface by surface, and that light is
kept: every wall still averages the brightness its designer gave it. What is
new is everything that moves through it.

**Cast light.** A fusion bolt carries its light down the corridor with it. A
compiler's shot lights the wall it passes and the floor it dies on. A
Fighter's staff, a grenade, the flash at the end of your own barrel: up to
sixteen at once, each the colour of the thing that made it. **And its
shadow:** the light stops where Marathon says a line of sight stops. It walks
the map polygon by polygon, as a Trooper's aim does, so it works where the map
folds through itself. Infinity's maps fold a good deal.

**Glow.** Infinity has its own list of wall lamps: fifty-three of them, found
by going through every one of its wall sets by eye. Lights, screens, switches,
lava, and for the first time the Jjaro set, which belongs to this game alone.
On a display that can go brighter than white, they do. The sky's brightest
parts bloom.

**Light that travels.** A lamp at one end of a hall now reaches a little way
along it. Light gathers from the lamps, the openings and the sky, settles over
a second or so, and leaves each surface as bright on average as it always was.
Unlit rooms do not blotch, and the surfaces a moving door uncovers start
clean.

**The weapon in your hands** takes the light around you: it warms as a bolt
passes close, and flares with its own fire.

**Relief.** Under the headlight the old wall art stands up a little, seams and
glyphs catching the light on one side; HD walls bring their own.

Infinity numbers its five wall sets as Marathon 2 does, water, lava, sewage,
Jjaro and Pfhor, but every one was drawn again: not one of Infinity's wall
pictures is the same as Marathon 2's.

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
| Normal Maps (HD walls) | `normal_maps` | Art |

---

## II · The air

### Each place has its own, and so does each telling

> You will learn to tell the timelines apart by the air. This one smells of
> burning. Most of them do.

The air has weight now. It lies thicker near the floor, drifts in slow
patches, and takes its light from the room it is in, so a dark room keeps dark
air and a bolt crossing it lights the haze as it goes. Over lava the smoke
rises and glows from beneath. It is held in three dimensions, in the shape of
the map, and stops at walls. Where a scenario declares its own fog, that
becomes the colour and thickness of the air instead.

Each kind of place has its own haze, by the same table as Marathon 2: water,
lava, sewage, Jjaro and Pfhor. The Jjaro air is defined and has not yet been
looked at in play.

| Switch | Stored as | Tab |
|---|---|---|
| Volumetric Fog | `volumetric_fog` | Light |
| Fog Strength (Light, Medium, Thick) | `fog_strength` | Light |

---

## III · The liquids

### Five of them, and one is not from here

> Water, lava, sewage, goo. And a fifth, which the Jjaro left, and which I
> would not drink.

Stand at the edge and look down. The floor of the pool is there, further off
than it looked, and darker the deeper it lies. Water takes the red out of what
is under it first. Sewage takes nearly everything. Lava shows you nothing and
lights the ceiling instead.

Infinity has five liquids: water, lava, sewage, goo, and one of its own, the
Jjaro liquid. Real liquids and their caustics cover all five, and their
reflections all but lava. The Jjaro liquid's look was defined with the others, and it is still to
be seen in play.

The surface moves and catches the light: a sheen at a low angle, a glint under
the headlight, a flare as a bolt crosses it. Beneath it the light comes down
in moving bands. Go under and the world wavers, the sound closes in, and what
you can see is decided by what you are swimming in.

| Switch | Stored as | Tab |
|---|---|---|
| Real Liquids | `liquids` | Light |
| Smooth Liquids, Lights, Fades | `smooth_world` | Feel |

---

## IV · The old colours

### 256 of them, chosen by hand

> Infinity never darkened a colour. It chose a darker one. It has a great many
> dark ones to choose from.

The original game did not dim a pixel by multiplying it. Each colour sat on a
ramp drawn by an artist, and shadow meant walking down that ramp to the next
colour along. It is why Marathon's darkness has the hues it has, and it is
reproduced here exactly, in the original bands or smoothly between them. Each
texel of the old art takes one shade, not a gradient smeared across it, so 128
by 128 stays crisp at any distance; far away it is filtered properly and stops
shimmering. If the whole picture sits too dark or too flat on your display,
the scene can be graded without touching the HUD.

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

> You will see the shadow on the floor before you see what casts it. In some
> timelines that is all the warning you get.

**Ambient shadows** darken the places light has trouble reaching: the foot of
a wall, the inside of a corner, the floor behind a crate. **Character
shadows** lie on the floor in the shape of whatever casts them: when something
nearby is burning, the shadow falls away from it and moves as it moves;
otherwise it falls forward, where you can see it. **Contact shadows** sit
under items, scenery and the dead. **Distance shade** lets the far end of a
long hall fall away into the dark beyond your headlight, and is off until you
ask for it.

| Switch | Stored as | Tab |
|---|---|---|
| Ambient Shadows | `ambient_shadows` | Light |
| Character Shadows | `character_shadows` | Light |
| Contact Shadows | `contact_shadows` | Light |
| Distance Shade (0 to 100%) | `distance_shade` | Look |

---

## VI · Built by other hands

### Thirty years of other people's work

> Whatever else fails, the crew that redrew this world is there in every
> telling. I checked.

The community has redrawn Infinity at many times its resolution: walls and
skies, monsters, scenery, the weapons in your hands, the pickups on the floor.
It is theirs: Durandal Infinity fetches it but never ships it. One button,
**Get HD Art** on the Art tab, brings five packs from their authors' own
pages, and any other Aleph One pack still works dropped into the Plugins
folder. Choose, by kind, what to take.

| Line in the dialog | What it is | Download |
|---|---|---|
| CFP - Walls MInf | Community/Freeverse Walls MInf 1.1 (herecomethej2000): Infinity's walls at 1024 by 1024 (Goran Svensson's sets with TheMan's and kaosof's work), the 4K landscapes, normal maps on all five wall sets | about 425 MB |
| CFP Monsters | The same pack as Marathon 2's; it covers Infinity, VacBobs included | about 516 MB |
| CFP Scenery | The same pack as Marathon 2's | about 24 MB |
| CFP Weapons MInf | Community/Freeverse Plugin, Weapons MInf 2.4 (herecomethej2000): Infinity's weapons and items | about 73 MB |
| 3D Items | thedoctor45's 3D pickups, made for Infinity | about 3 MB |
| **All five** | | **about 1.0 GB** |

The walls are the Community/Freeverse Walls for Infinity, at 1024 by 1024:
Goran Svensson's sets with TheMan's and kaosof's work, landscapes at 4K, and
normal maps on all five wall sets. The weapons and items are the
matching Infinity pack. The monsters and the scenery are the same packs
Marathon 2 uses, which cover Infinity down to the Bobs in vacuum suits, and
the 3D pickups are thedoctor45's, made for this game.

It fetches one pack at a time, each line showing how far it has got, while the
dialog stays open. A pack already in the Plugins folder is left alone. Each
download is checked against the version the engine was tested with; a newer
one installs anyway and says so. Close cancels the one in progress and keeps
what is done. New packs join the plugin list at once and their art loads from
the next level, wherever the Art tab's HD switches are on: Flagship, Rampant,
or Custom. The same five can be fetched from a terminal with `GAME=inf
scripts/get-hd-art.sh`.

Each image is compressed once and kept. The first visit to each kind of place
builds its share of that cache, a frame or two of a second or more, once. The
cache is shared with Durandal and keyed by file, so the packs both games use
are built only once between them.

| Switch | Stored as | Tab |
|---|---|---|
| Get HD Art... (a button: fetches the five packs from their authors' pages) |  | Art |
| HD Walls and Sky, Monsters, Weapons and Items, Scenery | `hd_walls`, `hd_monsters`, `hd_weapons`, `hd_scenery` | Art |
| 3D Pickups, Spinning Pickups | `models_3d`, `spin_pickups` | Art |
| Texture Cache | `texture_cache` | Art |

The packs, their authors and where to find them are in
[HD_ASSETS.md](HD_ASSETS.md), section 11.

---

## VII · The eye

### You could never look up

> Thirty degrees of aim. He calls the rest of the view a gift. It is a longer
> chain.

Marathon tilted the picture rather than the head. Now the camera turns: look
up and the walls converge above you, as walls do. Aim still stops at thirty
degrees, because the game's rules say so, but the view does not, and the
crosshair stays behind on the true aim so you always know where the shot will
go.

The sky stays fixed to the world while your head moves. Sidestep and the
horizon leans a little into the turn. The view fills a wide screen without
stretching: Infinity has the same built-in HUD as Marathon 2, so
**Widescreen** works here as it does there. The mouse is read every frame, not
every tick, and the world between ticks is drawn in between, up to whatever
your display can show.

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

> Fire a shot and the room tells you its size. Fire enough of them and it
> tells me where you are.

The game measures the space around you, through every open door, and the sound
takes its shape: tight in a duct, long in a cistern, thin and unanswered under
an open sky. Shut a door and the room behind it goes quiet by degrees, not all
at once. Under the surface the high notes go first. If you have installed a
soundtrack, choose one.

| Switch | Stored as | Tab |
|---|---|---|
| Room Reverb | `room_reverb` | Feel |
| Graded Sound Occlusion | `graded_occlusion` | Feel |
| Soundtrack | `soundtrack` | Art |

---

## IX · What the terminals say

### Half are his, half are mine

> Read them. Half are his, half are mine, and a few belong to nobody you have
> met.

The terminals are laid out exactly as they were, line for line and page for
page, and drawn at the sharpness of your display. The same is true of every
word the game puts on the screen. Infinity's terminals are the strangest in
the three games, and not all of them were written by anyone you could name.
They are untouched.

| Switch | Stored as | Tab |
|---|---|---|
| Crisp Terminals | `crisp_terminals` | Look |
| Crisp Text | `crisp_text` | Look |

---

## X · Five ways to run it

### From 1996 to the full dark

> Five measures. Four were given, one was taken. All are yours to weigh.

| Tier | What it is |
|---|---|
| **Stock** | Aleph One exactly: OpenGL, thirty frames a second, nothing added. |
| **Classic** | The Metal renderer, Marathon's own shading, crisp art and text, wide screens, every frame your display can show. |
| **Enhanced** | Classic, with glow, cast light and its shadows, liquids, reverb and the new camera. |
| **Flagship** | Enhanced, with the air, travelling light, relief, depth, and the community's HD art. |
| **Rampant** | Flagship, with bounced light, traced shadows from every figure, reflecting liquids, traced ambient shadows, dust and embers, and heat shimmer. |

Pick a tier, or switch any one thing and the tier becomes **Custom**. The
first run starts on Flagship. Every switch on the DURANDAL dialog works in
Infinity as it does in Marathon 2, in the same five tiers.

Measured on an Apple M5 with 16 GB, full screen at 1080p on a 240 Hz display,
uncapped, with the five HD packs installed, on the four demo films that ship
with the game: Flagship averaged 267 to 278 frames a second with a 1% low of
131 to 134, and Rampant 219 to 241 with a 1% low of 113 to 122. The worst
single second was 143 frames. Memory at the end of a run was about 0.6 to 1.0
GB. These figures are indicative: they come from the first two minutes of each
film, taken while other apps were busy on the same Mac, and are not a settled
baseline.

| Film (the four demos) | Flagship avg / 1% low / worst second | Rampant avg / 1% low / worst second |
|---|---|---|
| LA COSA NOSTRA | 268 / 134 / 187 | 230 / 116 / 143 |
| MORPHINE | 267 / 132 / 169 | 219 / 116 / 157 |
| WRATH NO MORE | 278 / 134 / 194 | 241 / 122 / 171 |
| YORRICK | 271 / 131 / 169 | 233 / 113 / 181 |

---

## XI · Watching it back

### Nothing in a film is live

> What was done is kept. What is kept can be shown. What is shown is not
> changed.

A Marathon film is a list of keys pressed, replayed against the same random
numbers, and it only works if nothing in the game has changed by so much as
one. Nothing has. Upstream's thirty-two test films, levels 1 to 33 with level
26 missing, replay in step three ways: stock, the default, and everything
switched on. Sixty-six checks, the same final number every way. Infinity's
gameplay, films and saves are exactly as upstream plays them.

Four demo films ship with the game: LA COSA NOSTRA, MORPHINE, WRATH NO MORE
and YORRICK. The map file holds fifty-seven levels. Thirty-three are for solo
play, from Ne Cede Malis to You Think You're Big Time? You're Gonna Die Big
Time!, the three Electric Sheep dream levels among them. The other twenty-four
are net maps: Duality, Thrud, Wrath No More?, Morpfhine, La Cosa Nostra,
Fortress Lh'owon and the rest.

The cheats on the Cheats tab are for looking around: Level Select, God Mode,
All Weapons and Ammo, Noclip. **Summon BOBs** works as in Marathon 2: press C
and five armed security BOBs teleport in beside you and go for the nearest
aliens, placed only where they could have walked to you, on your level and
clear of lava, goo and deep water; if there is no room the screen says so.
Single-player only, and a summon ends that game's film recording. A game saved
after one restores with its BOBs.

---

## XII · Rampant

### More than this machine was built for

> More than this machine was built for. Metal renderer only.

Rampant is the fifth tier, above Flagship, with its own tab. Six switches, all
Metal only, all read-only on the game world: films, saves and the rules are
untouched. In Infinity all six have work to do. They read only what the three
games share, so nothing in them needed changing for this one.

**Bounced light** (needs Light Redistribution). Light that reaches a surface
passes on to the next, frame after frame, so a lit room fills in its corners
and colour carries from wall to wall. The sky gives off the landscape's own
average colour; figures take the light of the floor and ceiling around them.
When a room's light changes for good, its surfaces settle again rather than
keeping the old light.

**Traced shadows** (needs Dynamic Lights and Light Shadows). Monsters, items,
scenery and corpses cast shadows from cast light, from their own silhouettes
rather than a blob: sharp at the feet, softer with distance, by the size of
the light. Grates throw patterned shadows, and figures cut shafts out of lit
fog.

**Reflecting liquids** (needs Real Liquids). The surface reflects the room
above it, the sky, and whoever is standing at the edge, broken by ripples;
what lies below bends. Not lava, and not from under the surface. **Traced
ambient shadows** (needs Ambient Shadows) are found by tracing short rays
through the level and the figures, so they know what is off screen and behind
things, within ten world units of you. **Dust and embers** drift in every room
in view, lit by the room and glinting where cast light passes; over lava,
embers rise and fade as they climb, and **heat shimmer** (needs Bloom) makes
the air above it waver. The motes are made from the world time, so a film
shows the same dust at the same tick.

**How it traces**

Rampant traces its rays on the GPU through Marathon's own map, the same
polygon walk the engine's line of sight uses, and not with the ray-tracing
hardware. The walk is exact where a map folds through itself, which a triangle
structure cannot represent. The Marathon 2 guide has the trial and its
figures.

**What it costs**

On the four demo films Rampant cost 13 to 18 per cent of the Flagship frame
rate, and held 113 or better at the 1% low. Its memory footprint at the end of
a run was 0.8 to 1.0 GB. With the HD art installed it was checked on screen on
all four: walls, landscapes, weapons and monsters replaced, wall lamps
glowing.

| Switch | Stored as | Needs |
|---|---|---|
| Bounced Light | `light_bounce` | Light Redistribution |
| Traced Shadows | `traced_shadows` | Dynamic Lights, Light Shadows |
| Reflecting Liquids | `reflections` | Real Liquids |
| Traced Ambient Shadows | `traced_ambient` | Ambient Shadows |
| Dust and Embers | `dust_embers` | nothing |
| Heat Shimmer | `heat_shimmer` | Bloom |

---

## XIII · The edge of the known

### What has not been seen yet

> I have told you some of it four times. There is a part I have not been able
> to tell you in any of them.

Below Rampant, nothing is traced: cast light finds its shadows by walking the
map, which is exact for walls and blind to sprites; ambient shadows are read
from the picture, so they know only what is on screen; the liquids neither
reflect nor refract; and light travels once across a room and stops. Rampant
changes each of those (part XII).

Pickups can be models; the Pfhor cannot, because nobody has made them, and the
sprites are better than models would be. Models do not animate. The view
pitches to about sixty-five degrees and no further. A sky is a picture, and it
ends: the HD skies cover about forty-five degrees above and below the horizon,
and past the top you see the colour of the picture's edge. In Rampant,
reflections use the wall art at 256 pixels and the figures in them are flat
cut-outs; figures cast shadows only from the four nearest lights bright enough
to matter; dust is drawn within sixteen world units. Film export is not
available on the Metal display; screen recording does the job. The OpenGL
renderer is still there and gets none of this.

Not yet seen or checked in Infinity: the Jjaro liquid and the Jjaro air in
play, and the reverb beneath that liquid; the dream levels and 5D space played
right through; the very largest levels against the travelling light's store;
and benchmark films recorded for the purpose, for which the four demos stand
in. The signed, notarised app is on
[Releases](https://github.com/sebcarley/durandal/releases/latest) since 0.2.0. Durandal Infinity needs macOS 12 or later
on Apple Silicon, and has been tested on macOS 27 only.

And none of it makes the game easier, or any clearer. The light shows you the
room. It does not tell you which timeline the room is in.

---

**Where it keeps things.** Durandal Infinity is its own app, one of three
sharing the engine, with its own folders:

- settings: `~/Library/Preferences/Durandal Infinity/`
- saves, films, screenshots, plugins: `~/Library/Application Support/Durandal Infinity/`
  (films in `Recordings`, HD packs in `Plugins`)
- log: `~/Library/Logs/Durandal Infinity Log.txt`
- shared with the other two games: the texture cache (`~/Library/Caches/Durandal`)
  and the crash record (`~/Library/Logs/Durandal Crash.txt`)

The game data is Bungie's freely released Marathon Infinity data, bundled
inside the app. From source: `GAME=inf scripts/build.sh`, or the Xcode
scheme **Marathon 3**. The tier is stored as `quality_tier`: Rampant is 5,
and Custom stays 4.

**Credits.** *Marathon Infinity* is Bungie's, and its data and source were
released by them. Durandal Infinity stands on [Aleph
One](https://github.com/Aleph-One-Marathon/alephone) and thirty years of work
by its developers, under the GPL 3. The HD art is the work of
herecomethej2000, Goran Svensson, TheMan, kaosof, thedoctor45 and the other
community authors named in
[HD_ASSETS.md](https://github.com/sebcarley/durandal/blob/durandal/main/docs/HD_ASSETS.md).
None of it is distributed here: the engine fetches it from its authors' own
pages, and hosts none of it.

Durandal Infinity is an unofficial project. It is not affiliated with or
endorsed by Bungie, and Marathon, its worlds and its characters belong to
them.

The other two guides: [Durandal](GUIDE.md) (Marathon 2) and [Durandal
Marathon](GUIDE-marathon.md). What the project is and how to build it:
[DURANDAL.md](DURANDAL.md).
