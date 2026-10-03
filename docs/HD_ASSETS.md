# Hi-res art for Marathon 2: what exists, and what the Metal loader must support

Research for Durandal, 27 September 2026. Nothing in this document is engine
code; it is the reference for the round that gives the game an in-game option
for enhanced textures and sprites. The 1995 art is low resolution, the enemy
and weapon sprites most of all, and that is where the packs below help most.

Method: every source in the brief was fetched and checked (Simplici7y pages,
the Aleph One wiki, GitHub through its API, PCGamingWiki through its MediaWiki
API, lochnits.com); every pack that could be fetched was downloaded into the
git-ignored `Assets/` folder, unpacked and inspected (`Plugin.xml`, the MML,
image formats with `file` and `sips`, shapes patches with a small parser). The
archives' SHA-256 sums are in `Assets/SHA256SUMS.txt`. No third-party art is
in git: `/Assets/` is ignored (`.gitignore`), and the game will load the packs
as user-installed plugins.

## Summary

- Everything hi-res for Marathon 2 descends from one source: the art Freeverse
  made for the Xbox Live Arcade "Marathon: Durandal" (2007). Bungie released
  it to the Aleph One project in 2011 as three plugins; Aleph One 1.5 (2021)
  stopped bundling them and they now live on a GitHub release. The community
  then cleaned, upscaled and extended them: the Community Freeverse Pack (CFP)
  is the current, maintained line (2025-26), the XBLA SuperPlugin the older
  all-in-one (2012-24).
- The official download page still says the Marathon 2 release includes the
  XBLA textures and monsters. It does not: the 20250829 DMG bundles only the
  HUD, theme, stats and Transparent Liquids plugins (checked by mounting it).
- Marathon 2 has no 3D model set. The only models that run in M2 are the
  "3D Items Plugin" (21 OBJ pickups made for Infinity, reported to work in M2)
  and its "Rotating Items" script. The 3D scenery pack is Marathon 1 only. No
  monster or weapon models exist for any Marathon game.
- Normal maps for M2 exist in exactly one pack: CFP Walls M2 2.1 (all 120
  walls). Glow maps exist in CFP (walls 36, scenery 14, weapons 17, monsters),
  the SuperPlugin, and lj6014's bloom pack for the stock walls. No pack has
  normal maps for sprites.
- The packs are plain Aleph One plugins: `Plugin.xml`, MML `<opengl><texture>`
  entries pointing at PNG or DDS files, and, for sprites, shapes patches
  (`.ShPa`) that give the collections new frame geometry. Every shapes patch
  inspected changes only bitmaps, frame rectangles and the sprite scale
  (`pixels_to_world`); none changes a sequence's timing, so films and gameplay
  are unaffected.
- Marathon 2's shapes have no Jjaro wall set (collection 20), no Jjaro scenery
  (25) and no VacBob (13): those collections are empty in `Shapes.shpA`. A pack
  covering 17, 18, 19 and 21 therefore covers every M2 wall set.
- The Metal renderer already draws replacement PNG textures and their glow
  images. It does not yet load DDS (the compressed packs), read `offset_image`
  normal maps, or run the bloom pass; see section 4.10.

## 1. Resource table

Sizes are the download archive; "inspected" means unpacked and examined here.
Licence: "Bungie notice" is the readme shipped with the official plugins:
"Art assets in this plugin were developed by Freeverse Software for
'Marathon 2: Durandal', released for Xbox Live Arcade. The content is
copyright Bungie. Please contact Bungie before using these assets for any
commercial purpose, or any purpose not directly connected with the Marathon
community." Packs marked "derived" contain the same art re-encoded or
upscaled and carry the same status whether or not they say so.

### Marathon 2 packs

| # | Pack | Author(s) | Version, date | Target | Content | Maps | Format, resolution | Size | Where | Licence | Inspected |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | HD Textures (official XBLA plugin) | Freeverse; plugin by Aleph One (Hopper, treellama) | 1.0, release 30 Dec 2021 | M2 | 120 walls (coll 17, 18, 19, 21), 4 landscapes (27-30) | base | DDS DXT5 512x512 with 9 mip levels (chain lacks the 1x1 level); landscapes PNG 1024x540 | 20.7 MB | [data-marathon-2 release plugin-removal](https://github.com/Aleph-One-Marathon/data-marathon-2/releases/tag/plugin-removal) | Bungie notice in Readme.txt | yes |
| 2 | HD Monsters (official XBLA plugin) | Freeverse "et al."; Aleph One | 1.0, 30 Dec 2021 | M2 | 13 monster collections (2, 3, 5, 6, 8, 9, 10, 11, 12, 14, 15, 16, 31) and 4 scenery sets (22, 23, 24, 26): 3,297 texture entries, one per colour table | base | 1,978 DDS DXT5 (mostly 256x256 and 256x128, largest 512x512) plus 1,057 small PNGs; 17 shapes patches | 30.7 MB (74 MB unpacked) | same release | Bungie notice | yes |
| 3 | HD Weapons (official XBLA plugin) | Freeverse; Aleph One | 1.0, 30 Dec 2021 | M2 | weapons in hand (coll 1, 48 entries), projectiles (4, 70), items (7, 21) | base | DDS DXT5, 32x32 to 512x512; 3 shapes patches | 2.1 MB (11 MB unpacked) | same release | Bungie notice | yes |
| 4 | XBLA Textures Plugin | treellama | 20120128, 10 Feb 2012 | M2 | same 124 entries as #1 | base | DDS DXT5 512x512 plus 231 PNG originals | 25.3 MB | [simplici7y.com/items/xbla-textures-plugin](https://simplici7y.com/items/xbla-textures-plugin/) | none stated; derived | yes |
| 5 | XBLA Monsters Plugin | treellama | 20120128, 10 Feb 2012 | M2 | identical to #2 (same Plugin.xml, MML and patches) | base | as #2 | 30.8 MB | [simplici7y.com/items/xbla-monsters-plugin](https://simplici7y.com/items/xbla-monsters-plugin/) | Bungie notice | yes |
| 6 | Freeverse's HD Textures Plugin (M2) @ 1024x1024 | Zetren | undated, 18 Dec 2016 | M2 | 120 walls, 4 landscapes ("Freeverse's HD skyboxes") | base | PNG 1024x1024 | 205 MB (197 MB unpacked) | [simplici7y.com/items/freeverse-s-hd-textures-plugin-m2-1024x1024](https://simplici7y.com/items/freeverse-s-hd-textures-plugin-m2-1024x1024/) | none stated; derived | yes |
| 7 | Community/Freeverse Plugin - Walls M2 (plugin name "CFP - Walls M2") | herecomethej2000 (Joshua Pettus); Freeverse, Hopper, Treellama, TheDoc45, General Tacticus, J2k, Spurious Interrupt, Tfear7, Zetren | page 2.1.1 (1 Jan 2026), Plugin.xml says 2.2 | M2 only (its readme) | 120 walls (Zetren's 1024 walls with the SuperPlugin's bloom values) and 4 landscapes; 124 entries | base, glow (36, with `minimum_glow_intensity` on 14), normal (all 120) | PNG: walls 1024x1024, landscapes 4096x2160; paths `XBLA/ShapesHD/<coll>/0/<bitmap>.png`, `<bitmap>a.png` glow, `<bitmap>b.png` offset | 391 MB "CFP M2 Walls Must Unzip.zip" (374 MB unpacked) | [simplici7y.com/items/community-freeverse-plugin-walls-m2](https://simplici7y.com/items/community-freeverse-plugin-walls-m2/) (Google Drive) | GPL-3.0 on the scripts repo; art derived; readme credits only | yes |
| 8 | Community/Freeverse Plugin - Scenery (CFP Scenery) | herecomethej2000 and the CFP contributors | 2.6, 3 Dec 2025 | M2 and Infinity | scenery coll 22-26, 183 entries | base, glow (14) | PNG, sprite-sized (272x400 to 768x216) | 25.1 MB | [simplici7y.com/items/community-freeverse-plugin-scenery](https://simplici7y.com/items/community-freeverse-plugin-scenery/) (Drive) | as #7 | yes |
| 9 | Community/Freeverse Plugin - Monsters (plugin name "CFP Monsters") | herecomethej2000 and contributors (Hippieman's HD renders, General Tacticus's VacBob and critters) | page 2.17 (23 Nov 2025), Plugin.xml says 2.15 | M2 and Infinity | 15 MML files, 4,240 entries over collections 2, 3, 5, 6 (1,632), 7 (critters), 8, 9, 10, 11, 12, 13, 14, 15, 16, 31; the VacBob (13) and critter entries target collections M2 does not have | base, glow (1,226 entries), bloom shifts on 402 | PNG, frames about 450 px tall (156x449 to 768x640), 5,393 files; 14 shapes patches | 541 MB "CFP Monsters Must Unzip.zip" (529 MB unpacked; unzip it, or levels load for minutes) | [simplici7y.com/items/community-freeverse-plugin-monsters](https://simplici7y.com/items/community-freeverse-plugin-monsters/) (Drive) | as #7 | yes |
| 10 | Community/Freeverse Plugin - Weapons M2 (CFP Weapons M2) | herecomethej2000 and contributors | 2.4 on the page, Plugin.xml says 2.3; 31 Aug 2025 | M2 | coll 1, 4, 7: 139 entries | base, glow (17), bloom shifts on 46 | PNG up to 2048x2048; 3 shapes patches | 68.7 MB | [simplici7y.com/items/community-freeverse-plugin-weapons-m2](https://simplici7y.com/items/community-freeverse-plugin-weapons-m2/) (Drive) | as #7 | yes |
| 11 | Marathon Durandal XBLA HD Graphics SuperPlugin (plugin name "M2 XBLA HD Graphics Superplugin v2.3") | thedoctor45 | 2.3, 19 Aug 2024 | M2 | one plugin for everything: 20 MML files, 3,904 entries over every M2 collection (walls 17-19 and 21, landscapes, scenery 22-24 and 26, all monsters, weapons, projectiles, items); 18 shapes patches | base, glow on 467 entries as `glow_image` + `glow_mask` (`<bitmap>_glow.png`), bloom shifts on 341, normal maps on 8 water walls only | PNG: walls 512x512, sprites up to about 800 px; landscapes PNG plus `XBLA Support/moon.dds` (4096x2160 DXT1); 5,417 PNG, 46 DDS | 392 MB "M2 XBLA HD Graphics v2.3.zip" (430 MB unpacked) | [simplici7y.com/items/marathon-durandal-xbla-hd-graphics-superplugin-v2](https://simplici7y.com/items/marathon-durandal-xbla-hd-graphics-superplugin-v2/) (MediaFire) | none stated, no readme; derived | yes |
| 12 | Extra XBLA HD Monsters Plugin (XHD Monsters) | treellama, renders found by Hippieman | 0.2, 27 Feb 2024 | M2 | tick, hunter, trooper, fighter, yeti (F'lickta), compiler, cyborg: 942 entries | base | DDS DXT5, typically 320x640, up to 640x644; 7 shapes patches | 50.4 MB (239 MB unpacked) | [simplici7y.com/items/extra-xbla-hd-monsters-plugin](https://simplici7y.com/items/extra-xbla-hd-monsters-plugin/) | Bungie notice | yes |
| 13 | ULTRA HD Weapons M2 | ParallaxTiger, from Freeverse's HD Weapons | 1, 31 Oct 2025 | M2 only | same 139 entries as #3, AI-upscaled 4x | base | DDS DXT5, 128x128 to 4096x4096; 3 shapes patches | 21.9 MB (245 MB unpacked) | [simplici7y.com/items/ultra-hd-weapons-m2](https://simplici7y.com/items/ultra-hd-weapons-m2/) | Bungie notice plus author's readme | yes |
| 14 | M2 AI Textures | W'rkncacnter | 1.0, 1 Apr 2023 | M2 | 120 walls, 4 landscapes; "based on the originals, created with AI" | base | DDS DXT1 1024x1024 (2 DXT5); landscapes DDS RGB 2048x1080 | 66.9 MB | [simplici7y.com/items/m2-ai-textures](https://simplici7y.com/items/m2-ai-textures/) | none stated; derived from Bungie's originals | yes |
| 15 | M2 Upscaled Textures | W'rkncacnter | 1.4, 25 Sep 2022 | M2 | 120 walls, Gigapixel AI upscale of the originals | base | DDS DXT1 512x512 (2 DXT5) | 13.0 MB | [lochnits.com/marathon/upscales](https://lochnits.com/marathon/upscales/) (`M2_Upscaled_Textures_v1.4.7z`) | none stated; derived | yes |
| 16 | M2 Upscaled Landscapes | W'rkncacnter | 1.1, 25 Nov 2020 | M2 | 4 landscapes | base | DDS RGB 2048x1080 | 15.5 MB | same site (`M2_Upscaled_Landscapes_v1.1.7z`) | none stated; derived | yes |
| 17 | M2 Pepperscapes | W'rkncacnter | 1.0, 1 Apr 2024 | M2 (map checksums 762432692 Mac, 1577821584 Windows) | a different landscape for each of the 41 levels, done with a `<map_patch>` that injects per-level MML resources (TEXT 128 and 13000-13040) pointing each level's landscape collection at its own image, plus a `<landscapes>` element setting the aspect exponents | base | DDS uncompressed RGB 4096x1536, 41 files (984 MB unpacked) | 583 MB | [simplici7y.com/items/m2-pepperscapes](https://simplici7y.com/items/m2-pepperscapes/) | none stated | yes |
| 18 | TTEP Plugin (M2) | Tim Vogel (tim4i) | 7.0, 20 Nov 2010 | M2 | 120 walls, remade rather than upscaled | base | DDS DXT1 1024x1024 (3 DXT5) | 63.8 MB | [simplici7y.com/items/ttep-plugin-m2](https://simplici7y.com/items/ttep-plugin-m2/) | none stated | yes |
| 19 | High-res Landscapes Plugin (M2) | Tim Vogel | 1.1, 20 Nov 2010 | M2 | 4 landscapes | base | DDS RGB 2048x1080 | 8.1 MB | [simplici7y.com/items/high-res-landscapes-plugin-m2](https://simplici7y.com/items/high-res-landscapes-plugin-m2/) | none stated | yes |
| 20 | Marathon 2: Durandal Wall Bloom | lj6014 | 1.0, 1 Apr 2020 | M2 | glow for 30 stock walls (17 x7, 18 x7, 19 x6, 21 x10); values from the SuperPlugin | glow with mask, bloom | PNG 128x128 `glow_image` and `glow_mask` | 0.8 MB | [simplici7y.com/items/marathon-2-durandal-wall-bloom](https://simplici7y.com/items/marathon-2-durandal-wall-bloom/) | none stated | yes |
| 21 | Pfhor Textures Remix | President People | 2.0, 5 Aug 2012 | M2 and Infinity | coll 21, 30 walls blending M2 and Infinity Pfhor art | base | PNG 128x128 | 0.9 MB | [simplici7y.com/items/pfhor-textures-remix](https://simplici7y.com/items/pfhor-textures-remix/) | none stated | yes |
| 22 | M2 Preview Textures Plugin | President People; Hopper converted the shapes | 2.2, 19 Jan 2012 | M2 | 18 walls from the 1995 preview build | base | DDS DXT1 128x128 | 0.15 MB | [simplici7y.com/items/m2-preview-textures-plugin](https://simplici7y.com/items/m2-preview-textures-plugin/) | none stated | yes |
| 23 | Weapons In Hand - Muzzle Flash and Motion Blur | Ares Ex Machina | 1.5, 30 Mar 2014 | M2 and Infinity | coll 1 (stock-look weapons with new flashes, blur, optional magnum scope); 3 shapes patches | base, masks | DDS uncompressed ARGB, 16x16 to 512x512 | 0.6 MB | [simplici7y.com/items/weapons-in-hand-muzzle-flash-and-motion-blur](https://simplici7y.com/items/weapons-in-hand-muzzle-flash-and-motion-blur/) | none stated | yes |
| 24 | 3D Items Plugin | thedoctor45; Envy (shield models), Hopper (normals) | 1.3, 24 Feb 2012 | Infinity; a reviewer reports it works in M2 | 21 `<model>` entries: coll 7 items (20) and one on coll 6; OBJ models with PNG skins | skins | OBJ, PNG 512x512 and 256x256 | 3.2 MB | [simplici7y.com/items/3d-items-plugin](https://simplici7y.com/items/3d-items-plugin/) | none stated | yes |
| 25 | 256 Color M2 Lanscapes | Tfear | 1.1, 7 Mar 2017 | M2 | the four 256-colour landscapes as PNG, no Plugin.xml (images only) | base | PNG up to 1749x924 | 3.5 MB | [simplici7y.com/items/256-color-m2-lanscapes](https://simplici7y.com/items/256-color-m2-lanscapes/) | none stated | yes |
| 26 | Marathon Durandal Textures @ 1024x1024 | hippieman | 1.1, 4 Dec 2013 | M2 | "original textures created for Marathon Durandal" at 1024x1024; Zetren's 2016 review notes it lacks the starscapes | base | unverified | unverified | listed on [simplici7y.com/scenarios/marathon-2-durandal/?page=3](https://simplici7y.com/scenarios/marathon-2-durandal/?page=3); item page not located | unverified | no |
| 27 | m23redux Plugin | tim4i | 1.1, 15 Nov 2010 | M2 | "3x extra sprites" and item visuals | unverified | unverified | unverified | same page; item page not located | unverified | no |
| 28 | Marathon 2 XBLA texture enhancement scripts | thedoctor45 | 1.0, 1 Nov 2009 | M2 | scripts to extract the XBLA art yourself; the origin of everything above | none | MML only | small | [simplici7y.com/scenarios/marathon-2-durandal/?page=4](https://simplici7y.com/scenarios/marathon-2-durandal/?page=4) | none stated | no |
| 29 | Classic Marathon 2 (official release) | Aleph One project | 20250829 | M2 | the game; plugins bundled: Basic HUD 1.2, Enhanced HUD 1.1, Marathon 2 Stats, Marathon 2 Theme, Transparent Liquids. No HD art. | none | | 42 MB DMG | [alephone.lhowon.org/games/marathon2.html](https://alephone.lhowon.org/games/marathon2.html) | GPL engine; Bungie data | yes (mounted) |

Also on Simplici7y for M2 but not art in this sense: 'Scape Stretcher (W'rkncacnter, 2025, MML that stretches landscapes across the screen), MaraToon (Juzo-kun, 2021, a cartoon restyle of Infinity and Durandal), Radiance Shapes (President People, 2011, edited shapes that make some bitmaps glow), Kill Bob LeVitus (1996 demo sprites). The "Marathon Trilogy Upscaled" item (W'rkncacnter, 2022) is a Simplici7y pointer to the lochnits files (#15, #16).

### Infinity-only packs (for the target column and for CFP's siblings)

| Pack | Author | Version, date | Content | Size | Where |
|---|---|---|---|---|---|
| Community/Freeverse Walls MInf | herecomethej2000 | 1.1, 26 Dec 2025 | Infinity walls at 1024x1024 with 4K landscapes (Goran Svensson's sets with kaosof and The Man's bump maps, per the CFP README) | 425 MB | Simplici7y (Drive) |
| Community/Freeverse Plugin - Weapons Minf | herecomethej2000 | 2.5, 31 Aug 2025 | Infinity weapons and items | 72.9 MB | Simplici7y (Drive) |
| Goran's Texture Sets, 2048 Landscape Plugin, HD Weapons | Goran Svensson; Aleph One | release 30 Dec 2021 | Infinity's five wall sets at 512x512 DDS, landscapes, weapons | | [data-marathon-infinity release plugin-removal](https://github.com/Aleph-One-Marathon/data-marathon-infinity/releases/tag/plugin-removal) |
| Bump Maps - Sewage; Bump Maps - Jjaro (50% and 100%) | kaosof (2020); Ares Ex Machina (2011) | 1.0 | bump (`offset_image`) maps with glow masks for Infinity's sets | | Simplici7y |
| ML Super Res Monsters / Walls Infinity | treellama | 1.0, 2020 | Pixelmator ML upscales | | Simplici7y |
| Hi-Res Critters And VacBobs; Hi res Jjarro scenery | General Tacticus | 2016 | Infinity-only monsters and scenery | | Simplici7y |

Marathon 1 packs (TTEP v7, 3D scenery for M1, M1 Weapons Redux, xBR Monsters) are not usable with M2's collections and are not listed.

## 2. Notes on the packs that matter

**The official XBLA plugins (#1-#3)** are the reference layout. `HD Walls.mml`
is 120 lines of `<texture coll="17" bitmap="0" normal_image="Textures/Water/0.dds"/>`
style entries; `HD Landscapes.mml` four more. The monster and weapon MML give
every colour table its own image (`clut="1"` to `clut="8"` for the player's
eight team colours, 1,477 entries on collection 6) and set `opac_type`; 35
entries use `clut_variant="-1"`. Each sprite collection comes with a shapes
patch (`requires_opengl="true"`) that replaces the collection definition, the
bitmaps, every frame rectangle and the sequences' `pixels_to_world` scale, so
the larger renders sit correctly in the world. The DDS files are DXT5 with a
mip chain that stops one level short (the engine allows this, see 4.4). The
1,057 PNGs in HD Monsters are the frames Freeverse did not render in HD.

**CFP (#7-#10)** is the same art re-mastered from Freeverse's own higher
quality sprites (via treellama and Hippieman), AI-upscaled where needed, with
community glow maps and, since Walls 2.1, a normal map for every wall. It is
split into Walls, Scenery, Monsters and Weapons, each a plugin with its own
`Plugin.xml`. All images are PNG (2.1 of Scenery replaced the last
uncompressed DDS). Paths are `XBLA/ShapesHD/<coll>/<clut>/<bitmap>.png` with
`<bitmap>A.png` (or `a.png`) for the glow and `<bitmap>b.png` for the offset
map; the masters repository keeps the glow *masks* separate and the README
explains how Treellama's Aorta tool combines them into the glow images the
MML references. Weapons M2's MML sets `normal_bloom_shift` on 46 entries and
glow parameters on 17; Scenery sets `opac_type="1"` (fuzzy edges) everywhere
and `opac_scale`/`opac_shift` on 63 entries, which is why it fights the
Transparent Liquids plugin (section 3). Weapons and Scenery ship shapes
patches; the Scenery patch touches frames and bitmaps only. The Monsters
readme warns that the zip must be unpacked or every level takes about five
minutes to load (the engine reads plugins straight from zips, slowly).

**Zetren's 1024 walls (#6)** are the official walls at twice the resolution as
plain PNG, plus the four landscapes; no glow, no normals, no readme. The page
warns that its skyboxes tile visibly above an FOV of 88 on 16:9.

**XHD Monsters (#12)** is treellama's 2024 rescue of higher-resolution XBLA
renders Hippieman found: seven monster types at up to 640x644 DXT5. Gaps per
its readme: no purple trooper, one brown hunter frame missing, the orange
compiler generated from the purple one, no marines. Its plugin name sorts
after "HD Monsters" and "CFP Monsters", so with both enabled it wins on its
seven collections.

**ULTRA HD Weapons M2 (#13)** is #3 upscaled 4x by AI, up to 4096x4096 DXT5,
245 MB unpacked; its readme asks for every other weapon plugin to be off.

**Wall Bloom (#20)** is the one pack that adds glow to the *stock* walls:
128x128 glow images and masks for 30 bitmaps with `glow_bloom_*` values
copied from the SuperPlugin, so it composes with any base-only wall pack.

**The SuperPlugin (#11)** is the older all-in-one: the same XBLA art as
the official plugins at their native resolution (walls 512x512, as PNG), with
thedoctor45's glow work expressed as `glow_image` pointing at the *same* file
as `normal_image` plus a `glow_mask` (`<bitmap>_glow.png`) on 467 entries,
`normal_bloom_shift` on 341, and eight `offset_image` normal maps on water
walls (bitmaps 11, 14, 17, 19, 21, 24, 28, 29 of collection 17). Its page's
"bump maps" means those eight. It also carries extra monsters (F'lickta and
Yeti files, purple troopers) and a 4096x2160 moon landscape as DXT1 DDS. Its
`XBLA Support/Liquids` folder holds alternative liquid images and `Shots`
screenshots. A 2025 review reports its landscapes not working; under Metal
the DDS moon would not draw today either (4.10). Compared with CFP it is one
plugin instead of four, at half the wall resolution and without CFP's
normal maps; the two must not be enabled together (its name sorts after
"CFP ...", so it would override every CFP entry it shares).

**Pepperscapes (#17)** is the one pack that uses a `<map_patch>`: 43 TEXT
resources injected into the M2 map file (matched by checksum) give every level
its own MML that points the landscape collection at a per-level 4096x1536
image, and a `<landscapes>` element sets the aspect exponents. It is the
only pack here that needs the map-patch path and the `<landscapes>` element.

**3D Items (#24)** shows the `<model>` path: `<model coll="7" seq="0" file="..obj" type="obj" scale=".." x_rot=".." ...>` with a `<skin normal_image="...png"/>` child, 21 of them. It is the only 3D content that applies to M2.

**Shapes patches and determinism.** Every `.ShPa` in the packs was parsed
(`cldf`, `hlsh`, `llsh`, `bmap`, `ctab` records) and each patched sequence
compared with the original in `data/Scenarios/Marathon 2/Shapes.shpA`. The
only field that ever changes is `pixels_to_world` (the sprite's drawn scale);
`frames_per_view`, `ticks_per_frame`, `key_frame`, `loop_frame`, transfer
modes and sounds are identical in all 313 patched sequences. Patches also
rewrite frame rectangles (`llsh`) and bitmaps (`bmap`). So the patches are
rendering-only, which is why Aleph One marks them `requires_opengl`; the film
tests run in software mode and never load them.

## 3. Recommended baseline for development

The priority is the enemy and weapon sprites. Recommended set, in the order
the engine loads them (alphabetical by plugin name, so "CFP - Walls M2",
"CFP Monsters", "CFP Scenery", "CFP Weapons M2"; later plugins win per
attribute, see 4.1):

1. **CFP Walls M2 2.1.1** (plugin name "CFP - Walls M2"): 1024x1024 walls with
   glow and normal maps, the only M2 source of normal maps, plus 4K landscapes.
2. **CFP Monsters 2.17** ("CFP Monsters"): the complete monster set, PNG, with
   glow. Unzip it.
3. **CFP Scenery 2.6** ("CFP Scenery"): PNG scenery with glow. Requires
   Transparent Liquids off (it is already disabled in the owner's preferences).
4. **CFP Weapons M2 2.4** ("CFP Weapons M2"): weapons in hand, projectiles and
   items with glow, up to 2048x2048.
5. **XHD Monsters 0.2** ("XHD Monsters"): optional, on top of CFP Monsters for
   the seven monster types it covers at higher resolution. Compare on screen;
   it sorts after "CFP Monsters" and therefore overrides it for those
   collections.
6. **Marathon 2: Durandal Bloom** only if a base-only wall pack is used
   instead of CFP Walls; with CFP Walls it would override 30 walls' glow
   parameters (its name sorts after "CFP - Walls M2").

The SuperPlugin is the single-download alternative to the four CFP packs
(same art at the official resolution, glow through masks, eight normal maps);
never together with CFP.

Lighter alternatives: the official **HD Textures + HD Monsters + HD Weapons**
(53 MB, DXT5, everything at XBLA resolution, no glow) are the smallest complete
set and the reference the community packs are measured against; **Zetren's
1024 walls** for a lighter 1024 wall set without maps; **ULTRA HD Weapons M2**
for the sharpest guns if 245 MB of DXT5 is acceptable.

Rules and conflicts:

- Exactly one wall pack at a time. Names decide which attribute wins: "HD
  Textures 1024x1024" (Zetren) sorts after "CFP - Walls M2" and would replace
  the colour image while leaving CFP's glow and offset maps in place, a
  mismatch.
  The same applies to "M2 AI Textures", "TTEP" and "M2 Upscaled Textures".
- Exactly one weapon pack (ULTRA HD's readme says the same).
- CFP Scenery and Transparent Liquids: TL's MML sets `opac_type="3"
  opac_scale="0.5" opac_shift="0.5"` on the liquid walls and on every scenery
  bitmap in collections 22-26; loading after "CFP Scenery" it overwrites CFP's
  `opac_type="1"` and edge settings on the splash and puddle sprites. Keep TL
  off (Durandal's Real Liquids draws the surfaces itself anyway).
- Landscapes: the wall packs #1, #4, #6, #14 each carry the four landscapes;
  W'rkncacnter's and Tim Vogel's 2048x1080 sets and Pepperscapes are separate
  plugins that override them by name order. Durandal's HDR Sky applies to
  whatever landscape image is loaded.
- Durandal's own features and replacements: substitute textures bypass the
  8-bit path (`PlaceIndexImage`, `OGL_Textures.cpp:566-567`), so Marathon
  Shading, DurandalGlow's wall lights and Surface Relief do not apply to
  replaced textures today; the pack's own glow image is used instead, and its
  normal map is not (4.10).

Install location for testing: `~/Library/Application Support/Durandal/Plugins/`
(the per-user folder, `Source_Files/CSeries/cspaths.mm:47-58`); the bundle's
`Contents/Resources/DataFiles/Plugins` also works but is rebuilt by Xcode.
Zips are accepted unexpanded (`Source_Files/XML/Plugins.cpp:560-592`) but load
slowly. Enable in Preferences > Environment > Plugins; the state is stored as
`enable_plugin`/`disable_plugin` entries in the preferences file
(`Source_Files/Misc/preferences.cpp:4063-4091`).

## 4. Plugin and MML format: what the Metal loader must support

File references are to this checkout (branch `durandal/hd-assets`, 27 Sep 2026).

### 4.1 Plugins

- **Discovery** (`Source_Files/XML/Plugins.cpp:600-623`, `enumerate`): Steam
  Workshop items first, then a `Plugins` folder under each data search path
  entry (`Source_Files/shell.cpp:405-456`: the bundle's DataFiles, the folder
  holding the app, `~/Library/Application Support/Durandal`). The scan is
  recursive and also reads `Plugin.xml` inside zip files (560-592). The list
  is **sorted by plugin name** (620).
- **Plugin.xml** (`ParsePlugin`, 349-558): root `<plugin>` with `name`
  (required), `version`, `description`, `minimum_version`, `auto_enable`
  (default true), `hud_lua`, `stats_lua`, `solo_lua` (legacy attribute),
  `theme_dir`; children `<mml file>` (files sorted alphabetically before
  loading, 533), `<solo_lua file>` with `<write_access>`, `<shapes_patch file
  requires_opengl>` (463-470), `<sounds_patch file>`, `<scenario name id
  version>` (matched by `compatible()`, 71-88), `<map_patch>`.
- **Load order** (`load_mml`, 195-205; `ResetLevelScript`,
  `Source_Files/XML/XML_LevelScript.cpp:209-230`): every level start resets all
  MML and re-parses the base `MML/` and `Scripts/` folders, then each enabled
  plugin's MML in name order, then the level's own MML. Image files are freed
  and reloaded each level (`OGL_LoadTextures`,
  `Source_Files/RenderMain/OGL_Subst_Texture_Def.cpp:66-75`, called from
  `OGL_StartRun` via `load_replacement_collections`, `shapes.cpp:1493-1505`).
- **Paths**: while a plugin's MML is parsed its directory is pushed to the
  front of the search path (`Plugins.cpp:180`;
  `Source_Files/Files/FileHandler.cpp:1804-1814`), so `normal_image` paths are
  relative to the plugin root, falling back to the other data folders. A
  missing file leaves the field at its previous value and the entry is skipped
  silently.
- **Validity** (`validate`, 674-736): only the last plugin by name with a
  `hud_lua`, `stats_lua` or theme stays active; a plugin with a `solo_lua` is
  disabled entirely outside solo play (95-104).
- **Shapes patches** (`shapes.cpp:902-1015`, `load_shapes_patch`): per
  collection, records `cldf` (a 544-byte collection definition), `hlsh`
  (sequence), `llsh` (36-byte frame), `bmap` (bitmap), `ctab` (colour table),
  `endc`. Patched bitmaps get `_PATCHED_BIT` and then **refuse substitution**
  (`OGL_Textures.cpp:678`), which is how a patch can carry a bitmap that must
  not be replaced. Loaded only when OpenGL is active for `requires_opengl`
  patches (`shapes.cpp:1448`).

### 4.2 The `<opengl><texture>` element

Parsed by `parse_mml_opengl_texture`, `Source_Files/RenderMain/OGL_Subst_Texture_Def.cpp:128-212`
(dispatch: `parse_mml_opengl`, `OGL_Setup.cpp:488-541`, which handles all
`texture`/`txtr_clear` children first, then `model`/`model_clear`, `shader`,
`fog`). Fields live in `OGL_TextureOptionsBase` (`OGL_Texture_Def.h:108-154`,
defaults 150-153) and `OGL_TextureOptions` (`OGL_Subst_Texture_Def.h:43-55`).
`docs/MML.html` documents it at lines 1549-1594.

| Attribute | Type, range | Field | Meaning |
|---|---|---|---|
| `coll` | 0-31, required | key | collection |
| `bitmap` | 0-32767, required | key | **bitmap** index, not the frame; lookup maps frame to bitmap with `get_bitmap_index` (`OGL_Textures.cpp:513`) |
| `clut` | -1 to 9 | key | colour table; -1 = all (`ALL_CLUTS`); 8 and 9 are the deprecated infravision/silhouette values, remapped at 144-154 |
| `clut_variant` | -1 to 2 | key | -1 all, 0 normal, 1 infravision, 2 silhouette; the internal CLUT numbering is 0-7 normal, 8 infravision all, 9 silhouette all, 10-17 infravision per CLUT, 18-25 silhouette per CLUT (`OGL_Texture_Def.h:47-54`) |
| `normal_image` | path | `NormalColors` | the colour image (the only required image) |
| `normal_mask` | path | `NormalMask` | greyscale file whose (R+G+B)/3 becomes the alpha of the normal image; must be the same size |
| `glow_image` | path | `GlowColors` | self-luminous layer, drawn with `GlowBlend` |
| `glow_mask` | path | `GlowMask` | opacity of the glow layer |
| `offset_image` | path | `OffsetMap` | bump map: tangent-space normal in RGB, height in alpha |
| `opac_type` | 0-3 | `OpacityType` | 0 crisp (alpha test at 0.5), 1 fuzzy, 2 alpha = average of RGB, 3 alpha = max of RGB |
| `opac_scale`, `opac_shift` | float | `OpacityScale`, `OpacityShift` | alpha = alpha x scale + shift, applied to the image at load (2036-2125; for DXT1, and for types 2-3, it decompresses) |
| `normal_blend`, `glow_blend` | 0-3 | `NormalBlend`, `GlowBlend` | Crossfade, Add, Crossfade_Premult, Add_Premult (`OGL_Texture_Def.h:97-105`); documented as deprecated but still parsed |
| `normal_premultiply`, `glow_premultiply` | bool | `NormalIsPremultiplied`, `GlowIsPremultiplied` | the file has premultiplied alpha; promotes the blend to the `_Premult` variants (`OGL_Textures.h:261-262`) |
| `actual_width`, `actual_height` | short | | use only the top-left sub-rectangle of the image (small sprites padded into square DDS) |
| `type` | -1 to 4 | `Type` | 0 wall, 1 landscape, 2 inhabitant, 3 weapon in hand, 4 HUD (`OGL_Setup.h:163-171`); selects the quality preferences (max size, mipmaps); -1 ignores them |
| `normal_bloom_scale`, `normal_bloom_shift` | float, default 0, 0 | `BloomScale`, `BloomShift` | the colour image's contribution to the bloom pass |
| `glow_bloom_scale`, `glow_bloom_shift` | float, default 1, 0 | `GlowBloomScale`, `GlowBloomShift` | the glow image's contribution to the bloom pass |
| `landscape_bloom` | float, default 0.5 | `LandscapeBloom` | landscape brightness in the bloom pass |
| `minimum_glow_intensity` | float, default 1 | `MinGlowIntensity` | floor on the light level when drawing the glow layer: `clamp(intensity, glow, 1)` |
| `tile_ratio_exp` | short | `TileRatioExp` | tile a replacement wall over 2^n x 2^n world units |
| `billboard` | -1 to 1 | `Billboard` | sprite billboard axes: user preference, Y, XY |
| `void_visible` | bool | `VoidVisible` | legacy fixed-function path only |

Deprecated and ignored: `image_scale`, `x_offset`, `y_offset`, `offset_x`,
`offset_y`, `shape_width`, `shape_height`. `<txtr_clear coll="n">` (214-221)
clears one collection's options, or all without `coll`.

### 4.3 Lookup and merge rules

- Storage is one map per collection keyed by (internal CLUT, bitmap)
  (`OGL_Subst_Texture_Def.cpp:43-45`). The parser finds the existing entry or
  creates one from the defaults and then **overwrites only the attributes
  present** (179-184). Two plugins touching the same bitmap therefore merge,
  the later plugin winning attribute by attribute. This is the mechanism
  behind every conflict in section 3.
- `OGL_GetTextureOptions` (87-120) tries: the exact (CLUT, bitmap); for a
  per-CLUT infravision or silhouette entry, the all-CLUT infravision (8) or
  silhouette (9) entry; then `ALL_CLUTS` normal; then the defaults. The
  runtime chooses the CLUT set in `ModifyCLUT` (`OGL_Textures.cpp:481-492`):
  static and tinted transfer modes use silhouette, active infravision the
  infravision set.

### 4.4 Image loading

`OGL_TextureOptionsBase::Load()` (`OGL_Setup.cpp:276-394`):

1. Flags: resize to powers of two unless NPOT textures are on; load mipmaps
   if the type's far filter is above linear; keep DXTC only if S3TC is
   available (the Metal display advertises it, `DurandalGL.mm:2027-2031`).
   The maximum size is the GL maximum capped by the per-type preference
   (280-296); **a DDS without mipmaps fails when the maximum size is 0**
   (`ImageLoader_Shared.cpp:533, 541-544`), so a loader must pass a real
   maximum.
2. The normal image is required (307-318). The offset image loads **only if
   `OGL_Flag_BumpMap` (0x2000) is set in the OpenGL preferences** (321-325);
   The owner's `ogl_flags="19097"` has it off, so today no pack's normal map is
   even read. The normal mask follows (328-331), then everything is minified
   to the maximum size (333-345), then the glow image and mask (348-368).
3. **The glow is dropped unless it is exactly the normal image's size**
   (378-392).

Decoders (`ImageLoader_SDL.cpp:41-146`): DDS is tried first (49), then
SDL_image (68: PNG, JPEG via libjpeg-turbo, GIF, BMP, TGA). Non-DDS images
are not rescaled to a power of two but blitted into the top-left of a padded
surface with `VScale = original width / padded width` and `UScale = original
height / padded height` (79-87, 106-116; the names are swapped and the sprite
code relies on it). Masks must match the image size and become alpha (91-97).

DDS (`ImageLoader_Shared.cpp:349-552`, header structs `DDS.h:34-83`):
uncompressed 24 and 32-bit RGB, and FourCC `DXT1`, `DXT3`, `DXT5` (437-450);
no cube maps, volumes, DX10 header, BC4, BC5 or BC7. Mip chains must be
complete or **missing only the last level** (461-467: "XBLA textures do
that"; the official walls have 9 levels for 512x512). Top levels above the
maximum size are skipped (473-478); with mipmaps on, all levels load and a
missing tail is copied from the smallest (481-513); otherwise one level
(514-531). Without S3TC the image is decompressed to RGBA (547, `MakeRGBA`
582-616). `Minify` (131-177) drops level 0 for DDS with mips, and for plain
RGBA calls `gluScaleImage` (153-157) through real GLU even in Metal display
mode: worth checking. `PremultiplyAlpha` (618-652) is never called and has a
bug (green and blue multiplied by the updated red).

`ImageDescriptor` (`ImageLoader.h:37-125`): `Width`, `Height`, `VScale`,
`UScale`, `Pixels`, `Size`, `MipMapCount`, `Format` (RGBA8, DXTC1, DXTC3,
DXTC5), `PremultipliedAlpha`.

### 4.5 How the engine uses the images

`TextureManager::Setup` (`OGL_Textures.cpp:504-642`) takes the options, uses
the substitute if one exists, otherwise builds from the shapes file; the glow
layer exists only with a glow image; bump only for substitutes with an offset
image (569-573); the Resolution preference minifies (591-612).
`LoadSubstituteTexture` (675-780): patched bitmaps are skipped (678); walls
and sprites must be powers of two unless NPOT is on (705-759); landscapes take
U scale and offset from the aspect exponent and **drop the glow** (718-735);
sprites take their scales from the image (753-756); opacity is applied (763);
under infravision glow and bump are dropped (766-773); silhouette whitens the
image and drops the glow (774-778). Non-replaced textures still honour
`opac_*` through the colour table (`FindColorTables` 909-1000), which is how
Transparent Liquids works with no images. `PlaceTexture` (1281-1494): Metal
hands off at 1288-1295; GL uploads RGBA with generated mips or DXT with
`glCompressedTexImage2D` (1355-1444); sRGB only when wanted and not for the
interface or weapons-in-hand collections (1311-1313); wrap: walls repeat with
anisotropy, landscapes repeat in S and repeat or mirror in T, sprites clamp
(1456-1493). `GetTextureMatrix` (1595-1621): substitute walls and sprites are
rotated 90 degrees and flipped in Y; substitute landscapes scaled by
`-U_Scale` and offset by `U_Offset`. Without a bump image `RenderBump` binds a
flat 1x1 `{0x80, 0x80, 0xFF, 0x80}` (273-291).

### 4.6 Bloom and bump parameters (OpenGL shader path, for reference)

Uniform names: `OGL_Shader.cpp:53-86` (`texture0` colour, `texture1` bump,
`glow` = the light floor, `bloomScale`, `bloomShift`, `flare`,
`selfLuminosity`, landscape `scalex/scaley/offsetx/offsety/yaw/pitch`). The
bloom pass (`kGlow`) writes
`color * clamp(clamp(light, glow, 1) * bloomScale + bloomShift, 0, 1)`
fogged towards black (`Shaders/wall_bloom.frag:33-40`,
`sprite_bloom.frag:26-36`, `bump_bloom.frag:49-55`); landscapes use only
`bloomScale` = `LandscapeBloom` (`landscape_bloom.frag:22-30`). The colour
image uses `BloomScale`/`BloomShift`, the glow layer `GlowBloomScale`/`Shift`
with `U_Glow = MinGlowIntensity` (`RenderRasterize_Shader.cpp:381-384,
532-538, 710-742`). The bump shader (`bump.frag:38-54`) does a four-step
parallax on the alpha height (scale 0.010, bias -0.005) with diffuse
`0.5 + |N.V| * 0.5`, chosen only with `OGL_Flag_BumpMap`
(`RenderRasterize_Shader.cpp:489-503`).

### 4.7 Landscapes and models

- `<landscapes>` (`Source_Files/RenderOther/ViewControl.cpp:327-398`):
  `<landscape coll frame horiz_exp vert_exp vert_repeat ogl_asprat_exp azimuth
  elevation projection>` fills `LandscapeOptions` (`ViewControl.h:81-110`);
  the image itself is an ordinary `<texture>` on collections 27-30.
- `<model>` (`OGL_Model_Def.cpp:676-829`): `coll`, `seq`, `file`, `file1`,
  `file2`, `type` (obj, 3ds/max, 3dmf/qd3d/quesa, dim3), `scale`, `x_rot`,
  `y_rot`, `z_rot`, `x_shift`, `y_shift`, `z_shift`, `side`, `norm_type`,
  `norm_split`, `light_type`, `depth_type`, `force_sprite_depth`; children
  `<seq_map seq model_seq>` and `<skin>` with the texture attributes of 4.2
  minus the wall-only ones (723-801, `OGL_SkinData`). Loaders in
  `Source_Files/ModelView/`. The 3D Items plugin uses `type="obj"` with
  `scale`, rotations, `side` and `norm_type`.
- `<shader name vert frag passes>` (`OGL_Shader.cpp:128-148`) replaces GLSL
  programs and is undocumented; no pack in the table uses it, and it has no
  Metal meaning.

### 4.8 What the packs actually use

Attribute census over every MML in the downloaded packs and the CFP masters:
`coll`, `bitmap`, `normal_image` (all); `clut` (sprite packs, one entry per
colour table); `opac_type` (sprite packs and CFP); `opac_scale`/`opac_shift`
(CFP Scenery 63, official monsters 50); `clut_variant="-1"` (35, official and
XHD monsters); `type` (AI, TTEP, upscales, muzzle flash, landscapes: mostly
`type="0"` walls and `type="1"` landscapes); `glow_image` (CFP masters 1,291,
walls 36, scenery 14, weapons 17; Wall Bloom 30); `glow_mask` (Wall Bloom
30); `normal_mask` (muzzle flash 5); `offset_image` (CFP Walls 120);
`glow_bloom_scale`/`glow_bloom_shift` (wherever there is a glow),
`normal_bloom_shift` (CFP 451, weapons M2 46), `normal_bloom_scale` (8),
`minimum_glow_intensity` (CFP 26, SuperPlugin 24, Wall Bloom 15);
`glow_mask` (SuperPlugin 467, Wall Bloom 30); `<model>`/`<skin>` (3D Items
21); `<landscapes>` and `<map_patch>` (Pepperscapes). One typo in the CFP
masters and CFP Walls, `low_bloom_shift`, is ignored by the parser. Never used
by these packs: `tile_ratio_exp`, `billboard`, `actual_width`,
`actual_height`, premultiply flags, the blend attributes, `<shader>`,
`<txtr_clear>`.

Image formats in use: PNG (CFP, Zetren, Wall Bloom, Pfhor Remix, the
official landscapes), DDS DXT5 with mips (official XBLA, XHD, ULTRA HD), DDS
DXT1 (AI Textures, TTEP, W'rkncacnter's walls, M2 Preview), DDS uncompressed
RGB/ARGB (all the 2048x1080 landscapes, the muzzle-flash pack). Sizes from
16x16 sprites to 4096x4096 weapons; walls 512, 1024 or 2048x1080 landscapes.

### 4.9 What a Metal loader must therefore do

1. Key options on (collection, internal CLUT, bitmap), merge attributes with
   later MML winning, and resolve with the fallback order of 4.3.
2. Follow the load order of 4.1: base MML, plugins by name with each plugin's
   files sorted, level MML; reload per level; resolve paths against the plugin
   root.
3. Decode PNG through SDL_image and DDS in DXT1/3/5 and 24/32-bit RGB, with
   mip chains that may lack the last level, into BC1/BC2/BC3 or RGBA8 Metal
   textures (the 2D shim already maps DXT to BC, `DurandalGL.mm:1865-1892`).
4. Pad non-power-of-two images and carry `UScale`/`VScale`, honour
   `actual_width`/`actual_height`, apply masks to alpha, apply `opac_*`, and
   respect the premultiply blend promotion.
5. Apply the per-type rules: landscapes drop the glow and take the aspect
   scaling; infravision drops glow and bump; silhouette whitens; the matrices
   of `GetTextureMatrix`.
6. Read `offset_image` regardless of the OpenGL bump preference and give the
   normal map to Durandal's lighting (dynamic lights, relief, redistribution).
7. Feed `glow_image` into the glow MRT with the pack's `glow_bloom_*`,
   `normal_bloom_*` and `minimum_glow_intensity`, since Durandal's bloom comes
   from the glow image (Round 4) rather than a separate pass.
8. Apply shapes patches exactly as the GL path does (they are already loaded
   by `shapes.cpp` when OpenGL is active, which the Metal display counts as).
9. Optionally: `<model>` for the 3D Items plugin (OBJ + skin), the only model
   content for M2.

### 4.10 Durandal today

`DurandalMetal::PlaceTexture` (`Source_Files/RenderMain/DurandalMetal.mm:1464-1554`)
accepts RGBA8 only and warns once "compressed replacement textures are not
supported yet; they will not be drawn": every DXT pack in the table is
currently invisible in Metal display mode, because the shim advertises S3TC
and the images stay compressed. PNG packs (CFP, Zetren, Wall Bloom) draw. The
glow layer is ported (`RenderRasterize_Metal.cpp:585-609`, using `GlowBlend`
and `MinGlowIntensity`); bump mapping is not (523); the bloom pass is not
(268) and `BloomScale`, `GlowBloom*` and `LandscapeBloom` are never read on
the Metal path; 3D models are not drawn (806-813). Durandal's ramps, wall
glow list and relief run only for non-replaced textures
(`OGL_Textures.cpp:566-567`, 1181-1222). `CLAUDE.md` records the same:
"Not ported: bloom, bump maps, 3D models".

## 5. Towards the in-game option

What was asked for is a switch in the DURANDAL preferences that turns the
enhanced textures and sprites on. From the above:

- The packs are plugins, discovered at launch and re-read at every level, so a
  Durandal switch can work by deciding which plugins are active (the same
  `enable_plugin`/`disable_plugin` state the Environment dialog uses) and
  reloading at the next level, or by shipping the chosen packs as one
  Durandal-managed plugin set that the switch enables. Because the art cannot
  be redistributed under a clear licence, the packs stay user-installed
  downloads; the switch can only use what is in the Plugins folder, and the
  dialog should say what it found.
- Priority for the lo-res complaint: monsters and weapons first (CFP Monsters,
  CFP Weapons M2 or ULTRA HD Weapons), walls second (CFP Walls M2 with its
  normal maps), scenery and landscapes third.
- "Models" is a matter of expectation: there are none for enemies or guns in
  any Marathon pack, and Aleph One's model support (`<model>`) is used by the
  M2-compatible 3D Items plugin only. An option labelled "3D models" would be
  empty in practice; the sprites are what the community has made.
- Engine work implied, for a later round: DDS in the Metal texture path,
  `offset_image` into the lighting, the pack bloom parameters into the glow
  image, a way for Marathon Shading and relief to coexist with replacements,
  and load-time cost (CFP Monsters is 541 MB of PNG decoded at every level
  change; a cache of decoded or BC-compressed textures would keep level loads
  short).

## 6. Open questions

- Licence: none of the XBLA-derived packs carries a grant beyond the Bungie
  notice in the official plugins ("contact Bungie before using these assets
  for any commercial purpose, or any purpose not directly connected with the
  Marathon community"). The CFP repository's GPL-3.0 covers its scripts; the
  art's status is the same as the source it was made from. The AI upscales
  and TTEP are derived from Bungie's 1995 art. Durandal must not ship any of
  it; documenting and loading user-installed packs is fine.
- Hippieman's 1024 textures (#26) and m23redux (#27) exist on the M2 scenario
  listing but their item pages could not be located by slug; check by hand at
  [simplici7y.com/scenarios/marathon-2-durandal/?page=3](https://simplici7y.com/scenarios/marathon-2-durandal/?page=3).
- The `gluScaleImage` call in `Minify` under Metal display mode
  (`ImageLoader_Shared.cpp:153-157`) may need a software path.
- How the SuperPlugin's monsters compare with CFP 2.15 and XHD on screen.
- The plugin versions inside the CFP zips lag the pages (Walls 2.2 in
  Plugin.xml against 2.1.1 on the page, Monsters 2.15 against 2.17; the
  Weapons zip says 2.3 against 2.4): the archives were re-uploaded without
  the manifest being updated, or the page counts differently. Treat the page
  as the version of record.
- One CFP Monsters shapes patch changes a sequence's `last_frame_sound`
  (every other patched field is `pixels_to_world`); a sound choice only,
  outside the film gate, but worth knowing.
- Which packs to QA first, and whether ULTRA HD's 4096x4096 weapons
  are worth their 245 MB.

### Manual downloads

None required after the cap was raised to 2 GB; everything listed was fetched
and inspected except the two items whose pages could not be located (#26,
#27). Local copies total 5.1 GB unpacked. Direct
links used: the Drive files through
`https://drive.usercontent.google.com/download?id=<id>&export=download&confirm=t`
(ids: Walls M2 `1oFhk1YAXi2zpOC-cPbT7_HQ_yJCQArY1`, Monsters
`1rFokW27Aj0zsdskPkmpKfiuHxd8tLOnt`, Scenery `13oLiU_uRdoUhDFmBrdGUMbFcZKSRs6pr`,
Weapons M2 `1vwkp1oc9XHU9u-IeNscHl8nwdQo4PuDm`); the SuperPlugin from
MediaFire via the Simplici7y redirect. Local copies: `Assets/<author>/`,
checksums in `Assets/SHA256SUMS.txt`.

## 7. What was built (Round 11, 27 September 2026)

The in-game option is the ART tab of the DURANDAL dialog: HD Walls and
Sky, HD Monsters, HD Weapons and Items, HD Scenery, and Normal Maps, all
Flagship-tier features. `Source_Files/RenderMain/DurandalArt.*` classes
every installed plugin by the collections its MML replaces and switches
each pack with the category most of its entries replace (section 5's
plugin-based design; the packs stay user-installed). The four CFP packs are symlinked
from `Assets/cfp/` into `~/Library/Application Support/Durandal/Plugins`.

What the engine gained, against section 4.9:

1. Keying, merging, load order, paths, shapes patches: upstream's,
   unchanged (the Metal display counts as OpenGL, so `requires_opengl`
   patches load).
2. DDS: `DurandalMetal::PlaceTexture` uploads DXT1/3/5 as BC1/2/3 with the
   file's mip chain (the XBLA chains that stop one level short are fine);
   a DDS without a chain is decompressed and mipmapped on the GPU. The
   loader keeps DDS mip chains on the Metal display whatever the texture
   type's filter setting (4.4 item 1).
3. `offset_image` is read for Normal Maps regardless of `OGL_Flag_BumpMap`
   (4.4 item 2) and bound beside the colour texture; `wall_fragment` lights
   replacement walls from it with Surface Relief's three terms (headlight
   by how much the relief turns to the viewer, a highlight, the room's
   light as if from above), and dynamic lights see the tilted normal. The
   maps' alpha is not a height (near-constant 255 in CFP), so there is no
   parallax.
4. Bloom parameters (4.6): `pack_glow` in the wall and sprite shaders writes
   the colour image's `normal_bloom_*` share and, in the glow pass, the
   glow image's `glow_bloom_*` share (with `minimum_glow_intensity` as the
   light floor) into Durandal's glow image, so the packs' lights feed Glow,
   Bloom and EDR. `landscape_bloom` is not used (HDR Sky glows the sky
   itself).
5. Per-level decode: `OGL_LoadTextures` decodes a collection's images
   across the cores on the Metal display (L06 with the CFP set: the level
   load fell from 11.2 s to 5.4 s; 2.7 s without HD art). Resident memory
   on that level is 3.06 GB with the set loaded: the engine keeps every
   decoded image for the level and the Metal copies are shared storage. A
   decoded or BC-compressed cache is the follow-up.

Normal-map convention. The CFP maps are hand-quality (rivets, ridges) but
neither the brightness correlation nor a curl test settles their y sign:
upstream's bump shader uses `abs(dot(n, v))`, so pack authors could never
see the sign. By eye the rivets carry green on their upper edges (y up the
image, the OpenGL convention), and on screen at gain 3 that convention
darkens the far edge of a raised ceiling panel under the headlight while
the other brightens it. y-up is the default; `DURANDAL_NORMAL_MAP_Y=down`
and `DURANDAL_NORMAL_MAP_GAIN=<f>` (default 1.5) are development
overrides for film runs.

Later the same day: `<model>` is drawn (3D Pickups on the ART tab; the
3D Items plugin's 21 OBJ pickups, static models only, lit as walls and
depth tested, with an optional slow spin), and replacement sprites get a
brightness-weighted glow gain (default 2) so bolts and flashes glow as the
8-bit self-luminous colours did. Not done: premultiply promotion and
`actual_width/height` (no pack uses them; the substitute path still
honours them as before), animated (Dim3) models.

## 8. Texture cache and level entry (27 September 2026, evening)

The follow-up from section 7.5. With the Texture Cache switch (ART tab,
Flagship, Metal display) each replacement image is compressed to BC7 the
first time it loads and kept in `~/Library/Caches/Durandal/Textures`;
from then on the level maps those files instead of decoding PNGs, and the
images stay BC7 on both sides. Details in CLAUDE.md (status) and the
headers of `DurandalTextureCache` and `DurandalBC7`.

Why BC7 and why our own encoder: BC1/BC3 give four levels per block and
band the packs' soft gradients; BC7 mode 6 gives sixteen between two
RGBA endpoints at the same 8 bits per pixel as BC3, and a mode-6-only
encoder is a few hundred lines (no dependency to add). Apple Silicon
Macs support BC7 natively. Measured on the packs: walls 54 dB, sprites
40-53 dB on their opaque pixels, invisible at 4x beside the PNG; the
GPU decodes the blocks exactly as the CPU decoder does (checked).

What the level entry was really spending (L06, CFP set, warm cache):

| phase                          | before   | after   |
|--------------------------------|----------|---------|
| plugins' MML (23 files)        | 1.74 s   | 0.14 s  |
| 3D Items models (21 OBJ)       | 0.55 s   | 0.02 s  |
| shapes patches                 | 0.19 s   | 0.01 s  |
| replacement images (2994)      | 1.28 s   | 0.12 s  |
| MML start to last collection   | 3.0 s    | 0.42 s  |

The MML cost was boost's case-insensitive property tree (locale
comparisons per character and an exception per missing optional
attribute); the OBJ and shapes patch costs were a system call per byte
or two. All three fixes are in upstream files, small and commented.

Memory after 10 s of play: 4.7 GB footprint with the PNG path (the
resident figure quoted in section 7 was after macOS had compressed most
of it), 405 MB with the cache, 347 MB with no HD art at all. Frame rate
unchanged. The first visit to each environment builds its entries (about
12 s for a level with 3000 images, spread across the cores); the ART tab
shows the cache's size, and deleting the folder is always safe.

Not done: the OpenGL path never sees the cache (Metal display only);
`landscape_bloom`; opacity-hack entries (`opac_type` 2 and 3, `opac_scale`,
`opac_shift`: a few CFP walls) decode to RGBA at level load as before.

## 9. Get HD Art, the in-game button (3 October 2026)

Preferences > DURANDAL > ART > GET HD ART... does in the game what
`scripts/get-hd-art.sh` does from a terminal, for the same five packs (the
four CFP packs and 3D Items, about 1 GB): one line per pack (already
installed, waiting, finding, downloading with megabytes, checking,
unpacking, installed, or why it failed), DOWNLOAD and CLOSE (which cancels
a running fetch; packs already installed stay). Each comes from its
authors' Simplici7y page by the link they publish, so nothing is re-hosted
and the licence position in section 6 is unchanged. Nothing is
overwritten; a pack whose SHA-256 differs from the version tested installs
all the same and its line says so. It needs about three times the download
free (archive, unpacked copy, margin). New packs join the plugin list
without a restart and their art loads from the next level. Only what
macOS provides (NSURLSession, CommonCrypto, `ditto`, `unzip`).
Code: `Source_Files/Misc/DurandalFetch.*`, the dialog in
`DurandalPreferences.cpp` (`GetHDArtDialog`), `Plugins::add_directory`.
QA passed 3 October 2026 (`baseline-11`).

## 10. Marathon 1 packs (catalogue, 3 Oct 2026)

Research for the trilogy plan (T2). No pack was downloaded or unpacked. The
sources were each Simplici7y item page, a HEAD request on its
`/items/<slug>/downloads/new` redirect (which shows where the file is hosted
and how big it is), the Aleph One `data-marathon` repository on GitHub
(its git history still holds the MML of the plugins Aleph One used to
bundle), the Aleph One wiki, lochnits.com, and the Steam Workshop pages and
treellama's Steam guide for Classic Marathon. "Target" says which M1 data a
pack was made for:

- **A1 M1**: the original Marathon data that Aleph One has shipped since
  2011, the data Durandal uses.
- **M1A1**: the older community conversion of M1 to M2 format. It numbers
  its bitmaps differently, so its packs mismatch textures on the original
  data.

Sizes are the download archives, from Content-Length. The upload dates come
from the item pages; the S3 file dates are all 3 June 2023, which is when
Simplici7y moved its storage.

Marathon 1's wall collections are 2, 8, 17, 18, 19 and 24. The bundled TTEP's
MML confirms this, with 32, 8, 30, 30, 30 and 6 entries (136 textures). M1
has no landscape collections. Its starfield is wall bitmaps turned into
landscapes by MML `<landscape>` entries: coll 2 bitmap 4, coll 17 bitmap 24
and coll 18 bitmap 21. A sky pack therefore overrides three of a wall pack's
entries. M2 and Infinity landscape packs (Goran's, Tim Vogel's High-res
Landscapes) do not apply to M1.

Licence background: Bungie's M1 content has no formal licence. The
`data-marathon` Readme calls it a "non-license", and every pack below
derives from Bungie's art, whether redrawn or upscaled. Apart from the two
repacks marked "with permission", no item page states a licence.

### Marathon 1 packs

| # | Pack | Author(s) | Version, date | Target | Replaces | Format, resolution | Size | Where; download kind | Licence / permissions | Reception |
|---|---|---|---|---|---|---|---|---|---|---|
| M1-1 | TTEP v7 (Aleph One's bundled copy; plugin "TTEP", "v7.0 by Tim Vogel") | Tim Vogel (Jay Faircloth credited in treellama's guide); adapted by Aleph One | v7.0; bundled 2011-21, release 28 Dec 2021 | A1 M1 ("adapted to work with Marathon's original data files") | all walls: coll 2, 8, 17, 18, 19, 24, 136 entries, including the three starfield bitmaps (type 1, `landscape_bloom="0"`) | DDS 512x512 with mips (about 175 KB each, so DXT1); starfield 369 KB | 17.1 MB | [data-marathon release `plugin-removal`](https://github.com/Aleph-One-Marathon/data-marathon/releases/tag/plugin-removal); direct GitHub file `TTEP.v7.zip` | none stated in the plugin; the readme is Tim Vogel's 2007 install notes | the Aleph One maintainer's Steam guide lists it first; hand-remade, faithful art |
| M1-2 | Updated Starscape | Hopper; nebula pattern from webtreats | 1.0; release 28 Dec 2021 | A1 M1 | the starfield: coll 2/4, 17/24, 18/21, as landscapes (`horiz_exp 1`, `vert_repeat false`) | one `star.dds`, described as 2048x1080 | 0.26 MB | same release; direct GitHub file `Updated.Starscape.zip` | the readme points to the mysitemyway pattern licence | listed in treellama's guide |
| M1-3 | TTEP Updated Plugin (M1) @ 1024x1024 | Zetren (art by Tim Vogel) | v7.0, 18 Dec 2016 | A1 M1 ("identical content wise" to the bundled TTEP folder; Aleph One 1.2.1 and later) | as M1-1 | the TTEP originals at 1024x1024 (DDS per TTEP 7) | 60.1 MB | [simplici7y.com/items/ttep-updated-plugin-m1-1024x1024](https://simplici7y.com/items/ttep-updated-plugin-m1-1024x1024/); `downloads/new` goes to an S3 zip | none stated | 5,296 downloads, no reviews; the readme says to disable the 512 copy |
| M1-4 | TTEP 7 Marathon 1 (original) | Tim Vogel | 7.0, 6 Dec 2007 | M1A1 (installed through the Scripts folder; not a plugin) | walls (and the old backdrops) | DDS up to 1024x1024 | 81.6 MB | [simplici7y.com/items/ttep-7-marathon-1](https://simplici7y.com/items/ttep-7-marathon-1/); S3 zip | none stated | 5.0 from 5 reviews: "Phenomenal"; lava #31 and the backdrops are weak |
| M1-5 | TTEP Plugin (M1) | tim4i (repack of M1-4) | v7.0, 20 Nov 2010 | M1A1 era (made before Aleph One moved to the original data) | as M1-4 | as M1-4 | 81.6 MB | [simplici7y.com/items/ttep-plugin-m1](https://simplici7y.com/items/ttep-plugin-m1/); S3 zip | "Released under permission" | 4.0 from 1 review |
| M1-6 | Marathon Texture Renewal Project: Surfaces Module | Rock (Rockmih) | 0.5, 2 Aug 2024 | A1 M1 (made on Steam's Classic Marathon) | walls, and probably the starfield (not stated) | 4x AI upscale; format not stated (188.8 MB unpacked on the Workshop) | 46.6 MB **.7z** | [simplici7y.com/items/marathon-texture-renewal-project-surfaces-module](https://simplici7y.com/items/marathon-texture-renewal-project-surfaces-module/); S3 `.7z`; also on the Steam Workshop | none stated | no Simplici7y reviews; Workshop: "very nice and faithful", but players report missing textures; author: "some textures still need some work" |
| M1-7 | Marathon Texture Renewal Project: Monsters Module | Rock | 0.5, 2 Aug 2024 | A1 M1 | monster sprites (the BoBs reworked by hand); coverage not stated | 4x AI upscale ("pixel art models"); 265.4 MB unpacked on the Workshop | 43.5 MB **.7z** | [simplici7y.com/items/marathon-texture-renewal-project-monsters-module](https://simplici7y.com/items/marathon-texture-renewal-project-monsters-module/); S3 `.7z` | none stated | 5.0 from 1 review (herecomethej2000, the CFP maintainer): cleaner than W'rkncacnter's upscale, but stray PNGs were left in folder 9/3; Workshop: "much better than the XBR upscale", or "very colourful … childish" |
| M1-8 | Marathon Texture Renewal Project: Weapons Module | Rock | 0.5, 2 Aug 2024 | A1 M1 | weapons in hand | 4x AI upscale; 8.4 MB unpacked | 1.8 MB **.7z** | [simplici7y.com/items/marathon-texture-renewal-project-weapons-module](https://simplici7y.com/items/marathon-texture-renewal-project-weapons-module/); S3 `.7z` | none stated | Workshop: "cartoon-y"; the author agrees and points players to a more faithful weapons pack |
| M1-9 | xBR Monsters for M1 | Flippant Sol (help from General Tacticus, treellama) | 1.3, 20 Nov 2017 | A1 M1 (indexed from M1's original shapes) | every monster and the marines, every colour table: 2,602 textures | xBR 4x per axis ("16x the original resolution"); format not stated | 47.1 MB | [simplici7y.com/items/xbr-monsters-for-m1](https://simplici7y.com/items/xbr-monsters-for-m1/); S3 zip | none stated | 5.0 from 2 reviews: "Excellent quality"; sprites "no longer stick out" beside HD walls |
| M1-10 | Tacticus' M1 Weapons Redux | General Tacticus | 1.1, 13 Jul 2017 | A1 M1 (thedoctor45: does not work with M1A1, hence M1-14) | weapons in hand, projectiles, most items, detonations, bullet impacts | HD sprites, 3D-rendered style; format not stated | 14.1 MB | [simplici7y.com/items/tacticus-m1-weapons-redux-2](https://simplici7y.com/items/tacticus-m1-weapons-redux-2/); S3 zip; also on the Workshop (10.6 MB) | none stated | 5.0 from 2 reviews ("6 stars actually"); in treellama's guide; Workshop note: OpenGL renderer only |
| M1-11 | Upscaled Weapons for M1 | Flippant Sol | 1.0, 5 Jul 2018 | A1 M1 | weapons in hand | xBR-style upscale (author suggests linear filtering) | 1.4 MB | [simplici7y.com/items/upscaled-weapons-for-m1-2](https://simplici7y.com/items/upscaled-weapons-for-m1-2/); S3 zip | none stated | no reviews |
| M1-12 | 3D scenery for M1 | General Tacticus | 1.2, 16 Nov 2017 (first 2016) | A1 M1 | the scenery sprites, as 3D models (escape pod, Pfhor egg and others) | models (OBJ presumably) plus skins | 4.1 MB | [simplici7y.com/items/3d-scenery-for-m1](https://simplici7y.com/items/3d-scenery-for-m1/); S3 zip | none stated | 5.0 from 4 reviews; author: "make sure bloom is turned off", and some errors can't be avoided |
| M1-13 | HD M1A1 Weapons in Hand (beta) | General Tacticus | 1.2, 2013 | M1A1 | weapons in hand, 3D weapon items | rendered sprites | 9.4 MB | [simplici7y.com/items/hd-m1a1-weapons-in-hand-alpha-version](https://simplici7y.com/items/hd-m1a1-weapons-in-hand-alpha-version/); S3 zip | none stated | 3.4 from 7 reviews: an assertion failure in shapes.cpp, wrong sizes, multiplayer desync; replaced by M1-10 |
| M1-14 | M1A1 Compatible HD Weapon Pack | General Tacticus, port by thedoctor45 | 1.0, 15 Jul 2017 | M1A1 only ("will not work with current versions of aleph one, using the original files") | as M1-10 | as M1-10 | 15.2 MB | [simplici7y.com/items/m1a1-compatible-hd-weapon-pack](https://simplici7y.com/items/m1a1-compatible-hd-weapon-pack/); S3 zip | none stated | not for Durandal |
| M1-15 | Marathon Trilogy Upscaled (M1 part: Textures 1.1, Monsters 1.1, Scenery 1.1, Weapons 1.1) | W'rkncacnter | Textures 25 Sep 2022, others 6 Aug 2022 | A1 M1 (in treellama's guide) | walls; monsters; scenery; weapons in hand | Gigapixel AI upscales (the M2 set was DDS) | **.7z**: Textures 12.8 MB, Monsters 46.1 MB, Scenery 0.8 MB, Weapons 1.8 MB | [simplici7y.com/items/marathon-1-upscaled-textures](https://simplici7y.com/items/marathon-1-upscaled-textures/) (`downloads/new` goes to the [lochnits.com/marathon/upscales](https://lochnits.com/marathon/upscales/) page, not a file); files at `lochnits.com/marathon/upscales/M1_Upscaled_<Part>_v1.1.7z` | none stated | 3.5 from 2 reviews: the sprite cropping is "terrible"; Rock's packs were made to improve on these |
| M1-16 | M1A1 Animated Textures for M1 | President People; animation by the M1A1 team | 1.0, 20 Feb 2025 | A1 M1 ("back full circle to classic M1") | animated wall textures (from M1A1), recoloured to M1's palette; the originals are included | not stated | 0.5 MB | [simplici7y.com/items/m1a1-animated-textures-for-m1](https://simplici7y.com/items/m1a1-animated-textures-for-m1/); **Dropbox** (`dl=0`: change to `dl=1` for the file) | credits Bungie and the M1A1 team; no licence | 3,202 downloads, no reviews |
| M1-17 | M1A1: StarScape DDS | M1 Map Dude (from Phil Demkeston's) | 1.0, 17 Aug 2010 | M1A1 (works on A1 M1 if renamed by hand into TTEP) | starfield | DDS 2048x1080 | 0.2 MB | [simplici7y.com/items/m1a1-starscape-dds](https://simplici7y.com/items/m1a1-starscape-dds/); S3 zip | none stated | 4.0 from 2 reviews; a bare file, not a plugin |
| M1-18 | Marathon over Tau Ceti E-I Landscape Texture (4096x1536 and 8192x3072 editions) | liacrow | 3.0, 17 Sep 2017 | A1 M1 | starfield, rebuilt as a Celestia render of the Tau Ceti sky; the Pfhor ship scenery becomes the unused slave transport sprite; Marathon sprite touched up | 4096x1536 or 8192x3072 | 2.5 MB / 7.7 MB | [4096 edition](https://simplici7y.com/items/marathon-over-tau-ceti-e-i-landscape-texture-2048/), [8192 edition](https://simplici7y.com/items/marathon-over-tau-ceti-e-i-landscape-texture/); S3 zips | none stated | 4.0 and 4.5: you see two Pfhor ships at once; an artistic reinterpretation, not a faithful sky |
| M1-19 | HD Scenery for M1A1 / HDscenery plugin | Visciom; repack by tim4i ("released under permission") | 1.0, 2007 / 2010 | M1A1 | a few scenery objects (barrels, torn paper) | not stated | 0.6 MB | [hd-scenery-for-m1a1](https://simplici7y.com/items/hd-scenery-for-m1a1/), [hdscenery-v1-0-m1-2](https://simplici7y.com/items/hdscenery-v1-0-m1-2/); S3 zips | as stated | 3.5: on official M1 the drones cycle through scenery textures |
| M1-20 | m1redux Plugin / Alpha's Redux | tim4i (sprites partly Visciom's); patch by 3371-Alpha | 1.1, 2010 / 2.2, 2016 | m1redux: M1A1; Alpha's Redux: A1 M1 | weapons in hand, ammo and item pickups, rockets, grenades | touched-up sprites | 2.1 MB / 0.8 MB | [m1redux-v1-0](https://simplici7y.com/items/m1redux-v1-0/), [alpha-s-redux-4](https://simplici7y.com/items/alpha-s-redux-4/); S3 zips | Alpha's Redux: "I did NOT get permission from tim4i" | m1redux mismatches ammo textures on official M1; **avoid Alpha's Redux** (no permission) |

Searched and not relevant to M1: treellama's ML Super Res plugins
(Infinity only; he has no M1 set), Goran's and Tim Vogel's landscape packs
(M2 and Infinity), PerseusSpartacus's HD Scenery (M2), the Weapon
Enhancement Pack (M2-era monsters), Yuge Definition Textures (a joke pack
for Mararthon: Yuge) and James Webb Telescope (listed under Marathon, but
the archive is named `Marathon_Infinity_James_Webb.zip`; not checked). The
3D Items plugin is Infinity's item collections and does not fit M1. There
are no hi-res M1 packs on GitHub beyond the Aleph One release (M1-1, M1-2).
Steam Workshop copies (Rock's three modules, Tacticus's weapons, the
"Classic Marathon Remix" collection) need Steam to download, so the button
cannot use them.

### Overlaps and conflicts

Use one pack from each group:

- **Walls:** M1-1, M1-3, M1-4, M1-5, M1-6 and the M1-15 Textures. M1-3
  supersedes M1-1, which its readme says to disable.
- **Starfield:** M1-2, M1-17 and M1-18. Each overrides three of the wall
  pack's entries, so it has to load after the wall pack. Aleph One loads
  plugins alphabetically, and "Updated Starscape" sorts after "TTEP".
  Durandal's own category switching must preserve that order.
- **Monsters:** M1-7, M1-9 and the M1-15 Monsters.
- **Weapons in hand:** M1-8, M1-10, M1-11, M1-13, M1-20 and the M1-15
  Weapons. M1-10 also replaces items and projectiles, so it overlaps the
  item half of M1-20.
- **Scenery:** M1-12 (models), the M1-15 Scenery and M1-19. M1-18 also
  swaps one scenery sprite.
- **Animated textures:** M1-16 works on the same wall bitmaps as the wall
  packs. Whether its frames show with TTEP on, or are replaced by TTEP's
  still image, needs a look in game.

### Recommendation

| Category | Pick | Why | Size |
|---|---|---|---|
| Walls | **TTEP 1024 (M1-3)**; fallback the GitHub TTEP 512 (M1-1) | Hand-remade, faithful, well reviewed for nearly 20 years. The 512 copy is what Aleph One shipped and what its maintainer recommends today, from a stable GitHub URL. The 1024 copy is the same set at the size Durandal deserves. Rock's upscale (M1-6) is v0.5, has reported missing textures, and is unfinished by its author's own account. | 60.1 MB (or 17.1) |
| Starfield | **Updated Starscape (M1-2)** | Official release, tiny, loads over TTEP's sky. Tau Ceti (M1-18) is an optional "lore" sky, not faithful. | 0.26 MB |
| Monsters | **MTRP Monsters (M1-7)**, or xBR Monsters (M1-9) after a side-by-side look | Rock's set is the newest and the CFP maintainer rates it cleaner, but it is v0.5 and some find it cartoonish. xBR is complete (every colour table and the marines) and well liked. The trilogy plan already says "pick after a look". | 43.5 MB (or 47.1) |
| Weapons in hand, items, projectiles | **Tacticus' M1 Weapons Redux (M1-10)** | The only complete, faithful set made for A1 M1 data. In the maintainer's guide; Rock himself points players to it. | 14.1 MB |
| Scenery | **3D scenery for M1 (M1-12)** | The only real HD scenery for A1 M1. Durandal's Metal model path (static OBJ) should draw it, but DurandalArt classes `<model>` plugins as "3D Pickups" and would need an M1 scenery class. Fallback: W'rkncacnter's M1 Scenery (0.8 MB .7z). | 4.1 MB |
| Optional | M1A1 Animated Textures (M1-16) | Small, current, made for classic M1. Check how it combines with TTEP first. | 0.5 MB |

**The recommended set totals about 122 MB:** TTEP 1024, Updated Starscape,
MTRP Monsters, Tacticus weapons and 3D scenery. It is about 126 MB with
xBR Monsters instead, or about 79 MB with the 512 TTEP. That is far
smaller than M2's 1.0 GB, because M1's packs carry no normal maps, no glow
maps and no 4K landscapes.

### What the in-game fetch needs for M1 (compare section 9)

- **Hosts.** Six of the seven packs above are direct files:
  - Simplici7y's `downloads/new` redirects straight to a file on
    `simplici7y.s3.amazonaws.com` (no Google Drive step).
  - The two Aleph One packs are GitHub release assets, which can be used
    directly:
    - `https://github.com/Aleph-One-Marathon/data-marathon/releases/download/plugin-removal/TTEP.v7.zip`
    - `https://github.com/Aleph-One-Marathon/data-marathon/releases/download/plugin-removal/Updated.Starscape.zip`

  The exceptions:
  - M1-16 is on Dropbox: rewrite `dl=0` to `dl=1`. The file comes with
    `filename=unspecified`.
  - The M1-15 Simplici7y item redirects to an HTML page; take the lochnits
    file URLs directly.
- **7z.** Rock's three modules and all of W'rkncacnter's files are `.7z`,
  which `unzip` and `ditto` cannot open. macOS's own `/usr/bin/tar`
  (bsdtar 3.5.3, libarchive 3.7.4 with liblzma) reads 7z: checked here on
  an LZMA2 test archive with `tar -xf` and `tar -tf`. So `unzip -tq` and
  `ditto -x -k` would become `tar -tf` and `tar -xf` for those packs,
  still using only what macOS provides. Not yet tried on Rock's actual
  archives: their compression method is unknown until one is fetched.
- **Folder names.** None of the M1 archives has been unpacked here, so the
  folder holding `Plugin.xml`, the MML, the collections targeted and the
  image formats (beyond TTEP and Starscape, read from git) all need the
  same inspection the M2 packs had before the list is fixed. In particular:
  - whether MTRP and xBR use shapes patches;
  - whether Tacticus's pack includes a shapes patch (its sprites are larger,
    so it probably does);
  - which collections 3D scenery's models replace.

### What was chosen and built (3 Oct 2026)

- **Fetched and inspected.** TTEP 1024, Updated Starscape, both monster
  sets, M1 Weapons Redux and 3D scenery for M1 were fetched and unpacked
  (in `Assets/cfp-m1`). What they replace, from their MML: TTEP all six
  wall sets and the starfield landscapes; the starfield three bitmaps as
  landscapes (`vert_repeat` false); xBR and Texture Renewal the same 2,495
  textures over collections 3, 4, 5, 6, 9, 12, 14, 15, 16, 21, 22, 26, 27
  and 29 (no shapes patches); the weapons pack collections 1, 4, 7 and the
  monsters' projectiles, with a shapes patch; 3D scenery 13 models on 7, 23
  and 25. xBR's archive carries read-only folders (the fetch now makes
  unpacked folders writable); Texture Renewal is a 7z archive that macOS's
  `tar` reads.
- **Monsters.** The owner chose Texture Renewal over xBR after a
  side-by-side in Rampant (L16 Hunters, L1 Fighters).
- **Marathon's list** in Get HD Art and `GAME=m1 scripts/get-hd-art.sh`:
  TTEP 1024, Updated Starscape (direct GitHub link), Texture Renewal
  Monsters, M1 Weapons Redux, 3D Scenery M1, about 123 MB. The in-game
  path was checked with the harness (`DURANDAL_FETCH_GAME=m1`): all five
  installed, checksums matching.
- **Classing.** `DurandalScenario::CollectionHolds` gives Marathon's
  collections (walls 2, 8, 17-19, 24; monsters 3, 5, 6, 9, 12, 14-16, 21,
  22, 26, 27, 29; scenery 23, 25, held as objects). 3D scenery's models are
  classed with the 3D Pickups switch, as model packs are.

## 11. Infinity's packs (3 Oct 2026)

- **The set:** CFP Walls MInf 1.1 (`communityfreeverse-walls-minf`, 425 MB,
  Google Drive; 155 textures over collections 17-21 and the four 4K
  landscapes 27-30), CFP Weapons Minf 2.4 (`community-freeverse-plugin-weapons`,
  the item's page title says "Weapons Minf"; 73 MB, Google Drive), and the
  CFP Monsters, CFP Scenery and 3D Items packs Marathon 2 uses (none
  restricts itself to one scenario). Checksums of the versions tested are
  in `DurandalFetch.mm` and `get-hd-art.sh`. Fetched into `Assets/cfp-inf`
  and linked, with the shared packs from `Assets/cfp`, into
  `~/Library/Application Support/Durandal Infinity/Plugins`.
- **Checked** in Rampant on the four demo films: walls, landscapes,
  weapons and monsters replaced; level entry builds the texture cache on
  the first visit (frames of 1.3-1.9 s), as for Marathon 2.
